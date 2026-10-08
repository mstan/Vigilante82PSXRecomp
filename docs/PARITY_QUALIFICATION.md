# Vigilante 8: 2nd Offense parity candidate

This is the serial review record for the owner-authorized campaign. Final
gameplay and performance acceptance belongs to the owner. Central issue:
`beads-eio.12.21`. Candidate branch: `campaign/v82-parity-hle-20261006`.

## Preserved baseline and identities

The review branch was rebased onto public main `0b4e2957` on 2026-10-08. It
retains current verified-media startup, worker-based media preparation, launcher
box art and the complete left-anchored single-player HUD, together with the
title's resident loading and PGXP build. Imported content stays disabled.

The renderer integration `de771e3e75c8f62d73295e6c7d7ceb833949e5b4` combines public
`d3b22491` with the parity framework `e740b81c`. This keeps the current public
startup and HD texture support and the build-selected resident implementation.
The merged renderer assigns its per-primitive filter attribute separately from
HD texture attributes. Focused real-GL checks passed: native filtering (509),
HD replacements (56), windowed HD replacements (57), and native baseline (19).
BIOS sources and stamps were regenerated with that framework's actual emitter.
The current source pin `c8f042f68382e963d817e53d3944ccd3dd557aac` adds the shared
verified launcher-artwork helper. Original V8 and V82 now use that helper, with
their own immutable image URL, digest and offline override. Both configurations
and runtime builds succeed; this extraction changes no rendering behavior.

The launcher uses public UI `1a8ae64b`, OpenBIOS, internal resolution 1080p,
Fit widescreen, texture correction and Resident Loading. Interpolation is
archived and unavailable. Generic CD-speed and host-pacing packages are omitted.
The owner should check an arena, camera motion, terrain/sky/water/HUD at wide
aspect, music, pause and reload/revisit. Broad automated matrices are deferred.

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
| Interpolation | Archived under `development/retired-interpolation`; absent from the serial build and launcher catalog. |
| Adaptive world visibility | `8001C134` establishes gameplay and expands packet arena; terrain row traversal `8001BE68`, view corner/ray corrections `8001C250/8001C304`, object planes `8002E22C` and screen bounds `8002E024`. Subdivision screen walkers widen eight paired bounds `80024E6C..80027668`. Verify live 4:3, 16:9, 21:9, 32:9 and resizing without edge clipping or activation holes. |
| Water / backgrounds / HUD | Native base water `80015FB0..8001665C`, tiled surface `80016664` and grid splitting before the 32x32 limit; sky packets from `8001C910`, HUD chains `80037A00..8003827C`. Preserve water reserve and packet budgets. Moving sky width jitter remains owner issue `beads-eio.12.19`; corrected shared tag rollback needs owner motion review. |
| Distance / subdivision / fog | The actual native range is 80 units for ordinary arenas and 48 for late arenas, written at `8001C190/8001C1A8`. `gp+DB4` feeds the terrain rays and far fog endpoint; `gp+DB6/DB8` also supply subdivision bands. Native subdivision and four-corner row traversal remain intact. The widened arena check had no packet-budget failures. Whole-object visibility at `8002E22C..8002E2E0` now repairs actual signed-halfword input wrap using the three full-coordinate planes, preserving the GTE and the Boolean return ABI. It fixes a demonstrated visibility disagreement without increasing the far range. Residual terrain seams and a further distance extension remain tracked under `beads-eio.12.20`. |
| Residency and transitions | Stock files use the existing native stream service; unknown/modified/virtual content stays with its existing guest/content adapters. Cold launch pack/hash/predecode cost, warm arena change, menus, revisits, SPU completion and all audio still need measured coverage. |
| Existing multiplayer | Preserve local split, LAN/Direct IP and existing unsplit netplay paths. Qualify both peers with identical profile/plan and each local camera/HUD. This campaign adds no new multiplayer mode. |

Performance issue `beads-eio.12.18` records uncontrolled Meteor Crater/Route 40
samples and substantial guest redraw cost. They do not attribute the owner's
individual hitches or establish a campaign speedup. Do not resume parked shared
DMA/guest-cycle/CD-clock experiments for this source task.

## Serial validation and review

The ENHANCED PGXP runtime builds with two compiler jobs. The canonical emitter
regenerated native main and both BIOS profiles. Developer and player configs
have overlay config hash `07053ec0`; emitter hash is `d1867bb4`. Running the
capture compiler against those identities reported no failures; the one
observed delay-slot/data region has no executable roots and is deliberately
left on the interpreter fallback. No cache identity was relabeled.

A private fresh OpenBIOS startup reached the first arena with internal scale
5x (1080p target), active resident whole-file/stream/SPU paths and interpolation
disabled. Its baseline 278 world frames recorded 429 wrapped object inputs and
270 plane disagreements, which motivated the guarded return correction. The
focused widescreen fixture checks both wrapped visibility outcomes and preserves
representable native results. This short hidden run has audio and physical
controllers disabled; it does not certify music or complete arena coverage. The actual
executable and execution manifest are bound in the coordinator's review receipt.

Final owner check: start an arena, move and turn through terrain edges, check
sky/water/HUD, listen to music, then pause and reload/revisit. Terrain seams,
far visibility and existing multiplayer remain review items. Keep the owning
Beads issues open until accepted. No release is published by this preparation.

The owner limited the subsequent terrain-seam investigation to two approaches.
Neither produced a qualified fix in a short fresh OpenBIOS arena comparison:
enabling the PGXP position-cache fallback left visible gaps and supplied no
fallback vertices in the sampled triangle records; enabling shared native-wide
projection recovery also left the gaps, including after a short camera turn.
The latter recorded two recovered vertices and three oversized triangles, but
did not repair the observed seams. Neither experimental setting was enabled in
the product. The residual issue remains open as `beads-eio.12.20`; further
terrain-seam work is deferred while the serial campaign proceeds to original
Vigilante 8. The earlier whole-object visibility repair remains in the candidate.

## Reference implementation

Use a separate build directory with `-DPSX_EXECUTION_PROFILE=REFERENCE` to
select the resident reference binding and execute the original generated guest
loader. The normal ENHANCED build selects the native adapter. The source-owned
resident contract fixture and catalog verifier remain available for targeted
investigations; broad profile matrices are deferred by the owner for this pass.
