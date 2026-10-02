"""Prepare original portraits and speech from verified owner discs, outside Git."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
from media_inputs import external_output, verified_volume, read_file
from import_original_cars import selector_records
from build_v8_selector_voice_bank import build, split_resident_bank
from extract_v8_result_voices import extract, validate
from tempfile import TemporaryDirectory


def native_result_stream(data):
    """Keep speech/cadence, using the sequel disc's non-audio filler sectors.

    The native CdReady callback (8001D9B0) reads a data sector's MSF and stops
    at file_end-1. Filter-rejected realtime audio produces no data interrupt
    under the PSX CD controller, so the reference's audio-only filler would
    let native playback run into the following file. Retail SHARED/*.XA uses
    file=1/channel=0/submode=0 filler instead. No speech payload is changed.
    """
    if not data or len(data) % (2336*8):
        raise ValueError("Invalid result speech cadence")
    result=bytearray(data)
    filler=bytes((1,0,0,0))*2+bytes(2328)
    for sector in range(len(data)//2336):
        if sector%8: result[sector*2336:(sector+1)*2336]=filler
    return bytes(result)


def outcomes(volume, files, output):
    if not volume.raw:
        raise ValueError("Result speech requires a raw BIN with XA sectors")
    with TemporaryDirectory(prefix="v8-voices-", dir=output) as temporary:
        folder=Path(temporary)
        for source,prefix in (("DEFEAT","D"),("VICTORY","V")):
            entry=files[f"SOUNDS/{source}.XA"]
            count=(entry["size"]+2047)//2048
            with volume.path.open("rb") as stream:
                stream.seek((volume.start_sector+entry["lba"])*2352);raw=stream.read(count*2352)
            if len(raw)!=count*2352:raise ValueError("Truncated result bank")
            bank=folder/(source+".XA")
            bank.write_bytes(b"".join(raw[i+16:i+2352] for i in range(0,len(raw),2352)))
            extract(bank,folder,prefix)
        header=bytearray(struct.pack("<4sI",b"V8XA",26)+bytes(26*8))
        body=bytearray()
        for i,voice in enumerate([folder/f"{prefix}{index:02}.XA" for prefix in ("D","V") for index in range(13)]):
            data=voice.read_bytes();sectors=len(data)//2336
            validate(voice,sectors//8)
            data=native_result_stream(data)
            struct.pack_into("<II",header,8+i*8,len(body)//2336,sectors)
            body.extend(data)
        target=output/"V8RESULTS.DAT";target.write_bytes(header+body)
        return dict(size=target.stat().st_size,sha256=hashlib.sha256(header+body).hexdigest())


def prepare(v8_path, v82_path, output):
    output = external_output(output)
    output.mkdir(parents=True, exist_ok=True)
    v8, f8 = verified_volume(v8_path, "v8")
    v82, f82 = verified_volume(v82_path, "v82")
    records = selector_records(read_file(v8, f8, "SHELL/CHARSEL1.TBL"))
    expanded, voice_report = build(read_file(v8, f8, "SHELL/SHELL.DLL"),
                                  read_file(v8, f8, "SHELL/SOUNDS.SND"),
                                  read_file(v82, f82, "SHELL/SOUNDS.SND"))
    resident, voices = split_resident_bank(expanded)
    sections = [records[f"SELECTOR_{i:02}.PPM"] for i in range(13)]
    sections += [records["SELECTOR_BASE_VIGILANTES.VLC"], records["SELECTOR_BASE_COYOTES.VLC"], resident, voices]
    data = bytearray(struct.pack("<4sI", b"V8ID", len(sections)) + bytes(8*len(sections)))
    for i, section in enumerate(sections):
        while len(data) % 4: data.append(0)
        struct.pack_into("<II", data, 8+i*8, len(data), len(section))
        data.extend(section)
    target = output / "V8IDENTITY.DAT"
    target.write_bytes(data)
    report = dict(size=len(data), sha256=hashlib.sha256(data).hexdigest(), voices=voice_report,
                  sections=[dict(size=len(s), sha256=hashlib.sha256(s).hexdigest()) for s in sections])
    (output / "presentation.json").write_text(json.dumps(report, indent=2)+"\n")
    report["outcomes"]=outcomes(v8,f8,output)
    (output / "presentation.json").write_text(json.dumps(report, indent=2)+"\n")
    return report


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--v8",type=Path,required=True)
    parser.add_argument("--v82",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    a=parser.parse_args();r=prepare(a.v8,a.v82,a.output)
    print(json.dumps({k:r[k] for k in ("size","sha256")},indent=2))
