# Vigilante82PSXRecomp

One combined Vigilante 8 game under **psxrecomp**, using USA 2nd Offense
(`SLUS-00868`) as its gameplay base and directly adapting the authorized
[reference implementation](https://github.com/GTTeancum/Vigilante-8-recomp).

The target is the sequel's eighteen cars plus thirteen independent original
V8 car entries, the eight sequel arenas plus ten original arenas, and N64
Super Dreamland 64. The sequel disc already contains all eighteen retail arena
assets. Sequel mechanics stay native to the sequel game code. Existing V8
project compatibility is not a requirement.

## Current result

The native Arcade menus expose 18 sequel cars plus 13 separate original cars,
and 18 retail arenas plus Super Dreamland 64. Original cars can also be selected
as opponents. Both combined-content features must be enabled. Quest retains its
retail roster; imported campaigns are outside this playable slice.
Survival also preserves the selected original car. Retail arenas use the retail
shared sound bank; Dreamland's appended effects load only on Dreamland.

Houston and Y have focused gameplay, original-special and flight checks. Both
now run together on Dreamland after native menu selection. All thirteen original
menu previews have been constructed in one uninterrupted carousel pass, followed
by a return to the sequel roster. Sixteen focused title checks pass. Dedicated
retail-map testing is intentionally excluded at the owner's request.

Original driver portraits, selection voices, victory/defeat speech, menu names
and in-game destruction names now use the original identities. Car models,
stats, independent identities and opponent previews are connected. This remains
a local playtest build; Quest campaigns and full presentation parity are not claimed.
Dreamland's appended effects use half-rate filtered audio with adjusted pitch and
loop positions to fit alongside the selected vehicle banks in PS1 sound RAM;
all 79 retail shared sounds remain unchanged. Full-quality streaming is deferred.

Slick Clyde's original lightning now has its launch, three strikes, original
voice, thunder, impact effects and native collision callbacks. The imported
animation codec preserves source cycle timing. All thirteen original specials
are now implemented, including their effects, voices, targeting and native
collision behavior. All ten newly added families fired in isolated matches;
Sheila has a full-magazine/audio check and Torque has normal/early terrain
restoration tests. Human checks of natural pickups, aiming and feel remain useful.
Only the selected cars' effect textures are reserved; original shared sounds
load on demand to fit the PS1 memory budgets.

See [the playtest checklist](docs/PLAYTEST.md). Native resolution is the default;
4K is disabled. Converted content, donor media and captures stay outside Git.

## Local preparation

Keep all media and conversion output outside this repository. Run:

```powershell
py -3 tools/prepare.py --cue '<sequel-media>/Vigilante 8 - 2nd Offense (USA).cue' --output '<external-extracted-dir>'
py -3 tools/import_original_cars.py --v8 '<original-v1.0-data-track.bin>' --v82 '<sequel-data-track.bin>' --output '<external-import-dir>'
py -3 tools/import_original_presentation.py --v8 '<original-v1.0-data-track.bin>' --v82 '<sequel-data-track.bin>' --output '<external-import-dir>/package'
py -3 tools/import_dreamland.py --rom '<N64-USA.z64>' --v8 '<original-v1.0-data-track.bin>' --v82 '<sequel-data-track.bin>' --encoder '<psxavenc.exe>' --output '<external-dreamland-dir>' --recompiler '<framework>/recompiler/build/psxrecomp-game.exe' --framework '<framework>'
$env:PATH = 'C:\msys64\mingw64\bin;' + $env:PATH
& '<framework>/recompiler/build/psxrecomp-game.exe' --config game.local.toml --project-root '<framework>'
& 'C:\Program Files\CMake\bin\cmake.exe' -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSX_DEBUG_TOOLS=ON '-DPSXRECOMP_ROOT=<framework>' '-DRECOMP_UI_ROOT=<recomp-ui>' '-DPSXRECOMP_BIOS_STEMS=OpenBIOS;SCPH1001'
& 'C:\Program Files\CMake\bin\cmake.exe' --build build-dev -j 16
```

Launch with `--game game.local.toml`. The standard profile uses native
resolution; 4K is not enabled. This checkpoint uses framework commit
`b9f9ba230fac9b24024ac6797e009231e7de538d` on
`checkpoint/v82-combined-20260930`, and recomp-ui
`faa3330fd574fcfc9d64b0381d301cdfb87a62d4`.
psxrecomp owns CPU/BIOS execution, rendering, audio, input and saves throughout.

`tools/third_party/NOTICE.md` records permission, attribution and licenses.
The source reference clone remains separate, outside this repository. All
source is checkpointed locally on `checkpoint/v82-combined-20260930`;
nothing has been published. All 16 title checks and five focused framework
checks passed at the checkpoint. The owner paused further implementation:
adaptive widescreen, original music and netplay remain unimplemented here.
Work is tracked centrally as `beads-eio.12.1`.
