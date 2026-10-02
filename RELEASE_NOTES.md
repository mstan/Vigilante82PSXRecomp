# v0.1.0 — initial private playtest

Native Windows x64 and Linux x64 builds of Vigilante 8: 2nd Offense (USA,
SLUS-00868), using psxrecomp and the shared launcher.

- Optional thirteen-car original roster, original selection/result voices,
  original soundtrack and Super Dreamland 64, converted from user-owned media.
- Original cars are available independently to both two-player selectors.
- LAN / Direct IP exchanges the host's mod plan and verifies matching content.
- Enhanced native-wide rendering in single player and netplay; multiplayer
  uses fixed 16:9 or 21:9, without changing the view on window resize.
- Optional true intermediate render frames in single-player and netplay battles,
  with stock logic, timer, audio and network cadence.

## Known issues

High Frame Rate is experimental. The multiplayer HUD can shift and flicker
between vehicle icons during intermediate frames. The owner accepted the
remaining build for this initial release and deferred this fix.

The presentation rate is a target; expensive frames may generate fewer extra
images. WAN/online-room play, full campaign completion, every death/result
sequence and every content combination have not been certified.

## Validation

All 21 title checks and nine focused framework checks pass. A fresh 21:9
Meteor Crater Versus session with an imported car confirmed netplay through
tick 24,904; all 682 compared core checkpoints matched. Over 2,000 intermediate
frames passed full machine-state restoration verification. The owner verified
original cars on both players, correct voices and soundtrack selection, and
accepted the offline Meteor Crater background. The HUD issue above remains.

The repository ships source and preparation metadata. Release binaries include
the frozen media importer and OpenBIOS; users supply all game media.

Production builds pass all 21 checks on Windows and Ubuntu 24.04 Linux under
WSL. Each package includes 59 matching native overlay shards and ten verified
bundled mod packages. Both frozen importers converted the combined menu from
the supplied sequel disc and verified its expected SHA-256. Isolated startup
checks loaded OpenBIOS and initialized the bundled OpenGL renderer on both
platforms; the Windows check advanced 2,429 frames with no fatal report.
Linux controller/audio play and cross-platform netplay remain human playtests.

The Linux x64 AppImage is built on Ubuntu 24.04 and requires glibc 2.39 or
newer. It can run with `--appimage-extract-and-run` when FUSE is unavailable.
New overlay compilation on Linux uses GCC from PATH; the bundled native cache
and frozen media importer do not require a separate Python installation.
