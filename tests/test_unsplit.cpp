#ifdef NDEBUG
#undef NDEBUG
#endif
#include "mod_plugins.h"
#include <array>
#include <cassert>
#include <cstring>
static std::array<uint8_t, 0x200000> ram;
static bool online, started;
extern "C" {
int psx_netplay_active() { return online; }
int psx_mod_game_started() { return started; }
uint8_t psx_mod_read_byte(uint32_t p) { return ram[p & 0x1fffff]; }
uint32_t psx_mod_read_word(uint32_t p) { uint32_t v; memcpy(&v,&ram[p & 0x1fffff],4); return v; }
void psx_mod_write_byte(uint32_t p,uint8_t v) { ram[p & 0x1fffff]=v; }
void psx_mod_write_word(uint32_t p,uint32_t v) { memcpy(&ram[p & 0x1fffff],&v,4); }
void mod_register_frame_hook(void (*)(void)) {}
}
#include "../src/mods/v82_unsplit.cpp"
int main() {
    psx_mod_write_word(0x80011614,0xAF82002C);
    psx_mod_write_word(Layout,1); psx_mod_write_word(Modal,1);
    psx_mod_write_byte(Row,1); psx_mod_write_byte(Rows,7);
    static auto before=ram;
    started=true; unsplit_frame(); assert(ram==before);
    online=true; started=false; unsplit_frame(); assert(ram==before);
    started=true; unsplit_frame();
    assert(psx_mod_read_word(Layout)==2 && psx_mod_read_byte(Rows)==5);
    assert(psx_mod_read_byte(Row)==2 && psx_mod_read_byte(Row+5)==1);
    static auto after=ram; unsplit_frame(); assert(ram==after);
    ram=before; unsplit_frame(); assert(ram==after); // rollback repeats identically
    psx_mod_write_word(Layout,0); before=ram; unsplit_frame(); assert(ram==before);
    psx_mod_write_word(Layout,1); psx_mod_write_word(0x80011614,0); before=ram;
    unsplit_frame(); assert(ram==before);
    online=false; psx_mod_write_byte(Rows,7); before=ram;
    unsplit_frame(); assert(ram==before); // native offline menu rebuild preserved
}
