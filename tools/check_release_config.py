"""Reject player config drift, including netplay and renderer hook metadata."""
from pathlib import Path
import tomllib

ROOT = Path(__file__).resolve().parent.parent


def check():
    dev = tomllib.loads((ROOT / 'game.toml').read_text(encoding='utf-8'))
    player = tomllib.loads((ROOT / 'packaging/release/game.toml').read_text(encoding='utf-8'))
    dev.pop('audit', None)
    dev.get('runtime', {}).pop('debug_port', None)
    if player != dev:
        raise SystemExit('Player config differs beyond the permitted audit/debug removal')
    print('Player config matches all gameplay, codegen, controller and netplay settings')


if __name__ == '__main__':
    check()
