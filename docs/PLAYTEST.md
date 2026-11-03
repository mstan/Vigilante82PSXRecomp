# Combined Arcade playtest

## v0.1.4 dependency refresh (2026-11-03)

Validated code is title `e57c6fdae681837eca97f4d1f0ed46aa196a8c5f`, framework
`d35f13c943b206d19d54f2912d73c6546806faed`, UI
`dd47b1887b1858473bb42c38f4307f0eb1b07e60`, recomp-net
`7a4d330ecf995b881d167234da3c0b01675fd512` and rbengine
`28a2a472986a7d692d0378a22e756b0936413507`. Documentation-only commits after
the tested title revision do not change the executable. Its SHA-256 is
`4ca71f3b371d4042878f2dc54363cd9980a6bd7d231e2afb7fb39d690884cb98`.
Native Clang 22.1.8 Release, `PSX_DEBUG_TOOLS=OFF`, PGXP flavor 2, both BIOS
backends and all 28 title checks pass, including the current owner vehicle
fixture and actual sequel-disc Resident Loading catalog check.

The latest framework deliberately declines persistent Windows file identities.
The media-receipt test now verifies the supported no-receipt/full-verification
path there; it retains the existing receipt/invalidation checks on supported
platforms. No production identity check was weakened. Production TCP input was
also found to acknowledge but discard P1/P2 overrides; the framework fix and its
debug/lean regression test are included in the pinned revision.

The actual ZIP was extracted into a fresh profile. Hidden launcher captures
show Internal Resolution 4x, Widescreen checked and Fit to Window. The game then
used only `C:/Windows/System32;C:/Windows` in PATH, dummy audio and private
cards/states. Native menus reached Arcade/Meteor Crater; acceleration/steering
moved the vehicle through the arena. Screenshots show widened terrain and HUD
placement; `video_info` reports requested/effective/hr scale 4, a 4096x2048 FBO,
and 1280x720 output. Current-build save/load completed generations 1/2 with
`last_ok=1`, and gameplay continued for 2,505 VBlanks after restoration. A warm
process loaded the same current-build state and continued another 1,402 VBlanks.
Both processes exited 0. No owner focus or live saved data was changed.

The old release contains 1,322 GCC cache pairs but no raw capture inputs. Its
old generated C/DLLs were not retagged. Ten retained additive captures were
regenerated for `cg18_3d3adb8c_gc07053ec0_f2`: 78 GCC and 78 TinyCC shards passed.
Fresh native output from the actual compiler-free runs expands the shipped
TinyCC set to 144 pairs, alongside those 78 GCC pairs. The bundled Python and
TinyCC were actually used; one gameplay sample reports 33 native region loads,
710 registered functions, 6,831,690 native overlay dispatches and 2,330,824
interpreter fallbacks, with zero stale blocks or candidate overflow. These
counters are not a proof of complete arena/vehicle coverage. Code outside the
retained cache still uses the existing verified capture/compile fallback.

The mixed menu/gameplay host-API completion sample was 4,096 presents over
69,444.5 ms (58.98 Hz), p95 26,220 us and p99 33,738 us on the shared PC. This
is a runtime sanity observation, not a matched performance comparison. The
warm startup reports disc verification 28,578,301 us, runtime commit 28,700,660
us, overlay join 34,259 us and Resident Loading activation 683,845 us. The disc
was on the owner's network share; no local-disc timing or startup win is claimed.

Evidence: `G:/PSXRefresh-20261103/v82-final-qa` (native logs, controls, screenshots,
private states and diagnostics), earlier fresh launcher images under
`G:/PSXRefresh-20261103/v82-qa`, and `v82-ctest-release.log`/
`v82-package-release.log` in the task root. Source issue: `beads-eio.12.26`.
Two-peer netplay, all other arenas/content combinations and Linux remain outside
this refresh's runtime qualification; prior limitations below remain applicable.

## Prepared-media receipts

Verified imports remain in the external `psxrecomp/imports` cache. The title
records a receipt after successful preparation, with the importer revision,
selected media and CUE dependencies, extracted CHD tracks, expected resources
and filesystem identities. An unchanged receipt returns prepared paths without
starting the conversion helper or rehashing source discs. Framework resolution
still validates every output's expected SHA and retains immutable bytes for
the launch and netplay fingerprint checks. Unsupported filesystem identities,
missing files, changed sources or tools, and malformed receipts take the normal
verification path. A receipt is never permission to bypass resource integrity.

Packaging writes `media-tools/preparer-revision` from the recursively imported
Python sources, encoder bytes and runtime package versions. Cold receipt
publication checks output SHA between identity observations; warm probes use
identities only, followed by authoritative framework verification. The existing
`mod_counters` command exposes `v82.media.helper_spawns`,
`v82.media.receipt_hits` and `v82.media.receipt_misses`; hit/miss counts measure
probe calls rather than launches. No runtime log files are introduced.

### Launch preparation spike accepted (2026-10-07)

The owner tested the staged build, reported that it "seems pretty snappy",
and requested closing the spike. Preparation starts when the selected disc or
mod inputs change. Unchanged launches load verified cached content before the
launcher opens and reuse that plan on Play without a preparation screen.

The accepted executable used title `24cf8e34`, framework `15d71183`, and the
feature-only recomp-ui integration `54fc1fd` on the existing `d3151bf` chain.
Its SHA-256 is
`3cc60b3722731f189cfc3dc7ed8e239fc248940681e5add81950461142e85bd6`.
With actual CUE/N64 media, a fresh warm process ran zero conversion helpers,
zero disc hashes and zero launcher preparation jobs. Verified preloading took
1.04 seconds; the reused runtime commit took 44.9 ms. These are startup
measurements, not gameplay frame-rate results.

Validation passed 25 title checks, four integrated framework checks, five UI
checks and eight hidden launcher scenarios. One optional Dreamland subcase was
skipped. The accepted build retains the sky, complete left HUD, launcher art,
disc persistence and render-replay admission fixes; Resident Loading is the
loading option. Settings and memory cards were preserved.

Acceptance was recorded before integration. The main-branch dependency pins
were public framework `d3b22491` and recomp-ui `1a8ae64b` (bumped for v0.1.3 to `7cddab82` and `0546fbcb`), preserving their newer
upstream changes as well as the accepted fixes. A clean title checkout at
`252889e0` built against those pinned submodules with strict BIOS stamp checks
and passed all 25 registered checks; the same optional Dreamland subcase was
skipped. Framework and launcher integration passed 19 and six focused checks,
respectively, plus the earlier eight launcher lifecycle probes.

The new executable SHA-256 is
`f2e6eb464b1a203c1516845a17fd2f8beb016b102b9cdc052a3ad5b6cb578165`.
After establishing receipts for its newly packaged importer, a fresh process
used zero conversion helpers, zero disc hashes and no preparation screen.
Verified preload took 1.03 seconds and the reused runtime commit took 46.3 ms;
the game advanced into its rendered introduction. This qualifies the merged
dependency build, without extending the earlier gameplay acceptance.

The separate periodic gameplay hitch remains tracked in `beads-eio.12.18`.
Full CHD extraction and the parked framework work remain outside this spike's
validation. No release was made as part of this landing.

## Two-player netplay handoff

Owner validation, 2026-10-02: the isolated LAN / Direct IP pair exposes original
cars to both native two-player selectors, loads both into a match, plays their
correct selection voices, and offers the original songs in the pause menu.
All 21 title checks pass with the owner media fixture. The separate latest
offline Meteor Crater check was accepted by the owner without an additional
stage-specific patch.

Launch normally into the shared launcher and open **NETPLAY**. Both players
use the same build and complete 17-track USA sequel CUE. Assign each computer's
controller under **PLAYER 1 / NETPLAY**. Host an online room or LAN lobby, select both
content packages under **Mods**, and have the other player join. Each computer
selects its own original disc and N64 ROM; preparation is automatic.

Choose **2 PLAYER > VERSUS**, select Super Dreamland 64, then choose one original
car on each peer. Check that each player drives only their own car, fires a
special, and can return to the room and start another match. This is the next
human check; the automated paired run covered native Versus gameplay, while
resource-backed host-plan negotiation has unit coverage. LAN / Direct IP
preserves the selected content and verifies that both peers resolve the same
content. The host's LAN Mods selection also travels with the start message.

Rendering stays at 1x. Music and widescreen are now optional packages; an online
match with those newly enabled packages still needs a paired human check.

## Netplay unsplit view

The current candidate uses the enhanced native-wide renderer and imports
High Frame Rate into the combined cars/music build. Multiplayer supports
fixed 16:9 and 21:9 chosen before play; window resizing only letterboxes.
Netplay render passes replay the local camera after the native DrawSync,
with the authoritative CPU VRAM untouched. Rollback resimulation skips them.
Both cameras' full wide surfaces render directly through OpenGL, including
both framebuffer bands; the native split divider stays outside the local view.

Two isolated Meteor Crater peers at 144 Hz completed 1,088 and 935 extra passes,
with zero verify mismatches, aborts, VRAM leaks or span failures. Captures verify
the same local car in native and intermediate frames and no split divider.
These runs loaded an isolated shared save and are not a fresh-session hash
confirmation result. A fresh 21:9 Meteor Versus session selected Wonderwagon
and the imported Luxo Saucer through both native selectors. It completed 363/380
extra passes with zero verify mismatches and confirmed ticks through 24,904.
All 682 compared core-state checkpoints matched through tick 24,896.
Both local captures are 560×240 with the correct car and no divider.
Human validation of motion, HUD, pause/quit and a second match remains pending.
The visible pair in `v82-enhanced-net-20261002` uses fixed 21:9, a 144 Hz
presentation target and native 1x OpenGL. Expensive pass verification is off
for human performance review; the automated runs above had it on.

Both peers now get their own full-screen fixed 16:9 or 21:9 camera in a two-player
match. Vehicles, radar and HUD retain their proportions. Selection and pause
menus remain complete; the split-direction option is hidden during netplay.
Offline local multiplayer retains that option. Different window shapes
letterbox the chosen view, keeping both peers' game geometry identical.

Use the new build on both peers and start a fresh match. The frustum hooks
change the generated-code fingerprint, so earlier candidate states are not
compatible. This includes the completed original-car pickup correction.

The earlier projection candidate's validation used actual NVIDIA OpenGL captures
at native 1x: separate views at
Nuclear Plant, nearby silo/wall edges, both players driving and pausing, the
whole quit-confirmation menu, and a new match on Meteor Crater. All 1,582
compared core checkpoints matched through simulation tick 54,016. The title's
21 checks pass; the framework tests exercise complete shared glyph/shadow
groups in both halves and identity behavior outside unsplit. Source-owned GL
capture checks pass at 1x and 4x without changing canonical CPU VRAM.

An online room with the imported roster/Dreamland, adverse WAN conditions,
and the complete death/result sequence still need playtesting. A diagnostic
low-health fixture did not trigger the native death sequence and is not counted
as result-screen validation. Its forced RAM writes and state reload are
excluded from the synchronized gameplay checkpoint result above.

## Widescreen and source-media handoff

The owner accepted the terrain improvement in `v82-fog-batch-order-20261001`.
The earlier signed-halfword ray correction alone was rejected. The accepted
change fixes OpenGL batch ordering: a native fog strip suppressed the wide
mirror while earlier terrain triangles were still queued. The renderer now
drains prior geometry before suppression and finishes the strip's canonical
triangles before drawing its wide overlay. Guest packets are unchanged.

The original renderer fails the source-owned hidden GL pixel regression; the
correction passes 133 checks at both 1x and 4x. Earlier software-renderer capture
matches establish repeatability only. Current rendering checks use actual hidden
OpenGL, explicit hexadecimal input strings, and states matching the hook metadata.

The owner then reported nearby silos/walls vanishing at the screen edges and
outer scenery loss during the death camera. The new candidate widens the native
triangle subdivision walkers' horizontal rejection bounds. These still tested
0..320 after whole-object visibility had already admitted the model. The fix
changes comparison results and transient coordinate copies; native vertex
records, emitted screen coordinates, vertical/depth tests and NCLIP stay native.

Paused OpenGL 1x comparisons restore both silo edges and nearby walls. The 4:3
centre stays unchanged (one animation pixel in one view). A verified diagnostic
whole-object bypass did not restore those surfaces; no radius-slack workaround
is included. Death-camera captures also show restored silo faces. Automated
checks cover all eight subdivision paths at 16:9, 21:9 and 32:9 bounds, including
objects wholly outside either edge, unchanged edge lengths and non-world guards.
The final build was checked under actual hidden OpenGL at all three aspects.
Temporary object bypass, camera hold and comparison switches are test-only and
absent from the candidate.

The owner accepted the nearby-object and death-camera culling correction in
`v82-subdivision-culling-20261001`. Start a fresh match after hook metadata
changes; older states are incompatible. The subdivision comparison itself did
not independently establish a fix for every pale distant terrain gap.
Precise NCLIP remains off; broader coordinate expansion remains deferred.
Evidence lives under local runs/v82-object-culling-20261001 and the earlier
runs/v82-gl-investigation-20261001. Changes remain uncommitted.

The earlier behavior is backed up by commit `6d8de57` and branch
`backup/v82-widescreen-before-distance-fix-20261001`.


Enable **Widescreen** and **Original Vigilante 8 soundtrack** in Mods.
Use **Fit to Window**, then resize the game from 4:3 through 32:9. In Dreamland,
check terrain and scenery at both edges, including the rear camera, HUD,
pause/resume, and returning to the native menus. Fixed 16:9 costs less terrain
work than Fit, whose culling covers the maximum view. Keep internal scale at 1x.

Press Start and select **Track** with Up/Down. Left/Right cycles through all
28 tracks when the album is **Both games**; the original names begin with
**V8:**. Check audibility and volume, change songs, then leave rotation running.
The last original track wraps to the first sequel track in the selector.
Album selection is in the launcher's mod options. Start a fresh match rather
than loading states from before the new hook configuration.

The import, native menu table/lead-out, culling boundaries, packet arenas,
HUD/sky tagging, verified PCM mapping and failed-playlist rollback have
automated checks. Human listening and visual checks complement those tests.

The isolated 32:9 Dreamland run matched all 330 presented images on a
same-state control replay. Pause selection reached original songs and wrapped
from the last song to the first sequel song. Seeking the isolated CD controller
to the final five seconds also let the native playback callback wrap from
track 29 to track 2. A gameplay save/load restored active CD playback. The title
suite passed 18 checks; one separate donor-package fixture was skipped.

## Original vehicle transformation pickups

The hovercraft freeze was reproduced on Sid Burn at the wheel rebuild midpoint.
Native texture decompression at `0x80021064` reserves `0xC70` stack bytes, while
vehicle physics uses the 1 KiB scratchpad stack. Imported replacement parts have
compressed textures, so that call underflowed the stack and returned to
`0xFFFFFFFF`. The title hook gives only package-owned compressed texture loads
entered on scratchpad a temporary 8 KiB guest RAM stack. The native decoder and
GPU uploads run normally; the caller stack is restored and the allocation freed.

Ground originals retain the sequel's hover, water and snow parts. Y's existing
capability guard rejects these pickups before either participant is changed,
leaving them available and retaining the saucer's flight behavior. New hook
metadata requires a fresh match. The shared launcher remains the entry point.

The isolated regression uses actual hidden OpenGL, the native dropped-pickup
constructor and collision callback, then native return-to-normal requests.
It exercises all three modes and a repeat pickup. It does not establish natural
pickup expiry timing. Debug-only `PSX_V82_TEST_TRANSFORM=1` enables the fixture;
normal launchers clear it. Test health maintenance and a shared retail opponent
bank (to stay within the native VRAM budget) are confined to that opt-in.
All 12 ground originals passed four pickup/restoration cycles each. Y rejected
all four pickups and four direct transform requests, retaining zero wheel
contacts. All 20 title checks passed, including the owner-package fixture.
Evidence and per-driver screenshots: local `runs/v82-original-transform-20261001`.

## Offline combined content

This build runs under psxrecomp at native 1x resolution, with 4K disabled.
Select the original V8 USA v1.0 disc image once for Original vehicles/music,
and the N64 USA ROM for Dreamland. The sequel disc belongs on the main page.
The launcher prepares the enabled content automatically; no loose-file inputs.
The original cars are separate entries in the native player and enemy menus.
Survival also retains the selected original car. The first human-reported
Survival startup failure is fixed: retail maps now load their original shared
sound bank, reserving Dreamland's additional sounds for Dreamland only.

The follow-up UFO/HUD texture corruption now has the reference's gameplay
draw-packet validation. Malformed packets are omitted before they can turn
subsequent geometry into texture uploads. Start a fresh match after updating;
loading an already corrupted save restores its damaged texture memory.

Dreamland water recovery now returns cars to the authored standing player
starts. The imported arena lacked the sequel's recovery event 19, leaving
cars sinking below the map indefinitely. Survival continues after recovery;
the fix does not turn a fall into an automatic death. The owner confirmed the
repeat-fall recovery works in the previous handoff.

Original drivers now have their own portraits, selection voices, victory/defeat
speech and gameplay names. Please check the portrait and spoken line when
accepting Houston or Y, then the name in the destruction notification and the
voice at the result screen. Start a fresh match: the prepared-content hashes
and generated hook configuration changed, so older saves are incompatible.

Slick Clyde (original slot 02, '70 Clydesdale) now uses his original lightning
weapon. Please check the spoken firing line, charging/launch effect and three
strikes against another car. Internal resolution remains 1x. Start a new match;
the original-car archive fingerprint changed with the animation timing fix.
All thirteen original cars now have their original specials. The new additions
are Chassey's radar burst, Sheila's gun, Torque's quake, Dave's tornadoes,
Convoy's horns, Loki's missile cluster, Boogie's disco stars, Beezwax's swarm,
Molo's exhaust and Sid's fireballs. Their own firing voices are connected.
Start a fresh match after this update; the prepared vehicle banks have changed.

For this handoff, please try your favorite originals with naturally collected
special pickups. Check the effect and spoken line, whether aiming/hits feel
right, and picking up more ammo after firing. Torque's ground wave and Loki's
cluster are especially useful visual checks. Resolution stays at 1x.

1. Choose **1 Player → Arcade**.
2. At **Select Location**, press Left from Meteor Crater to **Super Dreamland 64**.
3. At **Select Player**, press Left from the first sequel car to original Y
   (`'64 Luxo Saucer`). Five more Left presses reach Houston (`'75 Palamino`).
4. Accept the car. The enemy rows also wrap through the original roster. Keep
   the native `x1` quantity controls, then accept and press Cross to start.

Please focus on Dreamland driving/collision, water, scenery, and audio; Houston's
handling and beam weapon; and Y's hovering, steering, and special. Try returning
to the menu and choosing another car. Natural pickups and transformation behavior
are useful human checks. No dedicated testing of the eighteen retail maps is needed.

The earlier detailed checks concentrated on Houston and Y. All ten new special
families now also have live firing checks and callback tests. Sheila fired all
50 rounds, registered 11 vehicle hits, retired all 50 projectiles and emptied
her weapon; SPU events confirmed one voice line and 50 gun sounds. Torque's
wave ran to completion, and tests verify exact terrain restoration both at
completion and early destruction. Loki's corrected launch produces exactly
five bomblets and retires once. The
full thirteen-car carousel and return to a sequel entry were checked in one run.
Houston and a Y opponent were selected through the menus and ran together in
Dreamland, including movement, water/sound updates and a completed gameplay save.

Car models, portraits, displayed names, voices, stats and selected identities
are independent. Quest retains its retail roster; imported campaigns
are not part of this slice. Dreamland effects use half-rate filtered audio to fit
the PS1 sound budget; pitch and duration are preserved apart from SPU rounding.
Full-quality streaming remains future work. Retail shared sounds are unchanged.
An isolated run also hit an object-teardown callback crash; that separate
lifecycle failure remains under investigation. The texture fix is not a claim
of crash-free play across all events.
Torque on Meteor Crater reached Survival gameplay after the sound-memory fix;
this was a focused startup regression, not a retail-map test campaign.

All donor media, prepared assets, saves and screenshots live outside the source
repository. The initial private v0.1.0 playtest release documents the remaining
HUD jitter and certification limits in RELEASE_NOTES.md.


### High Frame Rate HUD follow-up (2026-10-02)

The split-screen guest loop pipelines world(0) with HUD(1), then world(1)
with HUD(0). Replaying one complete section placed the other player's HUD on
an extra frame. Extra frames now replay the local world and its matching HUD
from separately captured CPU contexts. The pending HUD context is retained
until the next native DrawSync, rather than overwritten by that frame's HUD.
The framework also keeps bounded replay out of compiled continuations that
would run past its stop PC.

A comparison build reproduces switching car icons and shifting HUD bounds.
The corrected build was inspected across 200 consecutive actual presentations
per player at each fixed aspect, 16:9 and 21:9, including multiple swaps within
one guest frame. Each player retains its own icon and HUD placement while the
world continues to render extra frames. More than 2,800 verified replay passes
and 2,100 actual extra presentations completed without span failures, state
mismatches, watchdogs or VRAM leaks. Meteor Crater single-player was entered
through the menus to check the original full-screen replay path as well.

Windows and Linux builds pass all 21 title checks (including the owner car
package check), plus six focused framework checks. Captures and prepared media
remain outside Git under the isolated `v82-hud-fix-20261002` run. This fix is on
`fix/v82-hfr-hud` and is included in v0.1.1. The v0.1.0 tag and assets
remain available unchanged.


### Compact multiplayer weapon HUD follow-up (2026-10-02)

The first icon/position check did not exercise an armed HUD. The owner then
reported a weapon display at the left with its ammo count near the center.
The native vertical-split HUD modes 4/5 use one compact 160px group. Classifying
each packet against global 320px thirds anchored its weapon left and ammo
center, separating them by the reveal margin. The dedicated compact HUD chain
now shares a left anchor; the subsequent reticle/radar routine remains separate.
Full-screen HUD modes retain their existing anchors.

An actual armed original Y state reproduced the detached count of 12. After
the fix it stays in its weapon display at both fixed aspects. Another 200
consecutive presentations per player per aspect include multiple swaps per
guest frame; the armed peer completed over 1,600 verified replay passes without
a state mismatch, span failure, watchdog or VRAM leak. The regression uses
retail weapon/ammo coordinates in both compact layouts, one to three ammo
glyphs, and both enhanced widths; it fails before the fix and passes after.
Returning to a single-player right anchor is also covered. Both production
platforms pass all 21 title checks. This correction is included in v0.1.1.


### Ultrawide water follow-up (v0.1.2)

The owner accepted building visibility and subdivision. A separate Nuclear
Plant report showed early water cutoff. Native base-water packets still used
X=0..320 in an 854-pixel view; the textured layer also had independent screen
outcodes and a 32x32 traversal cap. The fix widens only water's local bounds,
recenters its validated packet interval, and renders adjacent bounded grids.
Shared boundary vertices do not duplicate cells or overlap transparent faces.

Actual Windows OpenGL views at 16:9, 21:9 and 32:9 show restored side coverage
with advancing water counters and no steady-state packet/grid rejection.
The live HFR120-option run recorded 345 presentations and 13 verified replay
passes, with no mismatch, abort, watchdog, span failure or VRAM leak. This
does not establish sustained 120 FPS. The 160-pixel enhanced viewport path
has unit coverage; a new two-peer water run has not been performed.

The initial 21:9 diagnostic camera sweep stopped advancing during manual
camera writes; those captures were excluded. Fresh-state checks at the same
angles advanced normally. The transient's cause was not established.
Evidence stays outside Git in the v82-water-culling-20261002 run directory.

The v0.1.2 Windows and Linux production builds pass all 21 title checks,
including the original-car package fixture.
