"""Run the shared title pipeline; --channel public|playtest is required."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if __name__ == '__main__':
    raise SystemExit(subprocess.call([sys.executable,
        str(ROOT / 'psxrecomp-v4/tools/package_title_release.py'),
        '--root', str(ROOT), *sys.argv[1:]]))
