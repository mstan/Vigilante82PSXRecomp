"""Run the authorized reference's complete V8-to-V8:2 vehicle conversion.

The reference's project construction and registry codecs are reused directly.
This wrapper supplies verified owner media and keeps all outputs outside Git.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import base64
from dataclasses import replace
from media_inputs import external_output, verified_volume, read_file

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS / "third_party"))
sys.path.insert(0, str(TOOLS / "third_party/combined"))
import build_v8_to_v82_guest_roster as upstream
from vigilante8_vehicle_tools import registry, project, conversion, iff, compiler

# Reserve only the effects needed by the selected drivers. Identical textures
# deduplicate at runtime; the sparse common SFX archive is stored once and its
# samples enter SPU memory on demand.
SPECIAL_SOUND_IDS = (0, 5, 0x14, 0x15, 0x20, 0x2E, 0x2F, 0x30, 0x37, 0x38, 0x39, 0x3E, 0x41)
SPECIAL_EFFECTS = {0:{2,0x1B,0x2B},2:{0xD,0x10,0x2D},3:set(),4:{0x2D},
    5:{0,0x1F,0x27},6:{0,0xC,0x13,0x1A},8:{2,0x16},9:{2,0x17},10:{0xF},11:{4,0xC,0x18,0x2E}}


def shared_special_banks(vehicles, first):
    effects=conversion.v8_bank_to_v82(upstream.decode_bank(upstream.V8_COMMON,"V8",14))
    source=iff.IffChunk(tag=b"FORM",form_type=b"XOBF",children=[iff.IffChunk(
        tag=b"SND ",payload=(upstream.ROOT/"PS1 game/SOUNDS/MAIN.SND").read_bytes())])
    sounds=registry._decode_sounds(source)
    silent=dict(pitch=0x400,adpcm_base64=base64.b64encode(bytes([0,1])+bytes(14)).decode())
    sparse=tuple(sounds[i] if i in SPECIAL_SOUND_IDS else silent for i in range(max(SPECIAL_SOUND_IDS)+1))
    forms=[];records=[]
    for kind in range(13):
        roots=SPECIAL_EFFECTS.get(kind)
        if roots is None or not roots:
            records.append(struct.pack('<51H',65535,*([65535]*50)));continue
        bank,mapping=conversion.extract_roots(effects,roots)
        form=compiler.compile_xobf(vehicles[0],bank)
        if not forms:form.children.append(iff.IffChunk(tag=b"SND ",payload=compiler.compile_sounds(replace(vehicles[0],sounds=sparse))))
        records.append(struct.pack('<51H',first+len(forms),*(mapping.get(i,65535) for i in range(50))))
        forms.append(form)
    return iff.IffDocument(chunks=forms).to_bytes(),b'V8FX'+struct.pack('<I',first)+b''.join(records)


def connect_lightning(vehicles):
    """Original Clyde's own strike/impact assets, never a sequel substitution."""
    vehicles=list(vehicles)
    vehicle=vehicles[1]
    if vehicle.stable_id!="guest.v8.slick_clyde":raise ValueError("Original roster order changed")
    fields=("groups","slots","collisions","textures","animations")
    body=project.ObjectBank(**{key:getattr(vehicle,key) for key in fields})
    effects=conversion.v8_bank_to_v82(upstream.decode_bank(upstream.V8_COMMON,"V8",14))
    impact,mapping=conversion.extract_roots(effects,{0x14,0x15})
    if mapping[0x15]!=mapping[0x14]+1:raise ValueError("Lightning impact/spark order changed")
    merged,bases=conversion.merge_banks((body,impact))
    form=iff.IffChunk(tag=b"FORM",form_type=b"XOBF",children=[
        iff.IffChunk(tag=b"SND ",payload=(upstream.ROOT/"PS1 game/SOUNDS/MAIN.SND").read_bytes())])
    sounds=registry._decode_sounds(form)
    if len(vehicle.sounds)!=3:raise ValueError("Lightning source sound slots changed")
    vehicles[1]=replace(vehicle,**{key:getattr(merged,key) for key in fields},
        original_special_type=1,original_impact_kind=bases[1]+mapping[0x14],
        sounds=vehicle.sounds+(sounds[0x37],sounds[0x41]))
    return tuple(vehicles)


def selector_records(data: bytes) -> dict[str, bytes]:
    # Ported from reference tools/recompone-v8-2/extract_v8_selector_portraits.ps1.
    if len(data) < 4:
        raise ValueError("Truncated selector table")
    count = struct.unpack_from("<I", data)[0]
    if count < 18 or 4 + count * 4 > len(data):
        raise ValueError("Invalid selector table size")
    offsets = list(struct.unpack_from(f"<{count}I", data, 4)) + [len(data)]
    if offsets[0] < 4 + count * 4 or any(a >= b for a, b in zip(offsets, offsets[1:])):
        raise ValueError("Invalid selector table offsets")
    result = {}
    for i in range(13):
        record = data[offsets[i]:offsets[i+1]]
        if len(record) < 24:
            raise ValueError("Truncated native banner")
        width, height, header, mode = struct.unpack_from("<HHII", record)
        if not (0 < width <= 512 and 0 < height <= 512 and header >> 24 == 0x38 and mode == 0x20004):
            raise ValueError("Unexpected native banner format")
        result[f"SELECTOR_{i:02}.PPM"] = record
    for index, faction in ((16, "COYOTES"), (17, "VIGILANTES")):
        record = data[offsets[index]:offsets[index+1]]
        if len(record) < 12 or struct.unpack_from("<II", record)[1] != 0x20008 or record[3] != 0x38:
            raise ValueError("Unexpected native faction backing")
        result[f"SELECTOR_BASE_{faction}.VLC"] = struct.pack("<HH", 256, 480) + record
    return result


def prepare(v8_path: Path, v82_path: Path, output: Path) -> dict:
    output = external_output(output)
    v8, files8 = verified_volume(v8_path, "v8")
    v82, files82 = verified_volume(v82_path, "v82")
    inputs = output / "inputs"
    for volume, files, prefix, names in (
        (v8, files8, "PS1 game", ("SLUS_005.10", "COMMON.EXP", "SHELL/VEHICLES.EXP", "SOUNDS/MAIN.SND")),
        (v82, files82, "V8_2_WORK/disc", ("SHARED/COMMON.EXP",)),
        (v82, files82, "V8_2_LOOSE", ("SLUS_008.68",)),
    ):
        for name in names:
            target = inputs / prefix / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(read_file(volume, files, name))
    # Preserve the reference's data/path conventions without writing into its clone.
    upstream.ROOT = inputs
    upstream.V8_COMMON = inputs / "PS1 game/COMMON.EXP"
    upstream.V8_SELECTOR_VEHICLES = inputs / "PS1 game/SHELL/VEHICLES.EXP"
    upstream.V8_EXE = inputs / "PS1 game/SLUS_005.10"
    upstream.V82_COMMON = inputs / "V8_2_WORK/disc/SHARED/COMMON.EXP"
    upstream.V82_EXE = inputs / "V8_2_LOOSE/SLUS_008.68"
    vehicles = connect_lightning(upstream.build_projects())
    vehicles = tuple(replace(v, original_special_type=i) for i,v in enumerate(vehicles))
    package = registry.compile_package(vehicles)
    decoded = registry.decompile_package(package.archive, package.registry)
    if [project.to_dict(v) for v in decoded] != [project.to_dict(v) for v in vehicles]:
        raise ValueError("Reference vehicle package changed during semantic roundtrip")
    banks=len(iff.parse(package.archive).chunks)
    effects,metadata=shared_special_banks(vehicles,banks)
    package=replace(package, archive=package.archive+effects,registry=package.registry+metadata)
    portraits = selector_records(read_file(v8, files8, "SHELL/CHARSEL1.TBL"))
    artifacts = {"CUSTOM.EXP": package.archive, "VEHICLES.V8R": package.registry,
                 **{f"SHELL/{name}": data for name, data in portraits.items()}}
    report = dict(playable=False, source="V8", target="V8_2", retail_replacements=0,
                  vehicle_count=len(vehicles), entries=[dict(id=v.stable_id, name=v.display_name,
                    controller=v.controller_class, transformations=v.supports_transformations,
                    original_special=v.original_special_type) for v in vehicles],
                  artifacts={name: dict(size=len(data), sha256=hashlib.sha256(data).hexdigest())
                             for name, data in artifacts.items()})
    for name, data in artifacts.items():
        target = output / "package" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    (output / "import.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--v8", type=Path, required=True)
    parser.add_argument("--v82", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = prepare(args.v8, args.v82, args.output)
    print(json.dumps({key: report[key] for key in ("playable", "vehicle_count", "entries")}, indent=2))
