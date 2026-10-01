# Combined Arcade playtest

This build runs under psxrecomp at native 1x resolution, with 4K disabled.
Enable both prepared content packages; the local owner playtest already does so.
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
repository. This is a local playtest build; nothing has been published.
