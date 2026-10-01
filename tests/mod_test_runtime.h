#pragma once
// Exercise the actual title allocator through its registered entry contract.
#include "../src/mods/v82_combined_plugin.cpp"
#include <iostream>

static std::array<uint8_t, 0x800000> memory;
static void (*test_dispatch)(CPUState*, uint32_t, uint32_t) = nullptr;
static void (*test_gte_execute)(CPUState*, uint32_t) = nullptr;
static uint32_t (*test_spu_read)(uint32_t) = nullptr;
static void (*test_spu_write)(uint32_t,uint32_t) = nullptr;
static void (*test_spu_dma)(uint32_t) = nullptr;
extern "C" {
uint8_t psx_mod_read_byte(uint32_t p) { return memory.at(p & 0x1FFFFFFF); }
void psx_mod_write_byte(uint32_t p, uint8_t v) { memory.at(p & 0x1FFFFFFF) = v; }
uint16_t psx_mod_read_half(uint32_t p) { return psx_mod_read_byte(p) | uint16_t(psx_mod_read_byte(p+1)) << 8; }
void psx_mod_write_half(uint32_t p, uint16_t v) { for(unsigned i=0;i<2;++i) psx_mod_write_byte(p+i,uint8_t(v>>(8*i))); }
uint32_t psx_mod_read_word(uint32_t p) { return psx_mod_read_half(p) | uint32_t(psx_mod_read_half(p+2)) << 16; }
void psx_mod_write_word(uint32_t p, uint32_t v) { for(unsigned i=0;i<4;++i) psx_mod_write_byte(p+i,uint8_t(v>>(8*i))); }
void psx_mod_counter_add(const char*, uint32_t) {}
int psx_mod_finish_function(CPUState* c) { c->pc=c->gpr[31]; return 1; }
void psx_dispatch_call(CPUState* c, uint32_t pc, uint32_t ra) {
    if (!test_dispatch) std::abort();
    test_dispatch(c, pc, ra);
}
void gte_write_ctrl(CPUState* c, uint8_t reg, uint32_t v) { c->gte_ctrl[reg] = v; }
void gte_write_data(CPUState* c, uint8_t reg, uint32_t v) { c->gte_data[reg] = v; }
uint32_t gte_read_data(CPUState* c, uint8_t reg) { return c->gte_data[reg]; }
void gte_execute(CPUState* c, uint32_t cmd) {
    if (!test_gte_execute) std::abort();
    test_gte_execute(c, cmd);
}
uint32_t spu_read(uint32_t p) { if(!test_spu_read)std::abort();return test_spu_read(p); }
void spu_write(uint32_t p,uint32_t v) { if(!test_spu_write)std::abort();test_spu_write(p,v); }
void spu_dma_write(uint32_t v) { if(!test_spu_dma)std::abort();test_spu_dma(v); }
int psx_mod_current_resource_bytes(const char*, const uint8_t**, uint64_t*) { std::abort(); }
int psx_mod_append_disc_extent(const char*,uint64_t,uint32_t,uint32_t*) { std::abort(); }
int psx_mod_option_value(const char*,const char*,const char*,char*,uint32_t) { std::abort(); }
int psx_mod_set_main_ram_8mb(int) { std::abort(); }
int psx_mod_register_activation_plugin(const char*,void(*)(void)) { return 1; }
int psx_mod_register_function_entry_plugin(const char*,uint32_t,PSXModFunctionEntryCallback) { return 1; }
int psx_mod_register_guest_function_plugin(const char*,uint32_t,PSXModFunctionEntryCallback) { return 1; }
int psx_mod_register_instruction_plugin(const char*,uint32_t,uint32_t,PSXModFunctionEntryCallback) { return 1; }
}
