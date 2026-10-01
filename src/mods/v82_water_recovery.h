#pragma once
namespace dreamland {
constexpr uint32_t RecoveryState=0x80310400, RecoveryMagic=0x57524543;
inline bool recovery_pointer(uint32_t p,uint32_t size=0x80) {
    return p>=0x80010000 && p<=0x80800000-size && !(p&3);
}
inline void install_water_recovery(CPUState* c) {
    // The converted arena has events 0..18. Native sinking completes with
    // event 19, which must relocate the vehicle before the engine resumes it.
    // Keep the original callback in saved enhancement RAM and delegate every
    // existing event. Only this imported overlay calls the Water helper.
    const auto slot=c->gpr[28]+0x104C, original=rd(slot);
    if(original==ArenaEvent)return;
    if(!recovery_pointer(original,4)||rd(original)!=0x27BDFFC8)return;
    wr(RecoveryState,RecoveryMagic);wr(RecoveryState+4,original);wr(slot,ArenaEvent);
}
inline uint32_t arena_event(CPUState* c) {
    const auto original=rd(RecoveryState+4),vehicle=c->gpr[4],event=c->gpr[5];
    if(rd(RecoveryState)!=RecoveryMagic||!recovery_pointer(original,4)||original==ArenaEvent)
        fail("v82.dreamland.invalid_arena_callback");
    const auto result=invoke(c,original,vehicle,event,c->gpr[6],c->gpr[7]);
    if(event!=19||int32_t(result)<0)return result;
    if(!recovery_pointer(vehicle,0x200)||psx_mod_read_byte(vehicle+8)!=2)return result;
    // Negative IDs are the four standing player starts. Positive 1..31 are
    // reinforcement entrances: native spawning launches cars off those
    // sloped edges, so they are unsuitable for a stationary recovery.
    uint32_t start=0;int64_t nearest=INT64_MAX;
    const int owner=int16_t(psx_mod_read_half(vehicle+0xA));
    for(int id=-1;id>=-4;--id) {
        const auto candidate=invoke(c,0x80030250,c->gpr[28]+0x1078,uint32_t(id));
        if(!recovery_pointer(candidate)||psx_mod_read_byte(candidate+8)!=5||
           int16_t(psx_mod_read_half(candidate+0xA))!=id)continue;
        const int64_t dx=(int64_t(int32_t(rd(candidate+0x4C)))-int32_t(rd(vehicle+0x34)))/256;
        const int64_t dz=(int64_t(int32_t(rd(candidate+0x54)))-int32_t(rd(vehicle+0x3C)))/256;
        const auto distance=dx*dx+dz*dz;
        if(id==owner){start=candidate;break;}
        if(distance<nearest){nearest=distance;start=candidate;}
    }
    if(!start) {
        psx_mod_counter_add("v82.dreamland.recovery_missing_start",1);return result;
    }
    for(unsigned i=0;i<12;i+=4)wr(vehicle+0x4C+i,rd(start+0x4C+i));
    for(unsigned i=0;i<6;i+=2)psx_mod_write_half(vehicle+0x44+i,psx_mod_read_half(start+0x44+i));
    for(unsigned i=0;i<0x1C;i+=4)wr(vehicle+0x80+i,0);
    invoke(c,0x8002CF74,vehicle); // native position + orientation matrix update
    psx_mod_counter_add("v82.dreamland.water_recovered",1);
    return result; // native event-2 handler restores driving and camera state
}
}
