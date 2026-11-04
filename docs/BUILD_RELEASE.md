# Building and releasing Vigilante 8: 2nd Offense

Clone the title with its pinned framework and launcher:

```
git clone --recurse-submodules git@github.com:mstan/Vigilante82PSXRecomp.git
```

For an existing clone, run `git submodule sync --recursive` and
`git submodule update --init --recursive`.

## Prepare your own media

Use Python 3.12+ with PyInstaller, NumPy and Pillow, CMake, Ninja and a native
C/C++ compiler. Windows releases use MinGW64. Install psxavenc 0.3.1, pinned
source revision `bdc24e897ed09d2fd3d64cdbadc625b0673c76db`, or set its matching
executable through `V82_PSXAVENC` in the CMake cache. The usual Windows location
under `%LOCALAPPDATA%/psxrecomp/tools` is detected automatically.

```powershell
python tools/prepare.py --cue "<complete sequel.cue>" --output "<external media directory>"
```

This extracts the verified USA executable and creates ignored `game.local.toml`.
Its expected SHA-256 is
`9ecfd0a7986cee816ef79284a6635eaadba435183362f8b796f667b8fb5a3eb6`.
The release pipeline generates game/BIOS sources and uses bundled OpenBIOS.

## Cut a release

```powershell
psxrecomp-v4/tools/package_title_release.ps1 --channel public --platform windows --jobs 2
```

`tools/package_release.py` and `.ps1` are thin aliases. The required channel is
`public` (diagnostics default off) or `playtest` (diagnostics default on, marked
artifact name). Explicit player choices remain in control. The title registers
the shared diagnostics-default hook for its own recorder.

The shared pipeline and `packaging/release.toml` own provenance, regeneration,
production compilation, staging and artifact verification. This recipe retains
the qualified portable, unprofiled runtime build; packaging does not launch a
training game. See the framework's
[release documentation](../psxrecomp-v4/docs/RELEASING_A_TITLE.md).

The small V82 post-stage hook adds its verified conversion worker, notices and
current native cache from `build-release/cache`, using the shared codegen tag
and cache selector. Never relabel shards generated for another tag. The bundled
overlay compiler remains the fallback for code outside the cache. The worker's
source/encoder/environment revision must match; game/donor media, saves and
personal settings must not enter a ZIP.

`dist/Vigilante82PSXRecomp-v<version>-windows-x64.zip` is the deliverable. Keep
checksums and build evidence in the workspace and publish one current ZIP.
This release supplies Windows binaries; Linux needs a separately qualified
recipe and native build.

## Validation

Run CTest after building. `V82_ORIGINAL_CARS_PACKAGE` selects the original-car
fixture; `V82_DISC` enables the actual-disc Resident Loading catalog check.
Distinguish skipped fixtures from passed checks. Actual packaged gameplay,
save/load and netplay results remain in [PLAYTEST.md](PLAYTEST.md).

Use isolated profiles for automated QA, mute background launches and preserve
desktop focus. Product screenshots use `present_shot` and a completed
`present_shot_seq`, which capture final aspect-fitted presentation. A raw VRAM
or pre-fit image does not demonstrate the presented 21:9 view.
