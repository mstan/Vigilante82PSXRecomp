"""Reuse the reference's N64 Dreamland -> native V8:2 asset pipeline.

Produces external arena/selector data from verified owner media. This is not
a playable mod until the converted overlay and runtime arena registry connect.
"""
from __future__ import annotations
import argparse
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile

from media_inputs import external_output, verified_volume, read_file

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "third_party"), str(TOOLS / "third_party/combined")]
import port_dreamland_assets as upstream
import build_v8_dreamland_shell_assets as shell
from v8_n64_level import V8N64Rom, convert_arena, overlay_exports, psx_executable_exports
from vigilante8_vehicle_tools import iff
import v82_arena_registry as arenas
import v82_native_selector_table as selector
import dreamland_overlay as overlay
import dreamland_menu
import build_v8_dreamland_sound_bank as sound_bank

ROM_SIZE = 8388608
ROM_SHA256 = "fd75b40f561049b70b7e0bc652584062061fd17cf23e518700f5dcc5d4938799"
SOUND_IDS = (*sound_bank.SOURCE_IDS, 0x8D, 0x8E, 0x97, 0xA6)


def sequel_sound_bank(rom: bytes, source: bytes, encoder: Path) -> tuple[bytes, dict]:
    entries, payload = sound_bank.parse_bank(source)
    if len(entries) != 79:
        raise ValueError("Expected the sequel's 79-entry shared sound bank")
    result_entries = list(entries)
    result_payload = bytearray(payload)
    report = []
    for source_id in SOUND_IDS:
        metadata = sound_bank.sample_metadata(rom, source_id)
        from dreamland_audio import resident_sample
        encoded, pitch, quality = resident_sample(sound_bank.decode_n64_sample(rom, source_id), metadata["loop_start"], metadata["spu_pitch"], encoder)
        metadata["original_spu_pitch"] = metadata["spu_pitch"]
        metadata["spu_pitch"] = pitch
        report.append(dict(source_id=source_id, native_id=len(result_entries), bytes=len(encoded), **metadata, **quality))
        result_entries.append((len(result_payload)//8, int(metadata["spu_pitch"])))
        result_payload.extend(encoded)
    if len(result_payload)//8 > 0xFFFF:
        raise ValueError("Extended sound bank exceeds the native size field")
    result = struct.pack("<HH", len(result_entries), len(result_payload)//8)
    result += b"".join(struct.pack("<HH", *entry) for entry in result_entries) + result_payload
    checked_entries, checked_payload = sound_bank.parse_bank(result)
    if checked_entries[:79] != entries or checked_payload[:len(payload)] != payload:
        raise AssertionError("Original sequel sounds changed during append")
    return result, dict(original_entries=79, total_entries=len(result_entries), original_payload_preserved=True, entries=report)


def prepare(rom_path: Path, v8_path: Path, v82_path: Path, encoder: Path, output: Path) -> dict:
    output = external_output(output)
    raw = rom_path.read_bytes()
    if len(raw) != ROM_SIZE or hashlib.sha256(raw).hexdigest() != ROM_SHA256:
        raise ValueError("Expected verified USA N64 big-endian ROM")
    if not encoder.is_file():
        raise ValueError("psxavenc executable is required for the native loading image")
    rom = V8N64Rom(rom_path.resolve())
    v8, files8 = verified_volume(v8_path, "v8")
    v82, files82 = verified_volume(v82_path, "v82")
    inputs = output / "inputs"
    inputs.mkdir(parents=True, exist_ok=True)
    native_inputs = {}
    for name in ("LEVELS/V8/AIRGRAVE.EXP", "LEVELS/NUCLEAR.EXP", "LEVELS/BAYOU.EXP", "SHELL/LEVELSEL.TBL", "SHELL/SHELL.DLL"):
        target = inputs / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(read_file(v82, files82, name))
        native_inputs[name] = target
    callbacks = frozenset(item.name for item in (
        *overlay_exports(rom.decoded("DREAMLND.DLL")),
        *psx_executable_exports(read_file(v8, files8, "SLUS_005.10")),
    ))
    source, conversion = convert_arena(rom.decoded("DREAMLND.EXP"), "DREAMLND", object_callback_names=callbacks)
    # Keep the exact intermediate used by the overlay conversion outside Git.
    (inputs / "DREAMLND.V8.EXP").write_bytes(source)
    location_bank, location_report = shell.build_location_bank(
        read_file(v8, files8, "SHELL/LOCATNS.BIN"), rom.decoded("LOCATNS.N64"))
    location_path = inputs / "LOCATNS.BIN"
    location_path.write_bytes(location_bank)
    with tempfile.TemporaryDirectory(prefix="v82-dreamland-loading-") as temporary:
        loading, _ = shell.build_loading_chunk(upstream._n64_loading_chunk(rom_path), encoder.resolve(),
                                              Path(temporary) / "loading.ppm", target_size=(320, 96))
    result = upstream.convert(source, native_inputs["LEVELS/V8/AIRGRAVE.EXP"],
                              iff.IffChunk(tag=b"XLSC", payload=loading),
                              native_inputs["LEVELS/NUCLEAR.EXP"], native_inputs["LEVELS/BAYOU.EXP"])
    package = output / "package"
    arena_path = package / "LEVELS/N64/DREAMLND.EXP"
    arena_path.parent.mkdir(parents=True, exist_ok=True)
    arena_path.write_bytes(result)
    sounds, sound_report = sequel_sound_bank(rom.data, read_file(v82, files82, "SHARED/SOUNDS.SND"), encoder)
    sound_path = package / "SHARED/SOUNDS.SND"
    sound_path.parent.mkdir(parents=True, exist_ok=True)
    sound_path.write_bytes(sounds)
    overlay_bytes, overlay_report = overlay.convert(rom.decoded("DREAMLND.DLL"))
    overlay_path = arena_path.with_suffix(".DLL")
    overlay_path.write_bytes(overlay_bytes)
    (output / "DREAMLND.EXE").write_bytes(overlay.psx_executable(overlay_bytes))
    (output / "dreamland.seeds").write_bytes(overlay.seeds(overlay_report))
    (output / "overlay-conversion.json").write_bytes((json.dumps(overlay_report, indent=2) + "\n").encode())
    source_table = native_inputs["SHELL/LEVELSEL.TBL"]
    resources = selector.split_resource_table(source_table.read_bytes())
    preview = upstream._selector_preview_tim(location_path, resources[8])
    table_path = package / "SHELL/LEVELSEL.TBL"
    table_path.parent.mkdir(parents=True, exist_ok=True)
    selector.write_extended_table(source_table, table_path, preview)
    menu_data, menu_report = dreamland_menu.convert(native_inputs["SHELL/SHELL.DLL"].read_bytes())
    menu_path = package / "SHELL/SHELL.DLL"
    menu_path.write_bytes(menu_data)
    entry = arenas.ArenaEntry("n64.super_dreamland_64", "Super Dreamland 64", "Super Dreamland 64",
                             "Levels\\N64\\DreamLnd.exp", 84, 222, 18)
    arenas.validate_primary_export(entry, overlay_bytes)
    arenas.write_registry(package / "ARENAS.V8R", [entry])
    artifacts = {}
    for path in (arena_path, overlay_path, sound_path, table_path, menu_path, package / "ARENAS.V8R"):
        data = path.read_bytes()
        artifacts[str(path.relative_to(package)).replace("\\", "/")] = dict(size=len(data), sha256=hashlib.sha256(data).hexdigest())
    report = dict(playable=False, runtime_integration_required=True,
                  reference_revision="6fc8179bebc8d30dc01f05d5cf1fd398a82e2a19",
                  source="N64 USA", target="SLUS-00868 USA", arena=asdict(entry),
                  conversion=asdict(conversion), location=location_report, artifacts=artifacts, sound=sound_report, menu=menu_report,
                  overlay=dict(functions=len(overlay_report["functions"]),
                               engine_calls=overlay_report["engine_calls"],
                               branch_likely=len(overlay_report["trampolines"])-1))
    (output / "import.json").write_bytes((json.dumps(report, indent=2) + "\n").encode())
    return report


def compile_overlay(output: Path, recompiler: Path, framework: Path) -> dict:
    """Compile externally and reject missing event/call paths before playtesting."""
    output = external_output(output)
    result = subprocess.run([
        str(recompiler.resolve()), str(output / "DREAMLND.EXE"), "--overlay",
        "--seeds", str(output / "dreamland.seeds"), "--out-dir", str(output / "generated"),
        "--project-root", str(framework.resolve()),
    ], capture_output=True, text=True, encoding="utf-8", errors="replace")
    diagnostics = result.stdout + result.stderr
    failures = [line for line in diagnostics.splitlines()
                if "ERROR" in line or "reserved opcode" in line or "left to the interpreter" in line]
    if result.returncode or failures:
        raise ValueError("Dreamland code generation failed: " + "\n".join(failures[-10:]))
    entries, ranges = set(), []
    for line in (output / "generated/DREAMLND.EXE_full.ranges").read_text().splitlines():
        fields = line.split()
        if fields and fields[0] == "F":
            entries.add(int(fields[1], 16))
        elif fields and fields[0] == "R":
            low, size = int(fields[1], 16), int(fields[2], 16)
            ranges.append((low, low + size))
    metadata = json.loads((output / "overlay-conversion.json").read_text())
    expected = {overlay.LOAD_BASE + f["offset"] for f in metadata["functions"] + metadata["trampolines"]}
    expected.update(overlay.LOAD_BASE + p for p in metadata["continuations"] + metadata["pointer_entries"])
    if expected - entries:
        raise ValueError(f"Missing compiled entries: {sorted(hex(p) for p in expected - entries)}")
    image = overlay.relocate((output / "package/LEVELS/N64/DREAMLND.DLL").read_bytes(), overlay.LOAD_BASE)
    calls = [overlay.LOAD_BASE + p for p in range(0x2C0, 0x3208, 4) if overlay.read32(image, p) >> 26 == 3]
    missing = [p for p in calls if not any(low <= p < high for low, high in ranges)]
    if missing:
        raise ValueError(f"Missing compiled call sites: {[hex(p) for p in missing]}")
    report = dict(compiler="psxrecomp", compiled_entries=len(entries), checked_call_sites=len(calls),
                  missing_entries=0, missing_call_sites=0, playable=False)
    (output / "compile-validation.json").write_bytes((json.dumps(report, indent=2) + "\n").encode())
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("rom", "v8", "v82", "encoder", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--recompiler", type=Path)
    parser.add_argument("--framework", type=Path)
    args = parser.parse_args()
    if bool(args.recompiler) != bool(args.framework):
        parser.error("--recompiler and --framework must be provided together")
    report = prepare(args.rom, args.v8, args.v82, args.encoder, args.output)
    if args.recompiler:
        report["codegen"] = compile_overlay(args.output, args.recompiler, args.framework)
    print(json.dumps({key: report[key] for key in ("playable", "runtime_integration_required", "overlay", "artifacts")}, indent=2))
    if "codegen" in report:
        print(json.dumps(report["codegen"], indent=2))
