# Building v0.1.5

The private title repository pins the public `RetroPortingToolKit/psxrecomp`
framework and `RetroPortingToolKit/recomp-ui` launcher as submodules. A
collaborator needs access to `mstan/Vigilante82PSXRecomp` to build from source:

```
git clone --recurse-submodules git@github.com:mstan/Vigilante82PSXRecomp.git
```

For an existing clone, run `git submodule sync --recursive` before
`git submodule update --init --recursive` to use the current public URLs and
the commits pinned by the title.

Players need only the release download, their complete USA sequel disc, and
any optional original/N64 donor media. See `packaging/release/START_HERE.txt`.

## Source prerequisites

Use CMake, Ninja, a native C/C++ compiler and Python 3.12+ with PyInstaller,
NumPy and Pillow. The tested Linux importer versions are PyInstaller 6.20.0,
NumPy 2.2.6 and Pillow 11.3.0. Windows uses MinGW64 under `C:/msys64/mingw64`.
Linux needs SDL3 3.4+, OpenGL, FreeType, HarfBuzz and curl development libraries.
The runtime can fetch its pinned SDL3 if a system package is unavailable.

Build psxavenc 0.3.1 from the pinned upstream revision
`bdc24e897ed09d2fd3d64cdbadc625b0673c76db`, using Meson and FFmpeg development
libraries, or supply its matching Windows release executable. Set
`V82_PSXAVENC` in the CMake cache (Windows) or environment (Linux).

Build the framework `psxrecomp-game` and `psxrecomp-bios` targets under
`psxrecomp-v4/recompiler/build-release`. `tools/prepare.py --cue <sequel.cue>
--output <external-extraction-directory>` extracts the verified executable
and writes ignored `game.local.toml`. Invoke `psxrecomp-game --config
game.local.toml --project-root psxrecomp-v4` to generate the ignored main C.
Never hand-edit generated C or commit donor media, captures or converted assets.

Resident Loading is the title's loading option. The bundled media importer
prepares selected owner media and records verified persistent receipts; warm
launches reuse unchanged imports. The build packages its importer revision
alongside the helper, and framework resolution verifies the prepared outputs.

## Production build and packaging

Windows: configure this title with Ninja, `CMAKE_BUILD_TYPE=Release`,
`PSX_DEBUG_TOOLS=OFF`, `PSXRECOMP_ROOT=<title>/psxrecomp-v4`,
`RECOMP_UI_ROOT=<title>/recomp-ui`, and
`PSXRECOMP_BIOS_STEMS=OpenBIOS;SCPH1001`; then build and run CTest.
Call the intended CMake executable directly when Windows PATH contains Unix
shims. Linux: `bash tools/build_linux.sh <native-linux-build-directory>`.
Set `V82_PYTHON` to a Python environment containing the importer dependencies.
Use a native Linux build directory when building a Windows checkout in WSL.

Before packaging, compile the available external `overlay_captures.json`
against `packaging/release/game.toml` and the pinned framework using
`psxrecomp-v4/tools/compile_overlays.py`. Supply `--recompiler`,
`--runtime-include`, `--gcc` and `--out-dir <build>/cache`. Run this separately
on each platform; Windows DLLs and Linux shared objects are different artifacts.
The initial release carries 59 native overlay shards on each platform. Captures
are developer inputs and must never be bundled or checked into this repository.

Run `tools/package_release.ps1` for Windows, or
`bash tools/package_appimage.sh <linux-build> <native-recompiler-build>` for
Linux. Both use `tools/package_release.py` and the framework's shared staging
implementation, verify the build version, all bundled mod packages,
the matching native overlay cache, and the media/overlay tools. Staging requires
an empty output directory. Packages appear under ignored `dist/`.

The single version source is `packaging/release/VERSION`. The player config must
match all developer gameplay/codegen settings, with only the audit section and
debug port removed. The Linux packaging script verifies AppImage tooling by
SHA-256 on every use and retains its AppDir for smoke checks. It includes
dependency notices and uses writable per-user data through `AppRun`.

The Linux build was tested on Ubuntu 24.04 under WSL. The bundled cache works
without a host compiler; compiling newly discovered overlays on Linux uses
GCC from PATH. The media importer bundles its own Python on both platforms.
