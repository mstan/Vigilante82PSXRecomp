/* Original Slick Clyde lightning: SLUS_005.10 800380C8..80038A08.
 * Uses the authorized reference vehicle_init_joints.c for service/structure
 * context, corrected against owner-disc MIPS for all five callbacks. The
 * reference's LAB_8003846c omits the three strikes and launch-animation path.
 * Guest object ABI and engine services are V8:2; arithmetic wraps as on MIPS. */
namespace lightning {
using original_specials::invoke;
using original_specials::voice;
using original_specials::sound;
constexpr uint32_t Weapon=0x8FFF0040, Device=0x8FFF0044, Bolt=0x8FFF0048;
constexpr uint32_t Hit=0x8FFF004C, Sparks=0x8FFF0050, Active=0x8006B874;

unsigned impact_kind(uint32_t bank) {
    for(unsigned i=0;i<package.vehicles.size();++i)
        if(((package.vehicles[i].flags>>17)&31)==1 &&
           (package.vehicles[i].flags&(1u<<23)) && rd(state(i))==bank)
            return package.vehicles[i].original_impact_kind;
    fail("v82.lightning.missing_bank");
}
void sparks(CPUState* c,uint32_t) {
    const auto object=c->gpr[4];c->gpr[2]=0;
    if(c->gpr[5]==1) {
        for(unsigned n=0;n<8;++n) {
            const auto bank=rd(object+0x5C);
            const auto child=invoke(c,0x8002C17C,bank,impact_kind(bank)+1,0x94,8);
            if(!child)continue;
            for(unsigned i=0;i<12;i+=4)wr(child+0x4C+i,0);
            wr(child+0x88,uint32_t(original_specials::shift(invoke(c,0x8002AC5C),12,15))-2048);
            wr(child+0x8C,0u-uint32_t(original_specials::shift(invoke(c,0x8002AC5C),13,15)));
            wr(child+0x90,uint32_t(original_specials::shift(invoke(c,0x8002AC5C),12,15))-2048);
            wr(child+4,0x10);
            psx_mod_write_byte(child+0x86,uint8_t(original_specials::shift(invoke(c,0x8002AC5C),3,15)+24));
            invoke(c,0x8002CC48,object,child);
        }
    } else if(c->gpr[5]==0) {
        for(auto child=rd(object+0x10);child;) {
            const auto next=rd(child+0xC);
            const auto life=uint8_t(psx_mod_read_byte(child+0x86)-1);
            psx_mod_write_byte(child+0x86,life);
            if(!life) {invoke(c,0x8002CCBC,child);invoke(c,0x8002C4B4,child);}
            else {
                for(unsigned i=0;i<12;i+=4)wr(child+0x4C+i,rd(child+0x4C+i)+rd(child+0x88+i));
                wr(child+0x8C,rd(child+0x8C)+56);invoke(c,0x8002CF74,child);
            }
            child=next;
        }
    } else if(c->gpr[5]==5) {invoke(c,0x800309A0,object);c->gpr[2]=UINT32_MAX;}
}
void hit(CPUState* c,uint32_t) {
    const auto object=c->gpr[4],collision=c->gpr[6];c->gpr[2]=0;
    if(c->gpr[5]!=3)return;
    const auto bank=rd(object+0x5C),effect=invoke(c,0x8002C17C,bank,impact_kind(bank),0x80,8);
    if(effect) {
        psx_mod_write_byte(effect+8,7);wr(effect+4,0xB4);wr(effect,Sparks);
        for(unsigned i=0;i<12;i+=4)wr(effect+0x4C+i,rd(object+0x34+i));
        invoke(c,0x8003066C,effect);
    }
    wr(object+4,rd(object+4)|0x20);
    invoke(c,0x8001E580,voice(c),sound(object),4,object+0x34);
    const auto target=collision?rd(collision):0;
    if(target&&psx_mod_read_byte(target+8)==2) {
        psx_mod_counter_add("v82.lightning.vehicle_hits",1);
        invoke(c,0x8004E414,object+0x34,0x08C0C000);
        for(unsigned i=0;i<3;++i)if(const auto part=rd(target+0x11C+i*4))
            psx_mod_write_half(part+0xA,uint16_t(std::max(180,int(int16_t(psx_mod_read_half(part+0xA))))));
        if(invoke(c,0x8002AC5C)&1)invoke(c,0x80039BC4,target);
    }
    psx_mod_counter_add("v82.lightning.hits",1);
}
void bolt(CPUState* c,uint32_t) {
    const auto object=c->gpr[4],damage=rd(object+0x80);c->gpr[2]=0;
    if(c->gpr[5]==0) {
        const auto y=rd(object+0x38)+0xC800;wr(object+0x38,y);wr(damage+0x38,y);
        const auto ticks=uint16_t(psx_mod_read_half(object+0x94)+1);psx_mod_write_half(object+0x94,ticks);
        if(ticks==16)invoke(c,0x80030BA8,object);
    } else if(c->gpr[5]==5) {
        invoke(c,0x800309A0,damage);invoke(c,0x800309A0,object);c->gpr[2]=UINT32_MAX;
        psx_mod_counter_add("v82.lightning.bolts_retired",1);
    }
}
void strike(CPUState* c,uint32_t device,unsigned tick) {
    const auto bank=rd(device+0x5C),target=rd(device+0x84);
    const auto visual=invoke(c,0x8002C17C,bank,0x3D,0x98,8);
    if(!visual)return;
    const auto damage=invoke(c,0x8002CBE8,0x98);
    if(!damage){invoke(c,0x800309A0,visual);return;}
    wr(visual+4,0xB4);wr(visual,Bolt);
    if(const auto child=rd(visual+0x10))wr(child,0x8004CA10);
    wr(visual+0x4C,rd(target+0x4C)+(tick==30?0:(invoke(c,0x8002AC5C)-0x4000)*8));
    wr(visual+0x54,rd(target+0x54)+(tick==30?0:(invoke(c,0x8002AC5C)-0x4000)*8));
    wr(visual+0x50,rd(target+0x50)-0xC8000);wr(visual+0x80,damage);
    psx_mod_write_half(visual+0x94,0);
    psx_mod_write_byte(damage+8,8);wr(damage+4,0x60000000);wr(damage,Hit);
    psx_mod_write_half(damage+0x1C,psx_mod_read_half(device+0x1C));
    for(unsigned i=0;i<12;i+=4)wr(damage+0x4C+i,rd(visual+0x4C+i));
    wr(damage+0x80,rd(device+0x80));wr(damage+0x60,rd(visual+0x60));
    // A model-less native collider still needs its source bank for hit sounds
    // and the appended original impact/spark roots.
    wr(damage+0x5C,bank);
    invoke(c,0x8001E580,voice(c),sound(device),3,visual+0x4C);
    invoke(c,0x8003066C,visual);invoke(c,0x8003066C,damage);
    psx_mod_counter_add("v82.lightning.strikes",1);
}
void device(CPUState* c,uint32_t) {
    const auto object=c->gpr[4];c->gpr[2]=0;
    if(c->gpr[5]==0) {
        for(unsigned i=0;i<12;i+=4)wr(object+0x34+i,rd(object+0x34+i)+rd(object+0x88+i));
        const auto tick=uint16_t(psx_mod_read_half(object+0x94)+1);psx_mod_write_half(object+0x94,tick);
        if(tick==30||tick==60||tick==90)strike(c,object,tick);
        else if(tick==121) {
            invoke(c,0x800309A0,object);wr(Active,rd(Active)-1);c->gpr[2]=UINT32_MAX;
            psx_mod_counter_add("v82.lightning.devices_retired",1);
        }
    } else if(c->gpr[5]==5) {
        const auto parent=invoke(c,0x8002CD78,object),weapon=invoke(c,0x8002CD78,parent);
        if(const auto sibling=rd(object+0xC))wr(sibling+0x64,0);
        wr(object+4,0x90);const auto matrix=invoke(c,0x8002CDF4,object);
        for(unsigned i=0;i<32;i+=4)wr(object+0x20+i,rd(matrix+i));
        for(unsigned i=0;i<3;++i) {
            const int32_t component=int16_t(psx_mod_read_half(object+0x22+i*6));
            const int32_t product=int32_t(uint32_t(-component)*6103u);
            wr(object+0x88+i*4,uint32_t(product/4096));
        }
        invoke(c,0x8002CCBC,object);invoke(c,0x800305FC,object);
        if(!psx_mod_read_half(weapon+0x1C))invoke(c,0x8003A368,weapon);
        c->gpr[2]=UINT32_MAX;psx_mod_counter_add("v82.lightning.launched",1);
    }
}
void weapon(CPUState* c,uint32_t) {
    const auto object=c->gpr[4],owner=c->gpr[6];c->gpr[2]=0;
    switch(c->gpr[5]) {
    case 0:if(owner)invoke(c,0x80042330,object,owner);break;
    case 1: {
        psx_mod_write_half(object+0x1C,3);psx_mod_write_byte(object+8,3);wr(object+4,rd(object+4)|0x4000);
        invoke(c,0x8002FEE8,object,psx_mod_read_half(object+0x4A));
        const auto joint=rd(object+0x10),child=joint?rd(joint+0x10):0;
        if(child)wr(child+0x64,0);break;
    }
    case 12: {
        psx_mod_counter_add("v82.lightning.fire_requested",1);
        if(!owner)break;
        const auto shot=invoke(c,0x8002C17C,rd(object+0x5C),0x3F,0x98,8);
        if(!shot){psx_mod_counter_add("v82.lightning.allocation_failed",1);break;}
        const auto joint=rd(object+0x10),child=joint?rd(joint+0x10):0;
        if(!joint||!child){invoke(c,0x800309A0,shot);psx_mod_counter_add("v82.lightning.missing_joint",1);break;}
        invoke(c,0x8002C05C,child);wr(shot+4,0x10);wr(shot,Device);
        for(unsigned i=0;i<12;i+=4)wr(shot+0x4C+i,0);
        psx_mod_write_half(shot+0x1C,300);wr(shot+0x84,rd(owner+0xF0)?rd(owner+0xF0):owner);
        psx_mod_write_half(shot+0x94,0);wr(shot+0x80,owner);
        invoke(c,0x8002CC9C,joint,shot);
        invoke(c,0x8001E580,voice(c),sound(object),0,owner+0x34);
        invoke(c,0x8001E14C,voice(c),sound(object),1);
        psx_mod_write_half(object+0x1C,uint16_t(psx_mod_read_half(object+0x1C)-1));wr(Active,rd(Active)+1);
        c->gpr[2]=int16_t(psx_mod_read_half(owner+0xA))>=0?240:120;
        psx_mod_counter_add("v82.lightning.fired",1);break;
    }
    case 13:
        if(owner&&!rd(Active)&&rd(owner+0xF0)) {
            const auto distance=int32_t(invoke(c,0x80029F6C,owner+0x4C,rd(owner+0xF0)+0x4C));
            c->gpr[2]=distance>0x64000&&distance<=0x1F3FFF;
        }break;
    case 14:c->gpr[2]=6;break;
    case 15:c->gpr[2]=0x801F;break;
    }
}
void register_callbacks() {
    psx_mod_register_guest_function_plugin(Plugin,Weapon,weapon);
    psx_mod_register_guest_function_plugin(Plugin,Device,device);
    psx_mod_register_guest_function_plugin(Plugin,Bolt,bolt);
    psx_mod_register_guest_function_plugin(Plugin,Hit,hit);
    psx_mod_register_guest_function_plugin(Plugin,Sparks,sparks);
}
}
