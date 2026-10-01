# Authorized reference reuse

The owner reported author permission to adapt
[GTTeancum/Vigilante-8-recomp](https://github.com/GTTeancum/Vigilante-8-recomp)
on 2026-09-29 and requested maximal reuse for this combined game on 2026-09-30.
Source revision: `6fc8179bebc8d30dc01f05d5cf1fd398a82e2a19`.

`combined/provenance.json` records original paths and hashes for unchanged
copied Python modules. `vehicle_provenance.json` records the vehicle codecs.
The codec package initializer omits Blender UI imports; kind-10 control records
retain our existing correction for V8's four-byte versus V8:2's eight-byte dialect.
Local codec changes also admit all thirteen original-special capabilities and
preserve native animation terminal times and loop targets. Native ANM timestamps
are absolute within a cycle; summing them delayed Clyde's launch from 80 ticks
to 1640. Original upstream hashes remain recorded alongside these adaptations.

The Python tools have no blanket upstream license notice; their reuse rests
on the reported author permission, not a newly assigned license. Separately,
RecompOne runtime adapters carry the MIT license, copyright (c) 2026 flaffy;
its notice is retained in `LICENSE_RECOMPONE` for adapters ported from there.
`src/mods/v82_vehicle_package.{h,cpp}` adapts the pinned runtime's
`sdk/NativeVehicleBankSource.cs` and `sdk/V82VehicleRegistry.cs`, retaining
their native package format, capabilities and ownership validation.
`src/mods/v82_combined_plugin.cpp` also adapts `V82Compat.cs`,
`V82VehicleRegistry.cs` and `apply_vehicle_engine_patches.py`: independent
heap/bank ownership, constructor and suspension setup, native texture
reservations, capability checks and instruction-level pointer selection.
`src/mods/v82_original_specials.inc` directly adapts `sdk/V8OriginalSpecials.cs`:
Houston/Y weapons, beam segments, impact effects and native service calls.
Synthetic callback addresses use psxrecomp's reserved mod namespace; wrapping
arithmetic and the reference's separate outgoing stack frame are retained.
`src/mods/v82_flying_controller.inc` adapts the original-flight methods in
`V82VehicleRegistry.cs` and the three-command GTE sequence in `V8VehicleMath.cs`.
The native function-hook integration replaces the reference's in-function
wheel/ground/follow selections with equivalent contact lookup, ground dispatch
and post-function follow seams; no generated source is hand-edited.

`tools/import_dreamland.py` invokes the unchanged attributed Dreamland asset,
selector and arena registry converters. `tools/dreamland_overlay.py` adapts
the ABI, object-field, engine-call, global and lifecycle mappings from
`combined/port_dreamland_overlay.py` to input MIPS. Its native DLL relocation
arithmetic follows `RecompOne.Recompiler/CodeGen/OverlayWriter.cs` under the
retained MIT notice. psxrecomp alone recompiles the resulting input. The
additional branch-likely lowering preserves N64 annul semantics on R3000.

Only source and analysis metadata are reused. Users supply all game media;
extracted and converted assets remain outside repositories. psxrecomp provides
the runtime/recompiler; the reference runtime is not substituted for it.

Native combined-menu integration additionally adapts V82VehicleRegistry.cs and
apply_native_selector_patches.py at the same pinned revision in
src/mods/v82_native_selector.inc and tools/vehicle_menu.py. The adaptation emits
input-MIPS bridges for psxrecomp instead of modifying generated source. The
existing RecompOne MIT notice applies to these derived portions.

`src/mods/v82_draw_packets.inc` adapts the gameplay ordering-table validation
in `sdk/LibGpu.cs` (`IsSafeGameplayOrderingTablePacket`) and packet lengths in
`Gpu/GpuCommands.cs`, under the same MIT notice. psxrecomp still executes the
native SDK and hardware DMA; the title omits malformed packets before submission.
Polyline validation preserves the hardware's mandatory first two vertices.

`src/mods/v82_presentation.inc` adapts the original identity, portrait and
selection/result voice mappings in `sdk/V82VehicleRegistry.cs` and
`sdk/V82Compat.cs`, under the retained MIT notice. `import_original_presentation.py`
uses the unchanged selection-bank builder and result-XA extractor listed in
`combined/provenance.json`. psxrecomp supplies native VLC portrait decoding,
SPU playback and CD XA streaming from verified external resources.
The opt-in native defeat fixture adapts `V82Compat.UpdateDefeatRegression`;
it uses the game's lethal-damage path and is disabled in ordinary play.
The title importer replaces the extractor's rejected-audio filler with the
sequel disc's non-audio filler so native CdReady can stop at the file boundary.

`src/mods/v82_lightning.inc` uses `src/gameplay/vehicle_init_joints.c` for original
Slick Clyde service/structure context, with all five callbacks checked against
the owner's SLUS_005.10 executable (800380C8..80038A08). The reference C helper
omits the device's three strikes and launch path; those follow the source disc.
V8:2 engine-service equivalents were checked against the owner's SLUS_008.68.
`import_original_cars.py` appends the original impact/spark roots and original
thunder/impact samples to Clyde's owned bank, retaining his original fire voice.

`src/mods/v82_special_roster.inc` adapts the remaining original weapon families
from `src/gameplay/vehicle_init_joints.c` at the same pinned revision, under the
owner-reported author permission. That source has no blanket license notice;
this adaptation does not assign it the separate RecompOne MIT license. Missing
launch/collision paths, incorrect callback identities and constants were checked
against the owner's SLUS_005.10 and SLUS_008.68 executables. The title importer
extracts the required original effect roots into per-special banks with explicit
kind remaps and one sparse common sound source. Source assets stay external.
