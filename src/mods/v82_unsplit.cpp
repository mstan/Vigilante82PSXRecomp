/* V82 keeps two native cameras for deterministic netplay. The presenter shows
 * the camera assigned to this peer. Keep their layout vertical: the original
 * pause menu's horizontal option would otherwise disable the local viewport.
 * These frame services also execute during rollback; no local seat is read. */
#include "mod_plugins.h"
#include "psx_netplay.h"
#include <cstdint>

extern "C" void mod_register_frame_hook(void (*hook)(void));
namespace {
constexpr uint32_t Layout = 0x8006A81C, Modal = 0x8006B4F0;
constexpr uint32_t Row = 0x8006B400, Rows = Row + 1;
void unsplit_frame() {
    if (!psx_netplay_active() || !psx_mod_game_started()) return;
    // Signature of the native layout store: fail closed before/after this EXE.
    if (psx_mod_read_word(0x80011614) != 0xAF82002C) return;
    const uint32_t layout = psx_mod_read_word(Layout);
    if (layout != 1 && layout != 2) return; // single-player/menu layout
    if (layout == 1) psx_mod_write_word(Layout, 2);
    if (psx_mod_read_word(Modal) != 1) return;
    // Native row bit 1 controls BOTH drawing and navigation (8001235C,
    // 800125D8,8001265C). Let native navigation skip the split-only option.
    // Native menu entry rebuilds this mask, including for later offline play.
    const uint8_t rows = psx_mod_read_byte(Rows);
    if (rows == 7) {
        psx_mod_write_byte(Rows, 5);
        if (psx_mod_read_byte(Row) == 1) psx_mod_write_byte(Row, 2);
    }
    psx_mod_write_byte(Row + 5, 1);
}
PSX_MOD_CONSTRUCTOR(register_unsplit) { mod_register_frame_hook(unsplit_frame); }
}
