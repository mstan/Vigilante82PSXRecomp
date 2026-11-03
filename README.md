# Vigilante82PSXRecomp

Private playtest downloads are published on the
[releases page](https://github.com/mstan/Vigilante82PSXRecomp/releases).
See [START_HERE](packaging/release/START_HERE.txt) for setup and
[release notes](RELEASE_NOTES.md) for known issues. v0.1.5 restores prepared-plan
reuse for the default offline launch without changing source-dependent content
verification. v0.1.4 updates the shared
engine, launcher and netplay dependencies, defaults new installs to 4x internal
resolution and enables native-wide Fit to Window. Saved mod choices are retained.
v0.1.3 added default-on
**Playtest Diagnostics** (input recording and verified replay, see below) on
the current psxrecomp framework; v0.1.2 fixed water disappearing at ultrawide
screen edges. See [playtest validation](docs/PLAYTEST.md) for the follow-up.

## Playtest diagnostics

The default-on `v82.diagnostics` package records, for each offline guest
VBlank, both pads (buttons, sticks, connected state) and a cheap RAM digest to
`saves/diagnostics/session-<stamp>/` beside the game, with the starting memory
cards, settings and mod choices. `diagnostics-location.txt` beside the game
names the current session folder. After reproducing a bug, quit normally and
**zip the whole newest session folder** (it contains starting saves; never
include the disc or BIOS). Netplay sessions are not recorded. Turn it off in
Mods. Developers replay a session with
`tools/replay_v82_diagnostics.py <session> --exe <original build> --disc <cue>
--output <new dir>`; replay stops at the first divergence with a message.

One combined Vigilante 8 game under **psxrecomp**, using USA 2nd Offense
(`SLUS-00868`) as its gameplay base and directly adapting the authorized
[reference implementation](https://github.com/GTTeancum/Vigilante-8-recomp).

The target is the sequel's eighteen cars plus thirteen independent original
V8 car entries, the eight sequel arenas plus ten original arenas, and N64
Super Dreamland 64. The sequel disc already contains all eighteen retail arena
assets. Sequel mechanics stay native to the sequel game code. Existing V8
project compatibility is not a requirement.

## Current result

The native Arcade and two-player car selectors expose 18 sequel cars plus 13 separate original cars,
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

## Two-player netplay

Open **NETPLAY** in the launcher to host or join an online room. Configure
your local controller under **PLAYER 1 / NETPLAY** on each computer, then
choose **2 PLAYER > VERSUS** or **COOPERATIVE** in the game. Each peer controls
one standard controller port; the title offers two seats and no Multitap.

Both peers need the same build and the complete USA sequel CUE with all
17 tracks. The host selects the original-car and Super Dreamland 64 packages
with the room's **Mods** button. Both peers select their own donor images in Mods before launching. Package versions, selected features and the resolved
content fingerprint must match; local resource paths may differ. Donor assets
are never downloaded from the other player. The guest's saved offline mod
selection is preserved, including when returning from a match.

Rollback is enabled by default. LAN / Direct IP also supports two players and
the host's selected content packages, including original cars and music.
Both peers prepare and verify their own media before the match; Direct IP
launches require matching selected content. Netplay uses the enhanced native-wide
renderer from single player for each player's own full-screen camera. Select
16:9 or 21:9 in Widescreen → View before starting; netplay locks that ratio.
Other offline view choices default to fixed 16:9 during netplay. Selection, pause and
confirmation menus keep their complete original layout. Local offline two-player
play retains the native split-screen options.

The High Frame Rate package from `feat/v82-high-frame-rate` is included in the
combined build, with true render passes for both single-player and netplay
battles. Choose a display refresh or fixed presentation rate in Mods. Extra
frames interpolate the local camera and objects through the game's draw code;
logic, timers, audio and network simulation retain their original cadence.
Rendering remains at native 1x; the selected rate is a target and expensive
frames may render fewer intermediate images.

The release builds use the pinned `psxrecomp-v4` framework and `recomp-ui`
submodules. Windows production output is under `build-release`; Linux uses
a native build directory. See [release build instructions](docs/BUILD_RELEASE.md).

Validation: two isolated headless peers completed native menu selection and
entered a Versus match; eight independent input checks routed to the correct
car on both peers, with advancing matching state hashes through tick 43944.
Unsplit was checked with two hidden OpenGL peers on Nuclear Plant and Meteor
Crater, including independent driving, both pause controls, quit and a fresh
match. All 1,582 compared core-state checkpoints matched.
Mod negotiation, changed-media rejection, portable paths, and offline-selection
preservation have automated regression coverage. An actual online room with
the combined roster and Dreamland still needs human validation; WAN latency
and adverse-network behavior were not tested in this local run.

## Widescreen and original music

Two optional packages add adaptive widescreen and the original soundtrack.
Both default off. **Widescreen > View > Fit to Window** follows resizing
from 4:3 through 32:9; fixed 16:9, 21:9 and 32:9 are also available. Terrain
and object frusta grow together, gameplay gets larger packet buffers, the sky
fills the revealed area, and HUD elements anchor to the edges. Menus and movies
keep their original layout. Internal resolution stays at 1x.

Fit generates terrain for its maximum view independently of the window size,
so different monitors do not change that guest culling decision. It therefore
costs more geometry work than fixed 16:9. Native backface tests are retained.
The new display/music combination still needs a paired online playtest.

**Original Vigilante 8 soundtrack** adds twelve songs to the sequel's sixteen.
Select **Both games**, **Vigilante 8**, or **2nd Offense** in the package options.
The game's Start/pause **Track** selector shows the original song names; its
ordinary playback and rotation use the selected album. Playback uses native
CD audio, including the retail repeat/lead-out timing. Samples are lossless
44.1kHz stereo from the supplied disc, not replacement recordings.

Select the original **USA v1.0 disc image** in either Original vehicles or
Original soundtrack. Both use one shared setting, so the other picker is
filled automatically. Select the **USA N64 ROM** for Super Dreamland 64.
The sequel disc is selected once on the launcher's main page. No mod asks
for sequel media, extracted files or prepared packages.

On Play, the bundled importer extracts and converts the required content into
the user's cache, and the engine checks its exact size and SHA-256. Subsequent
launches reuse verified results. Neither Python nor extraction tools need to be
installed by players. Original vehicles work without Dreamland; Dreamland works
without an original PS1 disc. Resident Loading keeps verified data files in
memory and speeds up the game's loading work. Disable it in Mods to restore
the original loading path. CD Speed and Fast Loading (host pacing) are omitted
from this title's catalog.

Disc images can be CUE/BIN or CHD. Original music needs the audio tracks, and
original result voices need raw XA sectors; a data-only 2048-byte ISO has neither.
N64 .z64, .v64 and .n64 byte orders are accepted. Start a fresh game after this
update because the generated hook configuration and package identities changed.

## Developer preparation

Keep all media and conversion output outside this repository. Run:

```powershell
py -3 tools/prepare.py --cue '<sequel-media>/Vigilante 8 - 2nd Offense (USA).cue' --output '<external-extracted-dir>'
py -3 tools/import_original_cars.py --v8 '<original-v1.0-data-track.bin>' --v82 '<sequel-data-track.bin>' --output '<external-import-dir>'
py -3 tools/import_original_presentation.py --v8 '<original-v1.0-data-track.bin>' --v82 '<sequel-data-track.bin>' --output '<external-import-dir>/package'
py -3 tools/import_dreamland.py --rom '<N64-USA.z64>' --v82 '<sequel-data-track.bin>' --encoder '<psxavenc.exe>' --output '<external-dreamland-dir>' --recompiler '<framework>/recompiler/build/psxrecomp-game.exe' --framework '<framework>'
$env:PATH = 'C:\msys64\mingw64\bin;' + $env:PATH
& '<framework>/recompiler/build/psxrecomp-game.exe' --config game.local.toml --project-root '<framework>'
& 'C:\Program Files\CMake\bin\cmake.exe' -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSX_DEBUG_TOOLS=ON '-DPSXRECOMP_ROOT=<framework>' '-DRECOMP_UI_ROOT=<recomp-ui>' '-DPSXRECOMP_BIOS_STEMS=OpenBIOS;SCPH1001'
& 'C:\Program Files\CMake\bin\cmake.exe' --build build-dev -j 16
```

Launch with `--game game.local.toml`. The standard profile uses native
resolution; 4K is not enabled. The v0.1.0 release pins its private framework
and shared UI through `.gitmodules`. See [release build instructions](docs/BUILD_RELEASE.md)
for a clean source checkout and Windows/Linux packaging.
psxrecomp owns CPU/BIOS execution, rendering, audio, input and saves throughout.

`tools/third_party/NOTICE.md` records permission, attribution and licenses.
The source reference clone remains separate, outside this repository. The
private v0.1.0 repository includes the combined title and pins the integrated
framework with mod-plan negotiation, verified CD-audio playlists, enhanced
native-wide netplay and forward-only intermediate render passes. Local-player
expansion beyond two players remains deferred.
Work is tracked centrally as `beads-eio.12.1`.
