"""Resident Loading catalog is current for the owned disc in V82_DISC (.cue).

Skips (exit 77) without the disc; the catalog never contains disc content."""
import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
disc = os.environ.get('V82_DISC')
if not disc or not Path(disc).exists():
    print('V82_DISC not set; skipping catalog check')
    sys.exit(77)
framework = Path(os.environ.get('V82_FRAMEWORK', root / 'psxrecomp-v4'))
sys.exit(subprocess.call([sys.executable, str(root / 'tools/v82_resident_catalog.py'), '--check',
                          '--disc', disc, '--framework', str(framework),
                          '--output', str(root / 'src/mods/v82_resident_catalog.inc')]))
