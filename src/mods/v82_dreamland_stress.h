#pragma once
#ifndef PSX_NO_DEBUG_TOOLS
namespace dreamland {
// Opt-in headless fixture. Exercise real imported callbacks and native damage,
// spawning, homing and cleanup; all mutable fixture state stays in guest RAM.
bool stress_enabled=false;
constexpr uint32_t Stress=0x80310800;
inline void stress_projectiles(CPUState* c) {
    if(!stress_enabled || rd(RecoveryState)!=RecoveryMagic)return;
    const auto base=rd(RecoveryState+4)-0x63C;
    if(!recovery_pointer(base,0x3208)||rd(base+0x1904)!=0x27BDFFD8||
       rd(base+0x1E30)!=0x27BDFFD8)fail("v82.stress.overlay_identity");
    const auto tick=rd(Stress)+1;wr(Stress,tick);
    if(tick<120 || tick%32)return;
    // Scan each authored mushroom once. A live model child distinguishes a
    // ready actor from a dormant/recharging one and arbitrary archive bytes.
    auto cursor=rd(Stress+4);if(!cursor)cursor=0x80400000;
    for(;cursor<0x807FFF00;cursor+=8) {
        const auto callback=rd(cursor);
        if(callback!=base+0x1904 && callback!=base+0x1E30)continue;
        const auto child=rd(cursor+0x10);
        if(!recovery_pointer(child)||rd(child+0x14)!=cursor)continue;
        wr(Stress+4,cursor+8);
        // Retail event-3 collision payload: weapon type 7, player owner -1,
        // and enough damage to trigger the mushroom's native reaction.
        const auto collision=Stress+0x40,weapon=Stress+0x60;
        for(unsigned i=0;i<0x40;i+=4)wr(collision+i,0);
        wr(collision,weapon);psx_mod_write_byte(weapon+4,7);
        psx_mod_write_half(weapon+6,0xFFFF);
        psx_mod_write_half(weapon+0xC,0xFFFF);
        invoke(c,callback,cursor,3,collision);
        psx_mod_counter_add(callback==base+0x1904?
                            "v82.stress.good_mushroom":"v82.stress.bad_mushroom",1);
        // The active-object list must contain only proper projectile entries,
        // never an address modified by flag writes into its callback field.
        const auto root=rd(c->gpr[28]+0x1040);
        unsigned count=0;
        for(auto node=rd(root);node;node=rd(node)) {
            if(!recovery_pointer(node,12)||++count>4096)fail("v82.stress.object_list");
            const auto object=rd(node+8),entry=rd(object);
            if((entry>=base+0x15EC&&entry<base+0x1904&&entry!=base+0x15EC)||
               (entry>=base+0x1B78&&entry<base+0x1E30&&entry!=base+0x1B78))
                fail("v82.stress.invalid_projectile_callback");
            if(entry==base+0x15EC||entry==base+0x1B78)
                psx_mod_counter_add("v82.stress.valid_projectile",1);
        }
        return;
    }
    if(!rd(Stress+8)) {wr(Stress+8,1);psx_mod_counter_add("v82.stress.sweep_complete",1);}
    wr(Stress+4,0x807FFF00);
}
}
#endif
