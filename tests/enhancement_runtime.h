#pragma once
#include "mod_plugins.h"
#include "cpu_state.h"
#include <array>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
static std::array<uint8_t,0x800000> memory;
static std::array<uint8_t,1024> scratchpad;
static std::string option="Fit";
static uint32_t display_width=320, aspect_n=0, adaptive_n=0;
static int live_margin=0;
static int netplay_active=0, netplay_wide_width=0;
static PSXModWorldScenePredicate world_callback=nullptr;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
extern "C" {
int psx_netplay_active(){return netplay_active;}
int gpu_ws_netplay_local_viewport_width(){return netplay_wide_width;}
uint8_t psx_mod_read_byte(uint32_t p){p&=0x1FFFFFFF;return p>=0x1F800000?scratchpad.at(p-0x1F800000):memory.at(p);}
void psx_mod_write_byte(uint32_t p,uint8_t v){p&=0x1FFFFFFF;if(p>=0x1F800000)scratchpad.at(p-0x1F800000)=v;else memory.at(p)=v;}
uint16_t psx_mod_read_half(uint32_t p){return psx_mod_read_byte(p)|(uint16_t(psx_mod_read_byte(p+1))<<8);}
uint32_t psx_mod_read_word(uint32_t p){return psx_mod_read_half(p)|(uint32_t(psx_mod_read_half(p+2))<<16);}
void psx_mod_write_word(uint32_t p,uint32_t v){for(unsigned i=0;i<4;++i)psx_mod_write_byte(p+i,uint8_t(v>>(8*i)));}
void psx_mod_counter_add(const char*,uint32_t){}
int psx_mod_finish_function(CPUState* c){c->pc=c->gpr[31];return 1;}
int psx_mod_register_activation_plugin(const char*,PSXModActivationCallback){return 1;}
int psx_mod_register_vblank_plugin(const char*,PSXModVBlankCallback){return 1;}
int psx_mod_register_function_entry_plugin(const char*,uint32_t,PSXModFunctionEntryCallback){return 1;}
int psx_mod_register_instruction_plugin(const char*,uint32_t,uint32_t,PSXModFunctionEntryCallback){return 1;}
int psx_mod_option_value(const char*,const char*,const char*,char* p,uint32_t n){if(option.size()>=n)return 0;std::strcpy(p,option.c_str());return 1;}
int psx_mod_set_main_ram_8mb(int){return 1;}
int32_t psx_mod_widescreen_x_margin(){return live_margin;}
uint32_t psx_mod_display_width(){return display_width;}
int psx_mod_set_fixed_display_aspect(uint32_t n,uint32_t d){check(d==9,"aspect denominator");aspect_n=n;return 1;}
int psx_mod_set_adaptive_display_aspect(uint32_t n,uint32_t d){check(d==9,"adaptive denominator");adaptive_n=n;return 1;}
void psx_mod_set_world_scene_predicate(PSXModWorldScenePredicate p){world_callback=p;}
}
