#!/usr/bin/env python3
"""Prefill a V8:2 build directory for a human playtest.

Points settings.toml at the sequel disc and BIOS, binds the shared mod sources
(original V8 disc, N64 ROM), and enables every bundled mod feature, so the
launcher opens ready to play. Paths come from playtest.local.toml (gitignored):

    sequel_disc   = "//Server/games/PSX/Vigilante 8 - 2nd Offense (USA)/Vigilante 8 - 2nd Offense (USA).cue"
    original_disc = ".../vigilante8-v10/Vigilante 8 (USA) (v1.0).cue"   # must be USA v1.0
    n64_rom       = ".../vigilante8-n64-usa.z64"
    bios          = "F:/Projects/psxrecomp/psxrecomp/bios/SCPH1001.BIN"
    saves         = "F:/Projects/psxrecomp/Vigilante82PSXRecomp/saves"   # copied, never written

Usage: py -3 tools/stage_playtest.py <build_dir> [--config playtest.local.toml]
"""
import argparse
import re
import shutil
import sys
import tomllib
from pathlib import Path

SOURCES = {"v8.original": "original_disc", "v8.n64": "n64_rom"}


def q(s):
    return '"' + str(s).replace("\\", "/").replace('"', '\\"') + '"'


def set_key(text, section, key, value):
    """Set key in [section] of a launcher settings.toml, adding either if absent."""
    line = f"{key} = {value}"
    m = re.search(rf"^\[{re.escape(section)}\][^\[]*", text, re.M)
    if not m:
        return text.rstrip("\n") + f"\n\n[{section}]\n{line}\n"
    body = m.group(0)
    if re.search(rf"^{re.escape(key)}\s*=", body, re.M):
        body = re.sub(rf"^{re.escape(key)}\s*=.*$", line, body, count=1, flags=re.M)
    else:
        body = body.replace(f"[{section}]\n", f"[{section}]\n{line}\n", 1)
    return text[:m.start()] + body + text[m.end():]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir", type=Path)
    ap.add_argument("--config", type=Path,
                    default=Path(__file__).resolve().parent.parent / "playtest.local.toml")
    a = ap.parse_args()
    cfg = tomllib.loads(a.config.read_text(encoding="utf-8"))
    build = a.build_dir.resolve()
    for k in ("sequel_disc", "original_disc", "n64_rom", "bios"):
        if not Path(cfg[k]).is_file():
            sys.exit(f"missing {k}: {cfg[k]}")

    # Memory cards: copies of the real cards, never the originals.
    saves = build / "saves"
    saves.mkdir(exist_ok=True)
    for card in ("card1.mcd", "card2.mcd"):
        src = Path(cfg.get("saves", "")) / card
        if src.is_file() and not (saves / card).exists():
            shutil.copy2(src, saves / card)

    st = build / "settings.toml"
    text = st.read_text(encoding="utf-8") if st.exists() else (
        "# psxrecomp user settings - written by the launcher. Safe to hand-edit.\n")
    text = set_key(text, "disc", "path", q(cfg["sequel_disc"]))
    text = set_key(text, "bios", "path", q(cfg["bios"]))
    text = set_key(text, "memcard", "dir", q(saves))
    text = set_key(text, "memcard", "card1", q(saves / "card1.mcd"))
    text = set_key(text, "memcard", "card2", q(saves / "card2.mcd"))
    st.write_text(text, encoding="utf-8")

    # Every bundled package and feature enabled; shared sources bound.
    out = ["format_version = 2", "", "[sources]"]
    out += [f"{q(sid)} = {q(cfg[key])}" for sid, key in SOURCES.items()]
    for manifest in sorted((build / "mods" / "bundled").glob("*/*/manifest.toml")):
        m = tomllib.loads(manifest.read_text(encoding="utf-8"))
        out += ["", "[[package]]", f"id = {q(m['id'])}"]
        for f in m.get("feature", []):
            out += ["", "[[feature]]", f"package_id = {q(m['id'])}",
                    f"id = {q(f['id'])}", "enabled = true"]
    (build / "mods" / "state.toml").write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"staged {build}")


if __name__ == "__main__":
    main()
