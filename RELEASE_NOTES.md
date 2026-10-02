# v0.1.1 ? multiplayer HUD fix

Private Windows x64 and Linux x64 update to the initial v0.1.0 playtest.

- High Frame Rate extra frames now retain the local player's vehicle icon and
  HUD placement in netplay. The guest split renderer interleaves each world
  view with the other player's HUD; replay now draws the matching local HUD.
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
