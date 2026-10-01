#pragma once
/* Native arena helpers adapted from port_dreamland_overlay.py, revision
 * 6fc8179. Attribution/permission: tools/third_party/NOTICE.md.
 * The guest overlay is recompiled by psxrecomp; these are its platform ports. */
#include "cpu_state.h"
#include "mod_plugins.h"
#include <cstdint>
#include <cstdlib>

namespace dreamland {
constexpr uint32_t Hazard=0x8FFF0100, Water=0x8FFF0104, Contact=0x8FFF0108;
constexpr uint32_t Nearest=0x8FFF010C, Sound3d=0x8FFF0110, SoundAlt=0x8FFF0114;
constexpr uint32_t Sound=0x8FFF0118, WaterInit=0x8FFF011C, ModelEvent=0x8FFF0120;
constexpr uint32_t ContactUpdate=0x8FFF0124, ArenaEvent=0x8FFF0128;
inline uint32_t rd(uint32_t p) { return psx_mod_read_word(p); }
inline void wr(uint32_t p,uint32_t v) { psx_mod_write_word(p,v); }
[[noreturn]] inline void fail(const char* reason) { psx_mod_counter_add(reason,1); std::abort(); }
inline uint32_t invoke(CPUState* c,uint32_t pc,uint32_t a0=0,uint32_t a1=0,uint32_t a2=0,uint32_t a3=0) {
    const CPUState saved=*c;
    c->gpr[29]-=0x40;
    c->gpr[4]=a0;c->gpr[5]=a1;c->gpr[6]=a2;c->gpr[7]=a3;c->pc=0;
    psx_dispatch_call(c,pc,saved.gpr[31]);
    const auto result=c->gpr[2];*c=saved;return result;
}
inline uint32_t sound_id(uint32_t id) {
    // Importer appends N64 samples to the sequel's 79 untouched native entries.
    constexpr uint32_t ids[]={0x45,0x46,0x48,0x49,0x4A,0x4B,0x4C,0x4E,0x4F,0x50,0x5F,0x8D,0x8E,0x97,0xA6};
    for(unsigned i=0;i<15;++i)if(ids[i]==id)return 79+i;
    fail("v82.dreamland.unknown_sound");
}
}
#include "v82_water_recovery.h"
#include "v82_dreamland_stress.h"
namespace dreamland {
inline void helper(CPUState* c,uint32_t address) {
    address|=0x80000000u;
    const auto object=c->gpr[4], event=c->gpr[5];
    uint32_t result=0;
    switch(address) {
    case Hazard: {
        const auto impulse=c->gpr[29]-0x10;
        wr(impulse,0);wr(impulse+4,0);wr(impulse+8,0);
        if(event==0x80000000) fail("v82.dreamland.invalid_damage");
        const uint32_t damage=int32_t(event)>0?0u-event:event;
        result=invoke(c,0x8003A020,object,damage,impulse,1);
        psx_mod_counter_add("v82.dreamland.hazard",1);break;
    }
    case Water:
        result=invoke(c,0x80017EB8,object,event,c->gpr[6],c->gpr[7]);
        install_water_recovery(c);
#ifndef PSX_NO_DEBUG_TOOLS
        stress_projectiles(c);
#endif
        psx_mod_counter_add("v82.dreamland.water_frame",1);break;
    case WaterInit:
#ifndef PSX_NO_DEBUG_TOOLS
        if(stress_enabled)for(unsigned i=0;i<0x80;i+=4)wr(Stress+i,0);
#endif
        invoke(c,0x80017F34,0x2A400,0x7FFF0000);
        psx_mod_counter_add("v82.dreamland.water_init",1);return;
    case Sound3d:case SoundAlt:case Sound:
        result=invoke(c,address==Sound3d?0x8001E580:address==SoundAlt?0x8001E628:0x8001E098,
                      object,rd(0x8006B41C),sound_id(c->gpr[6]),c->gpr[7]);
        psx_mod_counter_add("v82.dreamland.sound",1);break;
    case Nearest: {
        const int32_t requested=int32_t(event);
        const int family=requested<0?int8_t(psx_mod_read_byte(0x8006AD74u+~event)):-1;
        uint32_t exact=0,related=0,exact_distance=UINT32_MAX,family_distance=UINT32_MAX;
        const auto root=rd(0x8006B830);
        for(uint32_t node=rd(root);node;node=rd(node)) {
            const auto candidate=rd(node+8);
            if(!candidate || psx_mod_read_byte(candidate+8)!=2 || !psx_mod_read_half(candidate+0x1C)) continue;
            const auto distance=invoke(c,0x80029F6C,object,candidate+0x4C);
            const int32_t id=int16_t(psx_mod_read_half(candidate+0x1A));
            if(id==requested && distance<exact_distance) {exact=candidate;exact_distance=distance;}
            else if(id<=0 && int8_t(psx_mod_read_byte(0x8006AD74u+~uint32_t(id)))==family && distance<family_distance) {
                related=candidate;family_distance=distance;
            }
        }
        result=exact?exact:related;psx_mod_counter_add("v82.dreamland.nearest",1);break;
    }
    case Contact: {
        const auto primary=c->gpr[6]&0xFFFF,secondary=c->gpr[7]&0xFFFF;
        const auto sound=rd(c->gpr[29]+0x10)&0xFFFF,interval=rd(c->gpr[29]+0x14),lifetime=rd(c->gpr[29]+0x18);
        const auto effect=invoke(c,0x8002C17C,event,primary,0x9C,8);
        if(!effect)fail("v82.dreamland.effect_allocation_failed");
        invoke(c,0x8002A39C,effect+0x20);
        for(unsigned i=0;i<12;i+=4)wr(effect+0x34+i,rd(object+i));
        wr(effect,ContactUpdate);wr(effect+0x58,0x8000);wr(effect+0x98,event);
        psx_mod_write_half(effect+0x96,uint16_t(secondary));wr(effect+4,rd(effect+4)|0xA4);
        psx_mod_write_half(effect+0x82,uint16_t(interval));
        invoke(c,0x800305FC,effect);invoke(c,0x80030CB0,effect,lifetime);
        const auto voice=invoke(c,0x8001DD9C);
        invoke(c,0x8001E580,voice,rd(0x8006B41C),sound_id(sound),object);
        result=effect;psx_mod_counter_add("v82.dreamland.contact",1);break;
    }
    case ContactUpdate:
        if(event==2) psx_mod_write_half(object+0x80,0xFFFF);
        else if(event==5) {wr(object+0x64,0);wr(object+4,rd(object+4)|2);result=UINT32_MAX;}
        else if(event==0) {
            const auto counter=uint16_t(psx_mod_read_half(object+0x80)-1);
            psx_mod_write_half(object+0x80,counter);
            if(counter==0xFFFF) {
                const auto particle=invoke(c,0x8002C17C,rd(object+0x98),psx_mod_read_half(object+0x96),0x80,8);
                if(!particle)fail("v82.dreamland.particle_allocation_failed");
                invoke(c,0x8002A39C,particle+0x20);
                const int64_t radius=int32_t(rd(object+0x58));
                const int64_t rx=int32_t(invoke(c,0x8002AC5C));
                wr(particle+0x34,uint32_t(((rx*2*radius)>>15)-radius));wr(particle+0x38,0);
                const int64_t rz=int32_t(invoke(c,0x8002AC5C));
                wr(particle+0x3C,uint32_t(((rz*2*radius)>>15)-radius));wr(particle,0x8004CA68);
                invoke(c,0x8002CC48,object,particle);
                psx_mod_write_half(object+0x80,psx_mod_read_half(object+0x82));
            }
            if(!rd(object+0x10)) {invoke(c,0x800309A0,object);result=UINT32_MAX;}
        }
        break;
    case ModelEvent: {
        const auto actor=c->gpr[6], child=invoke(c,0x8002CD78,object,event,actor,c->gpr[7]);
        const auto callback=child?rd(child):0;
        if(callback>=0x80000000)result=invoke(c,callback,child,event,actor);
        break;
    }
    case ArenaEvent:result=arena_event(c);break;
    default:fail("v82.dreamland.unknown_helper");
    }
    c->gpr[2]=result;
}
}
