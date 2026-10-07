/* REFERENCE keeps the generated SLUS-00868 stream, decoder, allocation,
 * loading wipe, VSync and SPU routines in the ordinary guest dispatch. No
 * entry hook intercepts them. This activation preserves the published mod
 * plugin identity without preparing a resident pack or changing guest state.
 * The original generated bodies remain common runtime sources in both builds.
 */
#include "mod_plugins.h"

PSX_MOD_CONSTRUCTOR(register_v82_resident_reference) {
    psx_mod_register_activation_plugin("v82.resident", [] {
        psx_mod_counter_add("v82.resident.reference", 1);
    });
}
