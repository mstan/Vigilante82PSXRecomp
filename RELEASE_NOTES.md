# v0.4.0 - widescreen and independent-screen netplay showcase

The public Windows release brings a player-focused README, 4x internal
resolution and adaptive Fit to Window defaults, and two-player netplay with
an independent full-screen camera per peer at 16:9 or 21:9. Original Vigilante
8 cars/music and Super Dreamland 64 remain optional imports from your own
media. Explicit choices and native memory-card saves are preserved.

Uses the qualified psxrecomp `060ef0ab`, recomp-ui `dd47b188`, recomp-net
`7a4d330e` and rbengine `28a2a472` stack, including v0.1.5's default offline
launch correction. Public packages now use the shared release channel,
manifest and audits. Diagnostic recording/display are off by default and
remain selectable. CD Speed and Fast Loading are absent.

High Frame Rate remains experimental. Quest keeps its retail roster. Save
states are build-specific; native memory-card progress is separate.

# v0.1.5 - default offline launch preparation

Updates psxrecomp to `060ef0ab`; UI and networking dependencies are unchanged.

Restores preparation reuse for the default offline mod selection. The default
plan contains no converted resources or disc-dependent patches, so it can be
prepared before the launcher and reused on Play after checking the actual
catalog and selection contents. It no longer hashes the entire disc solely for
an unused offline fingerprint.

Default diagnostic recording uses the committed offline plan without requesting
a netplay content hash on its first frame. Replay still requires the same build,
execution profile and mod plan.

Netplay and source-dependent conversions retain their content verification.
Existing 4x resolution, adaptive Fit to Window, gameplay fixes and saved mod
choices remain included. CD Speed and Fast Loading remain excluded.

## Validation

The Windows production package passes all 28 title checks. Fresh-profile and
warm-process launches with the actual network disc use zero whole-disc hashes,
conversion helpers or launcher preparation jobs. Reused runtime commits took
20.9/29.9 ms. Changed selections and same-size, restored-timestamp manifest edits
reject the stale plan and preserve externally saved choices.

The fresh launcher shows 4x and enabled Fit to Window; actual Meteor Crater
driving, current-build save/load and warm restoration pass. Tests used the
bundled native toolchain with system compilers absent from PATH. Existing cache
coverage limits remain; optional imports, other arenas, two-peer netplay and
Linux were not newly qualified by this default offline startup correction.

# v0.1.4 - current engine and 4x defaults

Updates psxrecomp to `d35f13c9`, recomp-ui to `dd47b188`, recomp-net to
`7a4d330e` and rbengine to `28a2a472`. Fresh installs use a fixed 4x internal
resolution and enable Widescreen with adaptive Fit to Window. Existing saved
mod and aspect choices remain in control after updating.

Both netplay peers must use this release. Previous gameplay fixes and content
packages remain included; High Frame Rate remains experimental. CD Speed and
Fast Loading are excluded from the release.

## Validation

The Windows x64 production build passes all 28 title checks. The packaged
launcher shows 4x and enabled Fit to Window; actual OpenGL allocation remains
4x in Meteor Crater. Retail Arcade selection, driving, current-build save/load
and a warm restart were exercised with isolated saves and muted output.

The package includes 78 GCC and 144 TinyCC native cache pairs. With host
compilers removed from PATH, the bundled Python/TinyCC toolchain compiled and
loaded additional native overlays. Other arenas and imported-content combinations
were not requalified; unprepared code uses the existing capture/compile fallback.
This update does not require a system compiler on Windows.

Current Windows media verification still runs on each process start. A warm
launch using a network-hosted disc spent 28.6 seconds verifying the disc;
local-disc startup was not measured. This is not a startup-performance claim.
This release refresh provides a Windows build; Linux was not rebuilt.

# v0.1.3 - playtest diagnostics

Adds default-on **Playtest Diagnostics** (`v82.diagnostics`): offline sessions
record both pads, their connected state and a rotating RAM digest at every
guest VBlank into `saves/diagnostics/session-<stamp>/`, with the starting
memory cards, settings and mod choices. Zip the newest session folder when
reporting a bug (never the disc or BIOS). `tools/replay_v82_diagnostics.py`
replays a session headless against the same build and stops at the first
divergence. Netplay is never recorded. Disable it in Mods.

Rebuilt on psxrecomp `7cddab82` and recomp-ui `0546fbcb` (from `d3b22491` and
`1a8ae64b`). Gameplay, content packages and netplay compatibility are
otherwise unchanged; both netplay peers must use v0.1.3. Start a fresh game
after updating.

## Validation

Windows and Linux production builds pass all 28 registered checks (two
fixture-gated checks skip without owner media). A headless recording of
6,659 VBlanks, including real Meteor Crater driving, weapon and P2 hot-plug
input, replays against the same build with zero divergence; both packaged
downloads boot with bundled OpenBIOS, record a session and (Windows) replay
it. Recording costs about 30 us per VBlank on the game thread (max 0.4 ms).
Not validated: a real two-peer netplay session, human-played recordings,
Linux replay, and gameplay on the new framework beyond the headless runs.
The native overlay cache now ships 1,322 per-fragment shards per platform
(12 fragments in one overlay region fall back to the interpreter).

# v0.1.2 - ultrawide water fix

Water now extends across the selected widescreen view instead of stopping at
the original 320-pixel screen bounds. Wider wave surfaces render in bounded
batches through the native water renderer, preserving its buffer limits.
This fixes the missing water observed at Nuclear Plant's screen edges.

The accepted building/subdivision behavior and v0.1.1 multiplayer HUD fixes
remain included. Both netplay peers must use the same release and matching
content/mod choices. Start a fresh game after updating.

## Validation

Windows OpenGL checks cover Nuclear Plant at native 1x resolution in 16:9,
21:9 and 32:9. The before/after comparison shows water in previously bare
side areas. A live High Frame Rate run captured 345 actual presentations and
completed 13 verified render passes with no state mismatch, abort or VRAM
leak. Tests cover water bounds for the 320-pixel single-player and 160-pixel
enhanced multiplayer views, packet integrity, and complete grid coverage.

Windows and Linux production builds pass all 21 title checks, including the
original-car package fixture.

A new two-peer water playtest and Linux visual water inspection have not
been performed. High Frame Rate remains experimental; its selected rate is
a target, not a guaranteed frame rate. Existing platform and playtest
limitations below still apply.

# v0.1.1 ? multiplayer HUD fix

Private Windows x64 and Linux x64 update to the initial v0.1.0 playtest.

- High Frame Rate extra frames now retain the local player's vehicle icon and
  HUD placement in netplay. The guest split renderer interleaves each world
  view with the other player's HUD; replay now draws the matching local HUD.
- Weapon icons and their ammo counts now move together in the compact
  multiplayer HUD at both fixed aspects, including extra frames.
- The pending HUD context stays with its world frame until native DrawSync.
- Bounded render replay stays out of compiled continuations that would run
  past its requested stop instruction.

Original cars, voices, music, LAN / Direct IP content negotiation, enhanced
native-wide rendering and fixed 16:9 / 21:9 multiplayer views remain included.
Both players must use the same release and matching content/mod choices.

## Validation

The prior replay reproduced icon switching and shifting HUD bounds. The fix
was checked across 200 consecutive actual presentations per player at each
fixed aspect, including multiple extra presentations within a guest frame.
More than 2,800 replay passes and 2,100 extra presentations completed with no
state restoration mismatch, span failure, watchdog or VRAM leak. A fresh
single-player Meteor Crater match also produced verified extra frames.

An armed multiplayer state reproduced the detached ammo count before the
anchor fix. The corrected count sits within its weapon display at 16:9 and
21:9 across another 200 presentations per player per aspect. This armed run
completed over 1,600 verified replay passes with no restoration mismatch or
span failure. Regression checks cover both compact layouts, both aspects,
one to three ammo digits, and returning to the single-player HUD anchors.

Windows and Linux builds pass all 21 title checks, including the owner car
package fixture, and six focused framework checks. Release packaging verifies
version/config parity, ten bundled mods, matching native overlay cache shards,
bundled importers/toolchains, dependency notices and absence of donor media.

## Known limitations and setup

High Frame Rate remains experimental. The requested rate is a target;
expensive frames may generate fewer extra images. WAN/online-room play,
full campaigns, every death/result sequence and every content combination
remain outside the validated playtest scope. Linux controller/audio play and
cross-platform netplay still need human validation.

Supply your complete USA 2nd Offense disc (SLUS-00868, all 17 tracks). Optional
content requires your original V8 USA v1.0 disc and/or V8 USA N64 ROM. The
packages include OpenBIOS and automatic media conversion; no game discs,
converted donor assets, retail BIOS images or personal saves are included.
Follow START_HERE.txt.

The Linux x64 AppImage requires glibc 2.39 or newer (Ubuntu 24.04 or a comparable
newer system). Use --appimage-extract-and-run if FUSE is unavailable. New Linux
overlay compilation uses GCC from PATH; no separate Python installation is
needed. Source builds require access to the pinned private psxrecomp-v82 repo.
