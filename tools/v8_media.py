"""Read owner-supplied media without extracting or distributing game assets.

The JSON inventory is evidence from the input itself. Container candidates are
reported separately from validated chunk boundaries; this does not convert an
arena or execute any donor code.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile
from collections import Counter

MAX_ROM_BYTES = 64 * 1024 * 1024
TERRAIN_DIRECTORY = 0x62920


class PsxVolume:
    """Bounded metadata reader for a single data-track BIN or cooked ISO.

    The runtime uses its full ISOReader for CUE and CHD. This analysis tool
    accepts the data track directly and never mounts or alters a live game.
    """
    def __init__(self, path: Path):
        self.start_sector = 0
        count = None
        if path.suffix.lower() == '.cue':
            from import_original_music import cue_tracks
            track = cue_tracks(path)[0]
            if track['audio']:
                raise ValueError('The first disc track must contain data')
            path, self.start_sector, count = track['path'], track['start'], track['count']
        self.path = path
        with path.open("rb") as stream:
            stream.seek(self.start_sector * 2352)
            prefix = stream.read(24)
        self.raw = prefix[:12] == b"\x00" + b"\xff" * 10 + b"\x00"
        self.stride = 2352 if self.raw else 2048
        if path.stat().st_size % self.stride:
            raise ValueError("Image is not sector aligned")
        self.sectors = count if count is not None else path.stat().st_size // self.stride
        pvd = self.read_sector(16)
        if pvd[:7] != b"\x01CD001\x01":
            raise ValueError("Missing ISO9660 volume descriptor")
        declared = int.from_bytes(pvd[80:84], "little")
        if declared != int.from_bytes(pvd[84:88], "big") or not 16 < self.sectors <= declared:
            raise ValueError("Invalid ISO volume size")
        if int.from_bytes(pvd[128:130], "little") != 2048:
            raise ValueError("Unsupported logical sector size")
        self.root_lba = int.from_bytes(pvd[158:162], "little")
        self.root_size = int.from_bytes(pvd[166:170], "little")

    def read_sector(self, lba: int) -> bytes:
        with self.path.open("rb", buffering=1024*1024) as stream:
            stream.seek((self.start_sector + lba) * self.stride)
            data = stream.read(self.stride)
        if len(data) != self.stride:
            raise ValueError("Truncated disc sector")
        if self.raw:
            if data[15] not in (1, 2):
                raise ValueError("Non-data sector in volume")
            start = 16 if data[15] == 1 else 24
            data = data[start:start + 2048]
        return data

    def read(self, lba: int, size: int) -> bytes:
        if size < 0 or lba < 0 or lba + (size + 2047) // 2048 > self.sectors:
            raise ValueError("File exceeds ISO volume")
        sectors = (size + 2047) // 2048
        result = bytearray()
        with self.path.open("rb", buffering=1024*1024) as stream:
            stream.seek((self.start_sector + lba) * self.stride)
            for _ in range(sectors):
                data = stream.read(self.stride)
                if len(data) != self.stride:
                    raise ValueError("Truncated file sector")
                if self.raw:
                    if data[15] not in (1, 2):
                        raise ValueError("Non-data sector in file")
                    start = 16 if data[15] == 1 else 24
                    data = data[start:start + 2048]
                result.extend(data)
        return bytes(result[:size])

    def files(self, lba: int | None = None, size: int | None = None,
              prefix: str = "", seen: frozenset = frozenset()) -> list[dict]:
        if lba is None:
            lba, size = self.root_lba, self.root_size
        if lba in seen or len(seen) >= 16:
            raise ValueError("Cyclic or excessive directory nesting")
        seen = seen | {lba}
        data = self.read(lba, size)
        entries = []
        at = 0
        while at < len(data):
            length = data[at]
            if not length:
                at = (at // 2048 + 1) * 2048
                continue
            if length < 34 or at + length > len(data):
                raise ValueError("Truncated directory record")
            record = data[at:at + length]
            name_size = record[32]
            if 33 + name_size > length:
                raise ValueError("Directory name exceeds record")
            name = record[33:33 + name_size]
            if name not in (b"\0", b"\1"):
                filename = name.decode("ascii").split(";", 1)[0]
                entry_lba = int.from_bytes(record[2:6], "little")
                entry_size = int.from_bytes(record[10:14], "little")
                if record[25] & 2:
                    entries.extend(self.files(entry_lba, entry_size, prefix + filename + "/", seen))
                else:
                    entries.append({"name": prefix + filename, "lba": entry_lba, "size": entry_size})
            at += length
        return entries

    def digest(self) -> str:
        digest = hashlib.sha256()
        with self.path.open("rb", buffering=1024*1024) as stream:
            stream.seek(self.start_sector * self.stride)
            for _ in range(self.sectors):
                data = stream.read(self.stride)
                if len(data) != self.stride:
                    raise ValueError("Truncated disc during identity read")
                if self.raw:
                    if data[15] not in (1, 2):
                        raise ValueError("Non-data sector during identity read")
                    start = 16 if data[15] == 1 else 24
                    data = data[start:start + 2048]
                digest.update(data)
        return digest.hexdigest()


def psx_inventory(path: Path, arena: str | None = None) -> dict:
    volume = PsxVolume(path)
    files = volume.files()
    arenas = []
    for entry in files:
        if not entry["name"].endswith(".EXP") or (arena and Path(entry["name"]).stem != arena.upper()):
            continue
        data = volume.read(entry["lba"], entry["size"])
        if data[:4] == b"LZSS":
            data = decode_lzss(data)
        arenas.append({**entry, "decoded_size": len(data),
                       "decoded_sha256": hashlib.sha256(data).hexdigest(),
                       "form_type": data[8:12].decode("ascii"), "chunks": form_chunks(data)})
    return {"kind": "psx-disc", "size": volume.sectors * 2048,
            "sha256": volume.digest(), "files": files, "arenas": arenas}


def terrain_files(data: bytes) -> list[dict]:
    """The retail ROM's 20-byte directory records, verified before use."""
    records = []
    offset = TERRAIN_DIRECTORY
    if data[offset:offset + 12] != b"TERRAIN " + bytes(4):
        raise ValueError("Terrain directory guard mismatch")
    offset += 20
    while offset + 20 <= len(data):
        name = data[offset:offset + 12].decode("ascii").rstrip(" \x00")
        if not name.endswith((".DLL", ".EXP")):
            break
        address, size = struct.unpack_from(">II", data, offset + 12)
        if address >> 24 != 0xB0 or (address & 0xFFFFFF) + size > len(data):
            raise ValueError("Arena directory record points outside ROM")
        records.append({"name": name, "offset": address & 0xFFFFFF, "size": size})
        offset += 20
    return records


def decode_lzss(data: bytes) -> bytes:
    """Derived from retail N64 instructions at 0x80132690..0x8013281c.

    MSB-first flags, 1=literal; LE16 match holds an absolute 11-bit ring
    index and five-bit length minus two. Both ring cursors start at zero.
    Reject reads of unwritten history so output never depends on a prior file.
    """
    if data[:4] != b"LZSS" or len(data) < 8:
        raise ValueError("Missing LZSS header")
    size = int.from_bytes(data[4:8], "little")
    if not 0 < size <= MAX_ROM_BYTES:
        raise ValueError("Invalid LZSS output size")
    ring = bytearray(2048)
    written = bytearray(2048)
    output = bytearray()
    position = 8
    cursor = 0
    while len(output) < size:
        if position >= len(data):
            raise ValueError("Truncated LZSS flags")
        flags = data[position]
        position += 1
        for bit in range(7, -1, -1):
            if len(output) == size:
                break
            if flags & (1 << bit):
                if position >= len(data):
                    raise ValueError("Truncated LZSS literal")
                values = [data[position]]
                position += 1
            else:
                if position + 2 > len(data):
                    raise ValueError("Truncated LZSS match")
                match = int.from_bytes(data[position:position + 2], "little")
                position += 2
                count, source = (match & 31) + 2, match >> 5
                if len(output) + count > size:
                    raise ValueError("LZSS match exceeds output size")
                # Copy incrementally: overlapping matches can read bytes they
                # just emitted, including across the ring wrap.
                for _ in range(count):
                    if not written[source]:
                        raise ValueError("LZSS references unwritten history")
                    value = ring[source]
                    source = (source + 1) & 2047
                    output.append(value)
                    ring[cursor] = value
                    written[cursor] = 1
                    cursor = (cursor + 1) & 2047
                continue
            value = values[0]
            output.append(value)
            ring[cursor] = value
            written[cursor] = 1
            cursor = (cursor + 1) & 2047
    if position != len(data):
        raise ValueError("Trailing bytes after LZSS stream")
    return bytes(output)


def form_chunks(data: bytes) -> list[dict]:
    if data[:4] != b"FORM" or len(data) < 12:
        raise ValueError("Missing FORM header")
    if int.from_bytes(data[4:8], "big") + 8 != len(data):
        raise ValueError("FORM length does not match decoded file")
    chunks = []
    offset = 12
    while offset < len(data):
        if offset + 8 > len(data):
            raise ValueError("Truncated chunk header")
        size = int.from_bytes(data[offset + 4:offset + 8], "big")
        if offset + 8 + size > len(data):
            raise ValueError("Chunk exceeds FORM bounds")
        tag = data[offset:offset + 4].decode("ascii")
        record = {"tag": tag, "offset": offset, "size": size}
        if tag in ("TITL", "TEXT"):
            record["text"] = data[offset + 8:offset + 8 + size].rstrip(b"\x00").decode("ascii")
        chunks.append(record)
        offset += 8 + size + (size & 1)
    if offset != len(data):
        raise ValueError("Invalid final chunk padding")
    return chunks


def read_n64(path: Path) -> bytes:
    if path.suffix.lower() == ".zip":
        with zipfile.ZipFile(path) as archive:
            entries = [i for i in archive.infolist()
                       if Path(i.filename).suffix.lower() in (".z64", ".v64", ".n64")]
            if len(entries) != 1 or entries[0].file_size > MAX_ROM_BYTES:
                raise ValueError("ZIP must contain exactly one N64 ROM of at most 64 MiB")
            data = archive.read(entries[0])
    else:
        if path.stat().st_size > MAX_ROM_BYTES:
            raise ValueError("N64 ROM exceeds 64 MiB")
        data = path.read_bytes()
    if len(data) < 64 or len(data) % 4:
        raise ValueError("N64 ROM is truncated or not word aligned")
    magic = data[:4]
    if magic == bytes.fromhex("37804012"):
        data = bytes(v for i in range(0, len(data), 2) for v in (data[i + 1], data[i]))
    elif magic == bytes.fromhex("40123780"):
        data = bytes(v for i in range(0, len(data), 4) for v in data[i:i + 4][::-1])
    elif magic != bytes.fromhex("80371240"):
        raise ValueError("Unrecognized N64 ROM byte order")
    return data


def n64_inventory(data: bytes) -> dict:
    forms = []
    start = 0
    while (start := data.find(b"FORM", start)) >= 0:
        sizes = {order: int.from_bytes(data[start + 4:start + 8], order)
                 for order in ("little", "big")}
        forms.append({"offset": start, "length_candidates": sizes,
                      "type_hex": data[start + 8:start + 12].hex()})
        start += 4
    strings = [{"offset": m.start(), "text": m.group().decode("ascii")}
               for m in re.finditer(rb"[ -~]{6,}", data)
               if re.search(rb"(?i)(terrain|arena|level|sand|ski|canyon|oil|farm|wild|secret|castle|moon|island|\.dll|\.frm)", m.group())]
    arena_files = terrain_files(data)
    arenas = []
    for entry in arena_files:
        if entry["name"].endswith(".EXP"):
            decoded = decode_lzss(data[entry["offset"]:entry["offset"] + entry["size"]])
            chunks = form_chunks(decoded)
            arenas.append({**entry, "decoded_size": len(decoded),
                           "decoded_sha256": hashlib.sha256(decoded).hexdigest(),
                           "form_type": decoded[8:12].decode("ascii"), "chunks": chunks})
    return {
        "kind": "n64-rom", "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "title": data[32:52].decode("ascii", errors="replace").strip(),
        "cartridge_id": data[59:63].decode("ascii", errors="replace"),
        "entry_pc": f"0x{struct.unpack_from('>I', data, 8)[0]:08x}",
        "form_candidates": forms, "strings": strings,
        "arena_files": arena_files, "arenas": arenas,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--psx", action="store_true", help="Inspect a PSX data-track BIN or ISO")
    parser.add_argument("--arena", help="Report one arena, for example DREAMLND")
    parser.add_argument("--summary", action="store_true", help="Summarize chunk counts")
    parser.add_argument("--prepare-rom", type=Path, help="Normalize a ROM outside source repositories")
    args = parser.parse_args()
    if args.psx:
        if args.prepare_rom:
            parser.error("--prepare-rom applies only to N64 media")
        result = psx_inventory(args.rom, args.arena)
        if args.summary:
            result.pop("files")
            for arena in result["arenas"]:
                arena["title"] = next((c["text"] for c in arena["chunks"] if c["tag"] == "TITL"), "")
                arena["chunk_counts"] = dict(Counter(c["tag"] for c in arena.pop("chunks")))
        print(json.dumps(result, indent=2))
        return
    rom = read_n64(args.rom)
    result = n64_inventory(rom)
    if args.prepare_rom:
        output = args.prepare_rom.resolve()
        if any((parent / ".git").exists() for parent in output.parents):
            parser.error("Donor ROM output must be outside source repositories")
        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists():
            if output.read_bytes() != rom:
                parser.error("Output already exists with different bytes")
        else:
            with output.open("xb") as stream:
                stream.write(rom)
    if args.arena:
        result = {k: result[k] for k in ("kind", "size", "sha256", "title", "entry_pc")} | {
            "arenas": [a for a in result["arenas"] if a["name"] == args.arena.upper() + ".EXP"]}
        if not result["arenas"]:
            parser.error("Arena not present in verified directory")
    if args.summary:
        result.pop("strings", None)
        result.pop("form_candidates", None)
        for arena in result["arenas"]:
            arena["title"] = next(c["text"] for c in arena["chunks"] if c["tag"] == "TITL")
            arena["chunk_counts"] = dict(Counter(c["tag"] for c in arena.pop("chunks")))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
