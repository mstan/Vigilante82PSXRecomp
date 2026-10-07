# Vigilante 8: 2nd Offense parity candidate

This is a source preparation receipt for the owner-authorized campaign, not
current-build visual, performance or release acceptance. Central issue:
`beads-eio.12.21`. Candidate branch: `campaign/v82-parity-hle-20261006`.

## Preserved baseline and identities

The source base is main `e1133d10`, which descends from release `d39db526` and
retains its ultrawide water, local multiplayer HUD and high-frame-rate HUD fixes.
It also retains the newer native replay, PGXP hook build, stable world filtering
and resident loader/load-floor work. The owner's release checkout, generated
bodies, media, caches and saves are separate inputs and were not modified.

The candidate pins shared framework `e5e2dca85c2e7c1758c6de27870599103a1f8bfc`
(codegen `18d1867bb4`). This includes the corrected native HOLD presentation,
render-pass background-tag rollback, per-primitive filter batching and native
identity hashing. These shared changes require title requalification together.

The resumed first-review build uses common UI03d58aa0 and the framework's exact
recomp-net dependency e2491d2, retaining two-player/netplay support. Local legal
media is read from `_wt-pr544-v82/media`; no owner files are changed. Build
ENHANCED and its native cache, then let the owner check an arena, camera motion,
sky/water/HUD at wide aspect, pause, reload/revisit, and existing multiplayer.
Broad automated matrices are deferred per the owner's latest instruction.

Primary media is the complete 17-track USA sequel, SLUS-00868. Existing import
contracts declare 301,703,168 decoded data bytes, data digest
`12777aecb48f08d5f0a20b71ae8df4d64a8b5d5d4832664009586eaf99e58201`, and boot
executable digest `9ecfd0a7986cee816ef79284a6635eaadba435183362f8b796f667b8fb5a3eb6`.
These are recorded source contracts; no media was opened or rehashed during
this source phase. Reuse of existing V82 content remains authorized. The
abandoned original-V8 asset import campaign does not remove V82's own content.

## Build-selected resident implementation

`PSX_EXECUTION_PROFILE=ENHANCED` selects the existing native resident adapter.
`REFERENCE` selects its activation-only reference binding; all original guest
stream/decoder/load routines remain generated runtime sources and receive no
resident entry hooks. This is a maintained guest route, not a copied obsolete
reference shard. The existing product preference can disable resident loading
in ENHANCED. Backend selection is fixed at build time; former debug environment
selectors for resident observation/load-floor timing are removed.

The `v82-resident-loading` family uses contract
`slus00868-stream-and-load-floor-v1`. Its identity hashes the selected binding,
adapter constants, catalog, decoder, catalog recipe and shared resident/plugin
interfaces, resident/guest services and the timing API. Executables get an execution manifest;
snapshots/netplay distinguish profiles. Use separate build and cache directories.

The 82-file catalog covers 20,267,008 sector bytes. Whole sectors, including
padding, are the hashing unit. The live guest code digest covers 13 ordered
ranges and is `b906ebfec04550da36a8a13cf621d15ac38d51d07254a55a27264a78c11eaff4`.
Catalog values are preserved; their current-disc check still needs execution.

| Caller boundary | Observable result and ordering |
| --- | --- |
| `80017F50 WholeRead(a0 dest, a1 LBA, a2 sectors)` | Only aligned RAM extents within a stock catalog file; complete sector DMA first, CD state byte `gp+83A=FF`, return destination in v0 to ra. |
| `80018148 Begin(a0 LBA)` from Open, ra `8001842C` | Require file/base equality, guest code identity and valid gp. Publish stream record and ring state, `gp+D68=LBA`, CD state FF, v0 FF, return to ra. Directory walking and Seek's own Begin keep guest execution. |
| `80018478 Read(a0 dest, a1 length)` / `800185F0 ReadByte()` | Copy verified bytes, materialize the current partial sector at `800B6170`, update `gp+D58/D64/D68`, then return v0=1 or the byte. Snapshot-backed Expansion 1 record owns stream identity. |
| `80018644 Seek(a0 signed offset, a1 whence)` / `800181E0 End()` | Seek publishes position/sector then returns the sector pointer; End clears record/next LBA before returning v0=0. Bounds or identity declines keep/restart the original stream as appropriate. |
| `80021C24`, ra `80013798` | For an arena wait with a0=0, mark a running wipe finished. Original cleanup restores the callback/draw environment; no false skipped continuation. |
| `80054C4C VSync`, ra `8001E954/5C/64/6C/74`, saved caller `80033064` at sp+28 | Only five ignored VSync(0) waits in EXP object setup complete immediately. Other VSync calls retain stock pacing. |
| `80020A80 RectAlloc`, ra `8001F490/8001F5C8` | Run the original allocation with supplied a0..a3 and ra, preserving its RAM output and v0 while omitting internal guest time. A 32M-cycle watchdog remains; nested entry runs the body. |
| `80021064 LZSS`, ra `8001F5F4`, a0 rectangle/a1 stream | Match every consumed compressed byte to a derived texture. Drain the queued CLUT upload through `8005BF88 DrawSync(0)` before row uploads, then return v0=0. Unmatched input keeps the decoder. A failed chunk leaves the original decoder continuation active to overwrite the full rectangle rather than falsely report completion. |
| `800535EC SpuWrite`, ra `8001DFE8` | Only callback-free DMA bank chunks. Publish libspu bookkeeping and whole 64-byte blocks, complete SPU upload before `SpuDone=1`, return clamped requested size. Rejected upload keeps original DMA; PIO/callback cases retain guest code. |

The fake-drive contract fixture covers randomized stream results, independent
decoder output, state restore, identity/bounds, ordering and five load shims.
It is not a real-caller comparison. This candidate adds second-chunk VRAM
failure coverage that checks preserved registers, compressed source and
rectangle, then drives an independent decoder fallback to the complete VRAM
result. Already uploaded rows are raw pixel replacement into the same rectangle;
the repeated prefix has the same pixels and is idempotent. Staging aliases of
the compressed source or rectangle are declined before DrawSync or native writes.
Real-caller failure injection remains a runtime gate. The C++ model fixture
has not yet been compiled or run. The historical improvement from about 307
to 45 guest frames in one arena load is
a useful baseline, not a campaign speed claim.

## Actual presentation and geometry coverage

Default source settings are OpenGL, 1080p, default-on PGXP CPU dataflow,
perspective correction through the PGXP package, stable world filtering with
nearest untracked HUD, adaptive Fit to Window and display-refresh native replay.
Nearest in game.toml is intentional for untracked UI. The HFR plugin selects
HOLD and keeps the last authored image if replay is unavailable; it no longer
substitutes motion-adaptive image blending. Settings alone do not qualify parity.

Source checks exposed existing player-config drift: its function-entry list
omitted load-floor hooks `80021C24`, `80054C4C` and `800535EC`, and its netplay
section omitted `content_negotiation=true`. The candidate player config now
matches the developer config apart from the permitted audit/debug removal.
This changes its codegen/config identity; existing AOT must be audited against
the corrected config before any reuse, even if its native binaries are retained.

| Feature | Actual title implementation and remaining gate |
| --- | --- |
| Native phases | 1P span `800143FC..80014734`, capture at `80014D94` with ra `800146B4`, camera `8002E0E8`; object matrices/particles and camera are interpolated and guest draw code replays transactionally. Split spans `80013E90..80014154` / `800141FC..80014358` reuse the opposite HUD section and wait for native DrawSync. Require current-framework correctness counters and motion/audio/input checks. |
| Adaptive world visibility | `8001C134` establishes gameplay and expands packet arena; terrain row traversal `8001BE68`, view corner/ray corrections `8001C250/8001C304`, object planes `8002E22C` and screen bounds `8002E024`. Subdivision screen walkers widen eight paired bounds `80024E6C..80027668`. Verify live 4:3, 16:9, 21:9, 32:9 and resizing without edge clipping or activation holes. |
| Water / backgrounds / HUD | Native base water `80015FB0..8001665C`, tiled surface `80016664` and grid splitting before the 32x32 limit; sky packets from `8001C910`, HUD chains `80037A00..8003827C`. Preserve water reserve and packet budgets. Moving sky width jitter remains owner issue `beads-eio.12.19`; corrected shared tag rollback needs owner motion review. |
| Distance / subdivision / fog | `800288E0` consumes cell X/Z and packet cursor a0/a1/a2, returns cursor v0, may emit many native packets and touches scratchpad/GTE. The title still runs its subdivision. Row padding and wide cull bounds do not establish extended far distance. `gp+DB4` feeds the far terrain ray, but this is not an identified fog/activation boundary. Further caller/consumer analysis belongs to `beads-eio.12.20`; no generic 3x patch, subdivision bypass or native family is claimed. |
| Residency and transitions | Stock files use the existing native stream service; unknown/modified/virtual content stays with its existing guest/content adapters. Cold launch pack/hash/predecode cost, warm arena change, menus, revisits, SPU completion and all audio still need measured coverage. |
| Existing multiplayer | Preserve local split, LAN/Direct IP and existing unsplit netplay paths. Qualify both peers with identical profile/plan and each local camera/HUD. This campaign adds no new multiplayer mode. |

Performance issue `beads-eio.12.18` records uncontrolled Meteor Crater/Route 40
samples and substantial guest redraw cost. They do not attribute the owner's
individual hitches or establish a campaign speedup. Do not resume parked shared
DMA/guest-cycle/CD-clock experiments for this source task.

## Pending commands and promotion gates

All build, game, disc, cache regeneration and archive commands below require a
coordinator resource token. They are listed for the next phase and have not run.

1. Configure separate ENHANCED and REFERENCE release directories with the pinned
   framework, shared UI and owned generated source. Retain the PGXP runtime
   flavor. Re-audit current main generation against codegen `18d1867bb4` and
   game.toml's instruction/entry sites before copying or generating any input.
   Compile with two jobs only after a compiler slot is assigned.
2. Run `ctest --test-dir <build> -R v82-resident --output-on-failure` and all title
   checks. The catalog test requires `V82_DISC=<owned sequel.cue>` and uses
   `V82_FRAMEWORK=<pinned framework>`. Exercise both execution profiles; capture
   real-loader destinations, returned values, continuation, VRAM and SPU results
   and completion ordering at the named caller boundaries.
3. Stage isolated saves/media bindings with `tools/stage_playtest.py <build>
   --config <private playtest.local.toml>`. Its current all-feature route also
   requires optional original/N64 media; for primary-disc qualification use an
   isolated primary-disc setup and leave optional packages disabled. Do not
   modify the owner's cards/settings or reuse another person's debug port.
4. Navigate to a known arena, then issue a **fresh Cross press after loading**.
   Native main `800132CC` waits at `800139E4` for controller bit `00400000` in
   RAM `8006B508`; neutral input alone does not enter gameplay. Record the
   primary-disc matrix across camera movement, weapons/effects, water, sky,
   pause/HUD, split/unsplit play, scene cuts, arena changes and revisits.
5. In a debug correctness build launched with `PSX_RENDER_PASS_VERIFY=1`, run
   `python <framework>/tools/qualify_render_passes.py --port <isolated-port>
   --case <actual-arena-path> --executable <exact-runtime> --purpose correctness
   --output <private-receipt.json>`. Replay and verification must be nonzero and
   error-free. Relaunch without verification for `--purpose performance`.
   Compare matched camera/workload samples across the two profiles; do not sum
   overlapping presenter/guest/GPU timing or treat interpolated swaps as ticks.
6. Observe each controlled load window using
   `python <framework>/tools/load_probe.py --port <isolated-port> run --secs 30
   --out <private-load.json>`. Record cold/warm host seconds, guest frames,
   resident events, timing pauses and audio rather than merely increasing CD
   speed. Preserve timing/results that callers require, omit internal waits
   where the established native service already owns completion.
7. Audit native overlay DLL/range pairs and codegen/config/flavor tags before
   reuse. No AOT has been copied, changed or regenerated in this source phase.
   Prepare platform-native emitters/cache independently for Linux.
8. `tools/package_release.py --build-dir <qualified-build> --stage <empty-stage>
   --recompiler-build <native-emitter-build> --cache-dir <audited-cache>
   --download-cache <tool-cache> --platform win --mingw-bin <native-bin>
   --zip <candidate.zip> --framework <pinned-framework>` binds execution to
   final Windows bytes immediately before the shared ZIP writer. For Linux,
   `PSXRECOMP_ROOT=<framework> bash tools/package_appimage.sh <linux-build>
   <linux-emitter-build> <output>` stages a native ELF payload and rebinds the
   execution receipt after linuxdeploy before making the AppImage. Both retain
   licenses/importer/toolchain and exclude retail media/personal state. Verify
   candidate hashes and launch on each platform; helper source changes alone
   do not establish a valid package.
9. Final owner playtest must accept motion, background width, image quality,
   gameplay/audio, load speed and existing multiplayer. Keep campaign and
   follow-up Beads open until those gates pass. No public push, merge or release
   is authorized by this source phase.

## Source-phase validation receipt

The developer/player configuration comparison passes after correcting the
pre-existing entry-hook and netplay drift. Python source syntax, all bundled
manifest TOML, enhanced defaults, display-refresh/Fit choices, HOLD-only native
presentation and recorded media/code digests were checked without reading media.
Bash syntax for Linux build/AppImage scripts and PowerShell packaging syntax
pass. Git whitespace checks pass. These checks establish source consistency;
they do not establish that either profile compiles or that packaging succeeds.

No C++ compiler, CMake configure, game process, disc hash, media copying, AOT
generation or archive creation ran in this worker phase. The new VRAM-failure
contract fixture, both runtime profiles, actual callers, gameplay, loading,
Windows ZIP, native Linux AppImage and owner acceptance remain pending.
