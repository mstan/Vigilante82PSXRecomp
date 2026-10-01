# Combined-content integration

Owner direction, 2026-09-30: mirror the existing combined game as directly as
possible, with no requirement to retain the original V8 host or previous
project behavior. psxrecomp remains the runtime/recompiler. Source reference:
`GTTeancum/Vigilante-8-recomp`, revision
`6fc8179bebc8d30dc01f05d5cf1fd398a82e2a19`.

| Reference component | Use here |
|---|---|
| `tools/recompone-v8-2/prepare_reference.py` | Reuse the 145 resident callback seeds; compile owner sequel MIPS with psxrecomp. |
| `tools/blender_addons/tests/build_v8_to_v82_guest_roster.py` | Reused project construction for all thirteen original cars; already produces the package from owner media. |
| `tools/recompone-v8-2/extract_v8_selector_portraits.ps1` | Native VLC portrait/backing extraction adapted in the preparation wrapper. |
| `sdk/V82VehicleRegistry.cs`, `sdk/NativeVehicleBankSource.cs` | Adapt independent loading, stats, bank/texture ownership, transformations and selector integration. |
| `sdk/V8OriginalSpecials.cs`, `sdk/V8VehicleMath.cs` | Reuse original-vehicle behavior adaptations in the sequel engine. |
| `tools/recompone-v8-2/port_dreamland_assets.py`, `v82_native_water_conversion.py`, `v82_native_selector_table.py`, `v82_arena_registry.py` | Reuse Dreamland assets, water, preview and registry construction. Source modules are copied with provenance. |
| `tools/recompone-v8-2/port_dreamland_overlay.py` | Reuse its explicit engine, object-layout and event mappings; implement equivalent generation through psxrecomp rather than hand-editing generated C. |
| `sdk/V82ArenaRegistry.cs` | Adapt the appended arena selection and file resolution. |

The next acceptance result is an original car selected as its own entry and
driven in the sequel. The following result is playable Dreamland. Base sequel
cars/behavior and the eighteen retail arenas use their existing game code.
Check each newly connected path with an actual load/drive/screenshot and the
relevant registry/callback counters. Resolve concrete failures; do not restart
format reconstruction or a broad validation campaign already covered by the
reference and owner-media conversion.

The initial isolated runtime is under
`%LOCALAPPDATA%/psxrecomp/runs/vigilante82-20260930` (port 4537), and converted
cars under `%LOCALAPPDATA%/psxrecomp/imports/v82-original-cars`. No game assets
or runtime evidence are source-repository contents.

## Runtime adaptation, 2026-09-30

`src/mods/v82_vehicle_package.{h,cpp}` consumes the reference's existing v4
`VEHICLES.V8R` and `CUSTOM.EXP` directly. The owner package passes in the native
C++ test: thirteen entries, twelve transforming ground controllers, one flying
controller, independent bank ownership, native object roots and transformation
instance markers. Tests also reject truncation, invalid offsets and shared bank
ownership. This is a runtime-format reader, not a playable roster yet.

Framework issue `beads-eio.3.212` adds
`psx_mod_register_guest_function_plugin`: a trusted title callback can own an
aligned address in the unused physical `0x0F000000..0x0FFFFFFF` range. Its active
package owns resource access. Generated BIOS dispatch invokes it before hardware
routes and retains the existing return/stack contract. Plan clearing removes it.
Tests cover activation, aliases, duplicate-address rejection, nested entry/VBlank
scope and dispatch emission. Both BIOS profiles were regenerated and the title
rebuilt. The title must map reference synthetic callbacks (e.g. `0x807FF000`) to
this reserved namespace, e.g. `0x8FFF0000`; do not fabricate guest machine code.

The native runtime adapter is now implemented in `src/mods/v82_combined_plugin.cpp`.
Its default-off development option selects an original car for the next match.
It builds independent BIN/ANM/SND banks, reserves textures and supplies authored
suspension. Mutable ownership and per-object upgrade bytes live in guest RAM.
Seven full-word-guarded instruction seams select stats, upgrade status, normal
and transformed wheel banks, transformation tables and native special behavior.
Function hooks reject transformation pickups for non-transforming controllers.

Fresh isolated run 2026-09-30, PID 52612, port 4537:
- Chassey Blue, type 64, player variant -1, native object `0x8056EC18`.
- Two banks built, four wheels, 97 unique texture reservations; runtime texture
  claims reached 44. Native vehicle events reached 4860 by frame 38225.
- Captures `realloc-fixed-started.png`, `chassey-drive.png`,
  `chassey-restored.png`, `chassey-pause.png` were inspected. Gameplay HUD and
  native pause menu are visible. Position changed during acceleration/steering.
- Save/load slot 5 completed (`generation=8`, `last_ok=1`). Slot 9 is a loading
  gate state from this build; slot 10 is title entry, slot 11 main menu.
- `v82-guest-heap`, package parsing and plugin audit pass. Framework runtime,
  recompiler patch and dirty-interpreter routing tests pass.

The first runtime adapter moved blocks on shrinking realloc. Unlike the source
implementation, that invalidated retained pointers; one arena object's callback
became `0x80C1E2EE` and startup stalled. Retaining the original allocation when
its capacity suffices fixed the fresh run. The allocator regression test covers
this exact internal-pointer invariant, failed growth, payload copying and
identity retirement after the constructor replaces the factory callback.

Remaining required integration: native independent roster/portraits and NPC
selection; ground-car specials and transformations tested in play; original
Houston/Y special callbacks and Y flying/no-wheel construction; repeated match
teardown; Dreamland assets/overlay/arena selector. Quest persistence is not yet
adapted. Upgrade state currently follows object lifetime. Full combined parity
is not claimed. The initial source-only converter report remains `playable:false`
because the complete thirteen-car package has not passed gameplay coverage.

The instruction API (`beads-kbj0`, overlay ABI 27) implements the reference's
in-function seams without modifying generated output. It permits register/data
changes before a guarded instruction and forbids guest reentry or PC changes.
For additional sites, use the pinned reference's explicit register selection;
do not restart format reconstruction. Framework callbacks issue `beads-eio.3.212`
is now exercised by actual original-car construction.

## Original special weapons, 2026-09-30

Following the owner's cost/scope reset, prioritize playable content and use
the development picker before investing in native menu reconstruction. The
previous assessment turn changed no source; this continuation directly adapted
the complete `sdk/V8OriginalSpecials.cs` into `v82_original_specials.inc`.
The eighth guarded instruction seam, `0x8004AF0C` / `0x16000003`, selects
Houston/Y callbacks before the native weapon constructor consumes S1.
All five weapon/projectile/effect callbacks use the existing framework guest
callback API. No framework changes were needed. Native services get the
reference's separate 0x40-byte outgoing stack frame, including argument five.

Fresh Houston match plus isolated test PID 51252, port 4537:
- Original Palomino model, two banks, four wheels, 124 texture reservations.
- Reference `UpdateSoakWeapon` fixture calls native `0x8003A500` with special
  mask `0x80000000`; it does not synthesize a weapon object. Enable only in an
  isolated debug build with `PSX_V82_TEST_ORIGINAL_SPECIAL=1`. The fixture is
  compiled out with `PSX_NO_DEBUG_TOOLS` and inactive without that exact value.
- L2 input (`buttons=65279`) fired six shots: ammo 12 -> 6, 48 propagated
  segments, one collision, all 54 segments retired. Viewed `houston-armed.png`
  and `houston-fired.png`; the latter shows the original beam effect.
- Save/load while firing completed at generations 7/8, pending=0, last_ok=1.
  `houston-special-restored.png` was viewed; native gameplay continued.
- Returned through the loss screen/location menu, loaded another match, and
  fired again: two more shots, sixteen propagated segments, eighteen retired.
  `houston-second-fired.png` was viewed. This is one repeat-match check, not a
  long lifecycle soak or proof of every cleanup path.
- Evidence: external run root `houston-special-validation.json`. Slot 5 is
  Houston without the fixture attachment, 6 armed, 8 during firing. Slot 7 is
  the loss screen; do not use it as a live-gameplay state. The isolated process
  was stopped after validation; no owner input was injected.
- Build, plugin audit, package parsing, heap regression and new
  `v82-original-specials` test pass. The new test exercises Houston and Y's
  callback/damage differences, fifth stack argument, failed construction
  cleanup, wrapping arithmetic, source sockets and bank-owned impact metadata.

The earlier remaining-work list is superseded for Houston/Y routine adaptation:
both are implemented, Houston is exercised in play, Y's flying vehicle is not.
Next content work remains Y's existing reference controller/no-wheel seams and
Dreamland conversion/overlay integration. Natural weapon pickup, other cars'
specials, transformations, the combined player/NPC picker and quest persistence
still require integration or gameplay evidence. Source remains uncommitted.

## Original Y flight, 2026-09-30

The previous goal turn made progress (Houston and original weapon routines);
this continuation adapts `ApplyOriginalV8FlyingPhysics` and `V8VehicleMath.cs`
directly in `src/mods/v82_flying_controller.inc`. No framework changes or
generated-code edits were required. Sequel EXE disassembly establishes the
following equivalents to the reference's three in-function selections:

- `0x8002C590`, only caller `0x8003C5A4` during construction with no transform
  bank: report the absent wheel socket. The native loop handles six absent
  contacts; body construction and lifecycle run normally.
- Wrap `0x80041B0C`; replace its ground dispatch calls with original flight.
  Ground calls are `0x8003EDC4` plus the rolling `0x8003E774`/`0x8003E8C0`
  pair and native transformed variants, each guarded by exact caller. Flight
  executes once per wrapper even for the rolling pair. Restore pre-dispatch
  `+0x9C`, whose ground-only store the reference skips. Retail weapon/powerup
  timers continue after the substituted contribution.
- `0x80041EF8..0x80041FC4` only computes follow position and immediately enters
  the epilogue. Publish the reference's unsmoothed position after the native
  function, preserving preceding flag `0x08000000` and radar-jammer timer
  `+0x132` gates. No native consumer occurs between that range and the epilogue.
- Eight full-word-guarded function registrations were added to both configs;
  game C was regenerated. Existing eight instruction seams are unchanged.

Isolated PID 37380/port 4537, native resolution, fresh Y match:
- Type 76, object `0x8057F9A0`, one bank, 49 texture reservations. Exactly six
  absent-contact queries; all six wheel pointers stay zero.
- `y-first-flight.png` and `y-steered.png` were viewed. Acceleration/steering
  changed position from `(62700061,2865503,83127750)` to
  `(63738064,2865205,83374869)` and changed heading.
- The existing explicit debug fixture equipped the original special. Four
  shots produced 32 follow-on segments and collision flashes; the viewed
  `y-flying-fired.png` shows the flash and ammo 8. Through the save/load pass,
  seven shots produced 56 follow-on segments and all 63 segments retired;
  collision counter reached five, flight ticks 7083. Natural pickup acquisition
  is still a separate unverified path.
- Save/load during flight completed generation 7, pending=0, last_ok=1;
  `y-flight-restored.png` was viewed, with live game HUD and continued flight.
- Build plus all five title checks pass: plugin audit, flying controller,
  original specials, heap and actual external vehicle package. Flight unit
  checks cover lift/thrust/drag, angular columns, wrapping arithmetic, once-only
  rolling dispatch, native timers, follow gates and ground/contact isolation.
- Evidence is external `runs/vigilante82-20260930/y-flight-validation.json`.
  Slot 5 is early armed Y gameplay; slot 8 is flying/firing Y. Slot 11 is the
  location selector from this run. The isolated process was stopped afterward.

Next: Dreamland assets/overlay/arena selection using the existing reference
converters, then remaining roster behavior and player/NPC selection coverage.
Y's flight is no longer an unimplemented blocker. Full combined parity remains
unproven; quest persistence, natural pickups, transformations, all-car coverage,
Dreamland and presentation enhancements are still outstanding. Source local
and uncommitted; no assets/evidence were written into Git.

## Dreamland conversion and code generation, 2026-09-30

`tools/import_dreamland.py` now runs the unchanged reference asset pipeline
against verified owner media. The required loading-image encoder is external
psxavenc 0.3.1. Its Windows archive was checked against the official release
asset SHA256 `b44a4cf5a8c293a27182cddcca513428b26f0e143189ad53e29e6729b3d9a7a5`.
Output is external `LocalAppData/psxrecomp/imports/v82-dreamland`:

- `package/LEVELS/N64/DREAMLND.EXP`: 533692 bytes,
  SHA256 `10a54fa1f4b9f71d18df84f4a5cd44953141cae90ef9e6e385e0bced5829cfb7`.
- `package/LEVELS/N64/DREAMLND.DLL`: 14660 bytes,
  SHA256 `1308ffeddf54f8106468e915a08f2076c58ba5b824478f25445bbb1e6da35467`.
- Native `SHELL/LEVELSEL.TBL` has the additional preview at slot 18;
  `ARENAS.V8R` preserves case-sensitive `Levels\\N64\\DreamLnd.exp` and its
  primary export identity. Assets remain outside Git.

The reference's final overlay conversion normally operates on generated C#.
`tools/dreamland_overlay.py` instead applies the same mappings to input MIPS:
210 engine calls, 27 global accesses, the object layout, resource and callback
fields, native event 16 insertion, and one-time global-water initialization.
It imports the reference mapping definitions. A test passes actual instruction
operands through the unchanged reference object converter and checks every
result against the binary transformation. No other recompiler/runtime is used,
and generated source is never edited.

Thirteen N64 branch-likely instructions require explicit PS1 lowering. Appended
MIPS blocks preserve annulled delay slots; relocation records move with the
displaced instructions. New direct jump destinations and all 73 relocated code
pointers supply mechanical seed evidence. Early codegen without those event
table entries omitted three helper calls; `compile_overlay` now rejects every
missing entry or JAL call site. Final psxrecomp output covers 116 entries and
all 257 original JAL sites, with no reserved opcodes or interpreter fallbacks.
GCC compiles that output successfully. Existing codegen emits warnings about
fallthrough after unconditional CPS returns; inspected examples are unreachable
code after the correct tail transfer, not missing dispatch.

All six title checks pass with the owner ROM/package supplied. Dreamland tests
cover reference object-layout equivalence, relocation at aligned and unaligned
bases, event table identity, water jump relocation and both branch-likely paths.
The external report deliberately remains `playable: false`.

Next required work is runtime package/file loading plus the reference native
helpers. Reserved calls `8FFF0100..8FFF0118` retain hazard, water rendering,
contact effect, nearest object and three sound adapters. `8FFF011C` is one-time
water initialization; `8FFF0120` is the legacy model event adapter. Adapt the
complete `NATIVE_HELPERS` in `port_dreamland_overlay.py`; contact-effect update
also needs a guest callback. Confirm the global sound bank used by the sequel
and its Dreamland append data before wiring sound IDs. No Dreamland match has
been launched, and no additional runtime has been started during this step.
Use the existing arena registry/selector reference to connect loading; the
external test EXE's `80380000` base is a compilation probe, not a required
native allocation address. Live native DLL loading may relocate elsewhere.

## Dreamland play and native location selection, 2026-09-30

The preceding runtime TODOs are superseded: `v82_dreamland_plugin.cpp` and
`v82_dreamland_helpers.h` now load the verified arena, code and sound resources
and implement the reference native helper callbacks. File reads include byte,
block and whole-sector consumers; the latter returns its destination pointer,
which the native whole-file loader passes to realloc. The shared sound bank
preserves all 79 sequel entries and appends 15 decoded N64 samples. A guarded
port of the reference preview-bank reclamation retries a failed allocation
only at the initial shared match-bank transition.

Live failures found two input conversion omissions. All 36 N64 likely branches
are now lowered (13 primary-opcode and 23 REGIMM forms). Both callback-base
definitions feeding conditional joins are converted, matching the reference's
replacement of every original model-event base. Mechanical branch/jump roots
produce 449 native entries covering all 257 original calls. The current external
DLL is 15580 bytes, SHA256
`722e90d5b7a0ed9bc6e98ad6590a788f757fb113a16b0315239d242155287a22`.

`dreamland_menu.py` ports the reference's location-table redirection into the
input SHELL DLL: copies and relocates the 18 existing records, appends Dreamland,
retargets all three table consumers, expands the subtitle rectangle and moves
the background-table index after the 19 previews. The verified original disc
supplies the original-map entitlement, so both native count paths expose 19.
The runtime match override and its mod option are removed. The native menu owns
selection and passes the appended record's path to the unchanged level loader.
Prepared SHELL is 110892 bytes, SHA256
`d38c9baa0da7551e5334b746e308689ebbca3928c01a13533ff05caa28565c21`.

Isolated headless validation at 1x: viewed Dreamland with original Y, movement,
sound events and water updates over 3500 simulation frames. Later PID10004 on
4537 selected Dreamland through the native location menu (left from Meteor
Crater), loaded it without a forced path and reached gameplay. Viewed captures
`dreamland-native-selected.png` and `dreamland-native-menu-gameplay.png` are in
the external run directory. All seven title checks pass; the overlay check now
also tests relocation of all 18 copied records and all three table consumers at
two bases when `V82_DREAMLAND_SHELL` points to the owner's extracted SHELL DLL.

Remaining: original-car player/NPC menu integration, brief remaining-car checks,
natural pickup/transformation checks and human handoff. Houston/Y already have
the detailed earlier checks. The owner explicitly excludes dedicated retail-map
tests; Dreamland remains in the validation scope. Save requests succeed in the
menu but are refused during imported loading/play even with HLE scheduler forced;
the debug refusal text is generic, so its exact cause is not yet established.
Native overlay probe output is compiled, but current live runs still use the
standing installed-RAM bridge for loaded DLLs; native overlay-cache integration
remains to be wired. No framework edits or commits were made in this step.

## Combined native Arcade roster, 2026-09-30

`vehicle_menu.py` and `v82_native_selector.inc` adapt the reference player/NPC
selector seams into the verified input SHELL and trusted title guest callbacks
8FFF0300..8FFF035C. Forty-seven guarded seams retain native input, previews,
stat bars, color edits, acceptance and enemy quantities. Relocatable MIPS bridges
preserve stack and return registers; generated C is untouched. All 18 sequel
entries are exposed for the combined Arcade selector, followed by 13 originals.
Quest is deliberately left retail until original campaigns are implemented.
The development vehicle override is removed from the mod UI. An explicit
PSX_V82_TEST_VEHICLE environment fixture remains in debug builds only.

Current prepared SHELL: 111332 bytes, SHA256
`b7f26d9f91f8fe4c6a2cc2027fa4ebea4ca23fbc4937edabfaf4ba98b9d258bf`.
The menu implementation includes independent accepted-player and NPC identities,
retail proxy isolation, native preview banks/wheels, names, stats and colors.
Driver portraits and acceptance voices remain retail proxies for this handoff.
The extracted files previously named SELECTOR_*.PPM are native VLC records,
not composed RGB captures; do not pass them to the reference PPM reader.

Two ownership fixes were required during live browsing. Native temporary lists
use allocation-header word -8, so ownership is now a separate saved bitmap at
80320000, rather than a header magic word. Preview cleanup checks live ownership
before releasing editable groups and retires wheel-bank texture handles too.
The latter fixed an Out of VRAM screen at the thirteenth preview. The reference
player zoom applies only at 80106CD4; applying it to small NPC views hid Y.

Dreamland plus Houston/Y and three retail opponents initially failed SPU
allocation: the appended full-rate effects left only 18544 bytes after native
banks, before the two 45520-byte imported sound banks. `dreamland_audio.py`
filters and halves only N64 sample rates, adjusts pitches and loop positions,
and uses the existing external psxavenc encoder. It frees 51360 bytes. Current
shared bank: 371868 bytes, SHA256
`1a4f2887075b7b95a52fe1c42d7db0d0b42c3ef5d13f2b6f028f2a9dc31a3dc0`.
All original 79 entries/payload bytes remain intact. This is the quick-handoff
tradeoff; full-quality streaming is deferred, and an optional owner preference
question was sent before proceeding under the stated fast-handoff assumption.

Live native menu selection produced Houston and a Y opponent together on
Dreamland, with 3244 water updates and 35 sound events by the later observation.
The bank retained 24304 free SPU bytes. A gameplay save to slot 8 completed.
The earlier save-refusal investigation was a test error: slot 12 is outside the
valid 0..11 range, not evidence of an HLE or imported-content save limitation.
An uninterrupted follow-up constructed all thirteen imported menu previews and
returned to sequel slot 17. Chassey, Houston, Y and the sequel return captures
were viewed. The remaining cars have light preview/package coverage only.
Nine focused checks pass, including native navigation/hold/NPC/cleanup cases,
bitmap ownership, conversion relocation, audio duration/loop and existing
special/flight coverage. Retail-map tests remain explicitly out of scope.

See PLAYTEST.md for the human handoff and remaining presentation gaps. Live DLL
execution still uses the standing installed-RAM path; native overlay-cache
wiring is not claimed complete. No framework code was changed in this step.

## First human playtest: Survival SPU failure, 2026-09-30

The owner confirmed the full car carousel and reported proxy portraits plus
an Out of SPU screen. Read-only inspection of the failed session identified
Survival (mode 3), Meteor Crater, and accepted original Torque. The native SPU
table contained five retail car banks plus the Dreamland shared bank, leaving
25072 bytes when the arena requested 33920. Dreamland's sound provider had
incorrectly replaced SHARED/SOUNDS.SND on every arena.

The provider now checks the accepted location byte at GP+C28: only native menu
index 18 uses the extended bank. Other locations resolve the original disc file,
saving 51648 allocated bytes. The earlier allocation-failure reset is removed:
car banks are already live before shared sounds load, so resetting the allocator
could overwrite them. Failures instead expose v82.spu.allocation_failed and
v82.spu.failed_bytes counters while retaining the native failure result.

SHELL+669C always selects P1 (store at +7998). Its A1=2 argument means Survival's
enemy-editor policy, not P2. Fixing that mapping prevents the caller's subsequent
P2=-1 store from erasing Torque. NPC identity application now follows unique
retail proxies through native quantity expansion and respects the primary-player
count, matching the reference ApplySelectedNpcTypes behavior.

All nine focused checks pass, including retail-versus-Dreamland sound routing,
preservation of live allocations on failure, Survival identity, repeated NPC
quantities, empty slots and four-primary-player protection. A native-menu
Survival run selected Torque, reached Meteor Crater gameplay with 2141 native
vehicle events at observation, and completed a slot-5 save. Free SPU space was
52464 bytes. The existing Houston/Y Dreamland scenario then reached gameplay
again, with 700 water updates, 11 sound events, 24304 free SPU bytes, and no SPU
failure counter. Screenshots and saves remain in the external run directory.
This is startup regression coverage only; no broader retail-map testing was done.

Driver portraits and acceptance voices remain retail proxies. The first human
report does not change their known presentation-gap status. No framework edits,
asset changes, generated-code edits, commits or publication were needed.

## Human texture-corruption report, 2026-09-30

Reproduced with original Y and Loki/Molo/Beezwax/Sid in Survival on Dreamland,
after selecting Y through the original-car preview carousel and driving. Both
software and OpenGL initially had exact imported image bytes; the problem is
not specific to OpenGL. Source image bytes remained intact in guest RAM.

The first corrupting GP0 record at frame 40002 was `A0673640 00530136 00540137`,
submitted from draw-list address `000A0B54`, OT rank 3297. Its A0 opcode starts a
311x84 CPU-to-VRAM upload at (310,83), consuming subsequent polygons as pixels.
Repeated malformed records explain noisy UFO/HUD textures and transient broken
geometry. These were draw-list packets, not texture-loader DMA transfers.

The pinned reference already rejects transfer opcodes and incomplete commands
inside gameplay draw lists. `v82_draw_packets.inc` ports that validation to a
word-guarded native DrawOTag entry hook at 8005C57C. It clears only the offending
node's word count, retaining its next link and native SDK/DMA execution. Scope
is a match with imported vehicle reservations or accepted Dreamland index 18.
Retail-only matches and an inactive mod keep their original packets. Counters
`v82.draw.rejected*` distinguish transfer, truncation, polyline and unknown-op
rejections. This deliberately matches reference omission of malformed geometry;
it does not reconstruct the intended primitive or replace the GPU interpreter.

Ten focused checks pass, including the captured A0 packet, valid colour words
that resemble opcodes, multi-command nodes, mandatory polyline vertices and
activation isolation. Use a fresh match: an already corrupted gameplay state
contains damaged VRAM and cannot be repaired by filtering future packets.
Evidence and reproduction scripts remain in the external 20260930 run folder.

After the guard, the software reproduction monitored Y's full 64x256 index
image continuously through frame 9628 with no mismatch. At frame 13089 all
26 claimed image rectangles and 26 CLUTs matched their source bytes, with
7118 rejected transfer packets. Hidden OpenGL repeated the menu/drive case;
at frame 6628 all 25 claimed images and 25 CLUTs matched, with 4105 rejected
transfer packets. Both screenshots were inspected. Final executable SHA256:
`620161912ab111c5889ac1e3a2a557203178cf8e3f1b01a96781b19224c6517b`.

A separate pre-fix OpenGL run reached a teardown callback failure (8C9A0080,
A0=1000, RA=80030724, frame 4487). Its dump is preserved externally; the packet
guard alone is not evidence that this independent lifecycle problem is fixed.

## Dreamland water recovery, 2026-09-30

The owner state showed player Leprechaun and an AI car repeating native sinking
callback 80038F7C far below the water plane. Its event-2 completion sends arena
event 19; the converted arena only implements 0..18 and returned zero without
moving the vehicle. Native timers were working, but each completion re-entered
water at the unchanged position.

The title's water helper now installs a saved arena-callback wrapper. Existing
events delegate to the converted overlay. Event 19 resolves the authored
standing starts (-1..-4) through native 80030250, uses the player's own start
or the closest start for AI, clears sinking velocity, and updates the native
transform through 8002CF74. The native completion still restores driving and
camera state. Positive start IDs are reinforcement entrances on sloped edges
and are deliberately excluded. No coordinates, health changes, framework
patches, generated-code edits, or new donor bytes are required.

The reference's separate lethal-water policy was evaluated but is not enabled:
it ends a Survival match instead of completing the requested recovery.

All ten title checks pass, including event forwarding, saved callback ownership,
authored-start validation, transform/velocity repair and unchanged health.
The captured owner state recovered both cars. An isolated repeated fall entered
80038F7C and returned to 8003C118 on dry ground; another 600 frames, driving,
and a save/load round trip passed without another recovery. That longer fixture
raised only its test player's health to prevent AI fire ending Survival; the
unmodified owner-state recovery also retained its original 750 health.
External evidence: `water-recovery-proof.json` and `verify-water.py` in the
20260930 run folder. Executable SHA256:
`6163c27081cd0719ccdcbdf932d6b2a3fa9556e917ec4501dc8b0ba67c52972c`.

## Original portraits, voices and names

`tools/import_original_presentation.py` reads the verified owner v1.0 and sequel
discs. `V8IDENTITY.DAT` holds thirteen original VLC portrait records, two faction
layers, a resident selector bank and the thirteen source acceptance samples.
`V8RESULTS.DAT` contains 26 native XA streams (defeat, then victory). Both are
required, hashed resources of the original-cars package; no donor bytes enter Git.

`v82_presentation.inc` connects the selector's native VLC decode to these images,
uploads acceptance speech into two bounded reusable SPU slots with original
pitch, and supplies original names to native object-name function 80038398.
That covers the previously blank local-player destruction notification as well
as imported opponents. The retail path is unchanged. Names and portrait storage
live at 80330000 and 80340000, below the existing 80400000 enhancement heap.

The native VLC expansion of Y's portrait is 119936 bytes. Retail scratch at
8009CA68 overwrote menu scene data, eventually reaching a null scene callback.
`vehicle_menu.py` now word-guards and relocates the two scratch-pointer
instructions at SHELL+10664/+10668 to 80200000, reserving one MiB below metadata.
Decoding remains native; converted MIPS is prepared input, never edited output C.
Menu conversion regression verifies both words at two overlay relocation bases.

Result formatting at 8005FA9C/RA80013268 selects an original Dxx/Vxx path;
80018210 supplies its native sector descriptor, and 8001DA14/RA80013294 selects
channel zero. Framework `psx_mod_append_disc_extent` mounts verified immutable
2336-byte Mode-2 records after the complete base disc. Native CD/SDK/XA execution
then owns playback. The reference extractor's rejected realtime-audio filler
cannot notify the native CdReady callback under PSX hardware semantics. The
importer instead uses the retail SHARED/*.XA non-audio filler subheader
01 00 00 00, preserving every speech sector and its eight-sector cadence.
Native callback 8001D9B0 reads each data-sector MSF and pauses at file_end-1.

Validation: all thirteen title tests and framework mod_runtime_test pass.
The carousel exercised original portrait/preview transitions and retail return;
Houston and Y acceptance uploads match original bytes and pitch in SPU RAM.
Y's actual native destruction displays `"Y" the Alien destroyed!`; the native
defeat flow plays all 29 speech sectors and pauses at final LBA 308503, without
reading into the next character's voice. Unit coverage checks all thirteen
identities/portrait mappings, both voice slots and all 26 outcome mappings.
Runtime captures and audio evidence remain external in the 20260930 run folder.
The opt-in debug fixture PSX_V82_TEST_DEFEAT invokes native lethal damage after
120 player updates; it is absent from release-only builds and unset for handoff.

Prepared hashes: identity a90a68f89d46f4b2006aa00c1d69938d49de6057c0dc4c0eb077def5acc40365;
outcomes 6f1aa7c0765c1cb79e467fe24d04d5784309d33fe093a047ab756216493ba0ec;
SHELL 4e69045eb4b8ca9d5497f38b83576bbbae2674ce4b2f5ff0f948472f0a04e246.
Older states are incompatible with these resource and codegen fingerprints.
The previously recorded callback failure 8C9A0080/A0=1000/RA80030724 also occurred
after a defeat; presentation validation does not claim that lifecycle bug fixed.

## Original Slick Clyde lightning

The reported slot-02 weapon used native 80049880, an ammo-only fallback. The
pinned reference registry enables only Houston/Y original callbacks. Clyde is
now capability 1, with five title-owned guest callbacks for weapon, launched
device, bolt, hit and sparks. Original owner-disc code at 800380C8..80038A08
supplies the complete behavior; the reference's C device helper omits its
launch and three strikes. No generated code or framework change is involved.
The source bank owns the appended COMMON roots 14/15 and MAIN.SND samples
37/41. Source fire sample 0 and voice 1 stay in their original positions.

ANM decode previously discarded the terminal timestamp and compiler code summed
all frame timestamps. Native engines compare absolute cycle timestamps: Clyde's
80-tick launch became 1640 ticks. The codec now retains the terminal time and
frame destination, including tail holds and interior loop targets. The package
passes semantic roundtrip, and synthetic binary tests independently preserve
an 80-tick cycle, a 30-tick held tail, and a nonzero loop destination.

All 15 checks pass, including failed allocation without ammo/voice, fire and
voice calls, matrix-derived launch velocity, empty-weapon removal, three strikes,
damage ownership and retirement. A fresh native match records launch at fire+80,
strikes at launch+30/60/90, device retirement at launch+121 and all three bolt
retirements. Screenshots show the actual lightning. The SPU event ring confirms
key-ons for the original fire, voice, thunder and impact samples at their loaded
addresses, with nonzero volume. Natural target testing recorded one lightning
vehicle hit and front-part health 628 -> 328 (300 damage); native collision and
damage code owns this result. Six bounded firing-time cases show moving targets
can evade the strikes. Root health stays fixed for these intact two-part cars;
native damage updates front/rear parts at +F8/+FC, which must be read separately.

External evidence is in the 20260930 run folder: lightning-natural-shot-checks.json,
lightning-keyon-evidence.json, lightning-strike-110.png. The stationary-target
experiment is separate diagnostic evidence, not the natural damage validation.
New CUSTOM.EXP: 1607376 bytes, SHA256
6156c4a4a8da9dad4f08f46b20c90adabc186b2de3d3bc97e2f13e58cb0ee8cd.
Registry: 2395 bytes, SHA256
07a7b77015e4cbcaead50b27b3257b5ac9051cc52a8c953b698cfdf9a2cd18e5.
Fresh matches are required. The other ten original specials remain unimplemented;
their current model/presentation support must not be described as weapon parity.

The final three-round runtime check consumed ammo 3 -> 2 -> 1 -> empty weapon,
launched three devices and nine strikes, retired all three devices and nine
bolts, and returned the native active-special count to zero. Final executable:
73d4e98d8d06a1c0da8e08aae38cfdb316029894d89337711a7003c688bcc66b.
The owner handoff runs at 1x OpenGL with all opt-in test fixtures disabled;
the prior executable and manifests are backed up externally. Beads eio.12.3
records this fix; eio.12.4 tracks the ten outstanding original specials.

## Complete original special roster, 2026-09-30

Beads eio.12.4 supersedes the ten-special outstanding list above. All thirteen
original capabilities are now enabled. `v82_special_roster.inc` supplies the
remaining ten weapon families and their helper callbacks through psxrecomp's
reserved mod function namespace. The existing Clyde/Houston/Y paths remain.
No generated source or framework behavior was changed for this implementation.

The authorized `vehicle_init_joints.c` provides the starting behavior. Its
incomplete or incorrect paths were checked against owner executable data:
Convoy's projectile is 8003A084, Torque's pulse is 8003EFC8, Loki has three
rounds and a launch/homing/cluster path, and Boogie's star callback is 80038A0C.
The sequel ground-contact bit corresponding to original 0x00200000 is
0x20000000 (8002FC98 -> 8003F25C). Native callbacks, attachment fields, events,
physics/audio calls and terrain addresses use the sequel ABI.

Whole-common-bank loading exceeded the native SPU and VRAM budgets. The importer
now emits 47 independently owned banks: 38 vehicle/preview/transformation banks
and nine trimmed effect banks. A 1334-byte `V8FX` registry extension identifies
the single sparse SFX source and maps original effect kinds into each trimmed
bank. The runtime reserves selected vehicles' effects and deduplicates identical
textures. Direct-color TIMs use their inline image block and no CLUT; Chassey's
1x1 effect exposed and now regression-tests that reservation path.

Common original SFX loads per sample into SPU RAM. On pressure, the cache
reclaims samples only after their voices stop, with the native two-audio-tick
key-on protection. Guest state owns cache handles, and match teardown frees
them. Vehicle voice banks retain their separate native ownership. Unit tests
exercise cache reuse, full-budget refusal, playing-sample protection and cleanup.

Loki's attached missile already belongs to the update list. Launch adds only
rendering and world ownership; registering it a second time left a stale tick
entry and emitted repeated clusters after destruction. The fixed live run
produced one launch, five bomblets, one missile retirement and five bomblet
retirements. Chassey/Loki/Boogie release the active-special gate in destruction
callbacks, including owner destruction before normal expiry. Convoy/Loki refill
events restore the visible ammunition racks.

Validation:

- All 16 CTest checks pass, including the external 13-car/47-bank package,
  malformed registry rejection, plugin audit and all prior title regressions.
- All ten new families fired in isolated native matches. Chassey retired nine
  bolts; Dave retired three tornadoes and registered 16 vehicle hits; Convoy
  retired one horn; Boogie emitted 15 stars, launched/retired 11 before the orb
  expired; Beezwax registered 21 hits and retired its swarm; Molo and Sid
  retired all 18 and five projectiles respectively in held-trigger checks.
- Sheila's deeper run fired/retired all 50 rounds, registered 11 vehicle hits
  and removed its empty weapon. SPU KEYON events show exactly one original
  voice at 0x72990 plus 50 gun sounds at 0x720D0, with nonzero volume.
- Torque activated and completed in a native match. Tests verify actual guest
  terrain changes and byte-exact restoration after 317 ticks and early teardown,
  plus map-edge bounds and rejection while airborne.
- Tests also cover all ten init/fire/voice/ammo paths, allocation-failure cleanup,
  independent CPU call frames, Sheila's cadence, visible-rack refill and early
  active-special release. Light tests are not a claim of full visual/AI parity;
  natural pickups and weapon feel remain useful owner playtests.

External evidence: `runs/vigilante82-20260930/all-specials-*-counters.json`,
`all-specials-sheila-full-magazine.json` and accompanying screenshots. New
registry SHA256: d7b035ed2fb307b2302cc4c3b30537f117808e53d8264f3cdd0cb6c0e746446c;
archive SHA256: b444b92dc538e36649551929c2aedb6a3bdc700ced6dfa1b212f6b511f1ab2e3.
The owner handoff stays at 1x OpenGL without test fixtures. Fresh matches are
required after the prepared-package change; no donor bytes enter Git.

Handoff executable SHA256:
7d71019734d86c52089ad23233d5529475914e203986d9d5bcfcb197995e4b18.
The previous executable, mod state, vehicle manifest and playtest notes are
backed up in the external playtest's `backups/before-all-specials-20260930-204327`.

## Dreamland effect cleanup repair, 2026-09-30

The owner's freeze at frame 109852 resolves `beads-eio.12.2`: native explosion
object `80621790` had an identity matrix at +10, overwriting its child pointer
with `1000`. Animation completion entered native teardown, followed that link
and tried to call the word at address 1000 (`8C9A0080`, return PC `80030724`).
This explains the same signature previously observed during Dreamland/results.

The input overlay converter recognized allocator/clone return values as object
pointers but omitted native effect factories `8004E128` and `8004DE54`. Their
return contracts are verified against the owner's SLUS-00868. The castle
projectile impact therefore retained old position/matrix offsets +48/+10;
the sequel requires +4C/+20. Projectile smoke similarly retained flags +0 and
roll +44 instead of +4/+48. The converter now tracks these returned objects
through register moves and the following call's delay slot. Exactly five
additional field accesses change; the rest still matches the pinned reference.
No generated source or generic runtime code changes, and no invalid-pointer
suppression is involved.

Validation: all 15 focused title/plugin checks pass with owner media supplied.
The regression replays the actual converted matrix and smoke field accesses,
checking that object links/callbacks survive and intended fields are updated.
Overlay compilation covers 449 entries and all 257 call sites, with no missing
entries/calls or interpreter fallback. A fresh isolated Boogie/Dreamland match
used the fixed bytes at its actual relocated base. Cannon fire, driving, water
recovery and defeat/results continued; the run reached frame 41588. Explosion
`80622738` retained child=0 and its matrix at +20, and its allocation-map bit
was cleared by normal cleanup. This is a bounded lifecycle check, not an
exhaustive arena soak. Captures, RAM evidence and the retirement check remain
external in `runs/vigilante82-20260930/dreamland-*`.

The corrected 15580-byte DLL is prepared externally under
`imports/v82-dreamland-object-layout/package/LEVELS/N64/DREAMLND.DLL`, SHA256
`31ac01e0e6fce03fb0e83e045da28861219d129484b146ebf84ca9909d1b9d10`.
The manifest pins this digest; other prepared assets and the executable are
unchanged. Start a fresh match after the resource update. Owner handoff remains
1x OpenGL with test fixtures disabled.

## Mushroom projectile conversion and repeatable stress checks, 2026-09-30

`beads-eio.12.5` is the subsequent freeze reported while Convoy was in water.
The live rings distinguish it from recovery: gameplay stopped at frame 107630,
693 vblanks after the last recovery. Mushroom setup wrote `8075F8BC` over
projectile `805DF9A8`'s callback `8075F89C` at overlay offset 19CC. The intended
flags OR 20 used old object offset 0 rather than native +4. The modified
callback skipped the projectile prologue and its epilogue read the stale
`Levels\\N64\\DreamLnd.exp` stack buffer as a return address (`445C3436`).
The resulting address exception entered the BIOS error loop. Water was where
the camera stopped, not the cause.

The converter now recognizes the local `DreamProjectileSpawn` return and the
host nearest-object/contact-effect returns. The two mushroom routines and
their projectile callbacks also identify +84 as a target-object pointer;
other actors do not inherit that interpretation. Twenty-one additional field
accesses now use native flags, damage, matrix, world position, sound-voice and
target-position offsets. The prior explosion repair remains intact. The
prepared 15580-byte DLL is external `imports/v82-dreamland-projectiles`, SHA256
`3245bda08be9ac658dec64f86adf7607fb030bf837251f8d131dcff07c36d5f5`.

Regression coverage now includes 4163 deterministic relocation/flag cases.
An aligned probe base can hide the callback OR because the relevant address
bit is already set; randomized aligned bases and arbitrary initial flags
assert both callback preservation and the intended flag update. The strict
reference comparison explicitly accounts for each locally verified difference.
All 16 CTest checks pass with owner media supplied; overlay code generation
still covers all 449 entries and 257 call sites.

For actual gameplay coverage, debug builds have an opt-in
`PSX_V82_TEST_DREAMLAND_PROJECTILES=1` fixture. It triggers native collision
event 3 against each ready authored mushroom once, using the normal damage,
clone, targeting and scheduling routines. It checks the active-object list for
callbacks accidentally pointing into a projectile's body. The old overlay
aborted this check; the corrected Convoy run completed 11 good and 3 bad
mushroom reactions, 14 nearest-target queries and 51 valid-projectile
observations, then continued through water recoveries and defeat/results.
Observations can count an active projectile more than once; they are not a
projectile-spawn count.

To repeat: launch a fresh isolated headless debug runtime on port 4537, with
the environment variable above and both prepared content packages enabled;
then run `py -3 tests/run_dreamland_stress.py --port 4537`. The driver routes
the native menus to Dreamland, requires both mushroom families to complete,
then requires 1000 further water/gameplay updates. It refuses owner port 4535.
The fixture is absent from builds with `PSX_NO_DEBUG_TOOLS` and disabled by
default. This is targeted property/stress coverage, not unrestricted fuzzing
or proof that every arena event is correct. Owner handoff clears the fixture
environment variable and stays at 1x; a fresh match is required.
