/* World Texture Filtering (v82.enhancement.texture-filtering): select the
 * framework's stable minification filter, as MediEvil II does. A mod-session
 * reset restores the player's Display texture filter. */
#include "mod_plugins.h"
static void stable(void) { psx_mod_set_texture_filter(2); }
PSX_MOD_CONSTRUCTOR(register_texture_filter) {
    psx_mod_register_activation_plugin("v82.filter.stable", stable);
}
