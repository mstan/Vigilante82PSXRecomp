"""Prepare the verified sequel executable outside Git and local build config."""
import argparse
import hashlib
from pathlib import Path
import re
from media_inputs import external_output, verified_volume, read_file

EXE_SHA256 = "9ecfd0a7986cee816ef79284a6635eaadba435183362f8b796f667b8fb5a3eb6"


def prepare(cue: Path, output: Path) -> Path:
    cue = cue.resolve()
    output = external_output(output)
    match = re.search(r'^\s*FILE\s+"([^"]+)"\s+BINARY\s*$',
                      cue.read_text(encoding="utf-8-sig"), re.I | re.M)
    if not match:
        raise ValueError("Expected a BIN/CUE disc")
    volume, entries = verified_volume(cue.parent / match[1], "v82")
    executable = read_file(volume, entries, "SLUS_008.68")
    if hashlib.sha256(executable).hexdigest() != EXE_SHA256:
        raise ValueError("Unexpected sequel executable")
    root = Path(__file__).resolve().parents[1]
    config = (root / "game.toml").read_text(encoding="utf-8")
    output.mkdir(parents=True, exist_ok=True)
    target = output / "SLUS_008.68"
    target.write_bytes(executable)
    config = re.sub(r'^exe = .*$', f'exe = "{target.as_posix()}"', config, flags=re.M)
    config = re.sub(r'^disc = .*$', f'disc = "{cue.as_posix()}"', config, flags=re.M)
    destination = root / "game.local.toml"
    destination.write_text(config, encoding="utf-8")
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cue", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.cue, args.output))
