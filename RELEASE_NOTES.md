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
