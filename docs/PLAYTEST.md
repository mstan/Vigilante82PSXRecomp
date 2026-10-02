# Combined Arcade playtest

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
`fix/v82-hfr-hud`; the previously published v0.1.0 assets are unchanged.
