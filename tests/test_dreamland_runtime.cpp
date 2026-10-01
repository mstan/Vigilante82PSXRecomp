#include "mod_test_runtime.h"
#include "../src/mods/v82_dreamland_plugin.cpp"

namespace {
constexpr uint32_t Stack=0x801F0000,Effect=0x80110000,Particle=0x80111000,Position=0x80112000;
struct Call {uint32_t pc;std::array<uint32_t,4> args;};
std::vector<Call> calls;
unsigned allocations=0;
unsigned sound_allocations=0;
uint32_t sound_result=UINT32_MAX;
void require(bool b,const char* why) {if(!b)throw std::runtime_error(why);}
void native(CPUState* c,uint32_t pc,uint32_t) {
    require(c->gpr[29]==Stack-0x40,"helper did not isolate outgoing ABI frame");
    calls.push_back({pc,{c->gpr[4],c->gpr[5],c->gpr[6],c->gpr[7]}});
    uint32_t result=0;
    switch(pc) {
    case dreamland::SpuAlloc:
        ++sound_allocations;result=sound_result;
        break;
    case 0x8003A020:result=19;break;
    case 0x8001E580:case 0x8001E628:case 0x8001E098:result=7;break;
    case 0x80017F34:case 0x80017EB8:break;
    case 0x80150000:result=c->gpr[5]==19?0:123;break;
    case 0x80030250:result=c->gpr[5]==UINT32_MAX?0x80152000:0;break;
    case 0x8002CF74:
        for(unsigned i=0;i<12;i+=4)wr(c->gpr[4]+0x34+i,rd(c->gpr[4]+0x4C+i));break;
    case 0x8002C17C:result=allocations++?Particle:Effect;break;
    case 0x8002A39C:case 0x800305FC:case 0x80030CB0:case 0x800309A0:break;
    case 0x8001DD9C:result=4;break;
    case 0x8002AC5C:result=16384;break;
    case 0x8002CC48:wr(c->gpr[4]+0x10,c->gpr[5]);break;
    case 0x80029F6C:result=rd(c->gpr[5]);break;
    case 0x8002CD78:result=Effect;break;
    case 0x8FFF0400:result=123;break;
    default:throw std::runtime_error("unexpected native helper call");
    }
    c->gpr[2]=result;c->gpr[4]=0xBAD;c->gpr[31]=0xBAD;c->pc=0xBAD;
}
CPUState cpu(uint32_t a0=0,uint32_t a1=0,uint32_t a2=0,uint32_t a3=0) {
    CPUState c{};c.gpr[4]=a0;c.gpr[5]=a1;c.gpr[6]=a2;c.gpr[7]=a3;
    c.gpr[29]=Stack;c.gpr[31]=0x80012000;c.gpr[28]=0x80060000;return c;
}
}
int main() {
    test_dispatch=native;
    auto c=cpu(Effect,50);const auto saved=c;
    dreamland::helper(&c,dreamland::Hazard);
    require(c.gpr[2]==19&&c.gpr[4]==saved.gpr[4]&&c.gpr[31]==saved.gpr[31],"hazard result/ABI");
    require(calls.back().args[1]==uint32_t(-50)&&calls.back().args[3]==1,"hazard sign/impulse flag");
    require(!rd(Stack-16)&&!rd(Stack-12)&&!rd(Stack-8),"hazard impulse vector");
    wr(0x8006B41C,0x800B1234);
    for(const auto mode:{dreamland::Sound3d,dreamland::SoundAlt,dreamland::Sound}) {
        c=cpu(3,0xBAD,0x5F,Position);dreamland::helper(&c,mode);
        require(c.gpr[2]==7&&calls.back().args==std::array<uint32_t,4>{3,0x800B1234,89,Position},"sound mapping/native bank");
    }
    c=cpu(1,2,3,4);c.gpr[2]=91;dreamland::helper(&c,dreamland::WaterInit);
    require(c.gpr[2]==91&&c.gpr[6]==3&&calls.back().args[0]==0x2A400&&calls.back().args[1]==0x7FFF0000,"water initialization preserves callback args");
    for(unsigned i=0;i<12;i+=4)wr(Position+i,0x11220000+i);
    c=cpu(Position,0x800B0000,9,10);wr(Stack+16,0x45);wr(Stack+20,3);wr(Stack+24,12);
    dreamland::helper(&c,dreamland::Contact);
    require(c.gpr[2]==Effect&&rd(Effect)==dreamland::ContactUpdate,"contact callback");
    require(rd(Effect+0x34)==rd(Position)&&rd(Effect+0x3C)==rd(Position+8)&&rd(Effect+0x98)==0x800B0000,"contact ownership/position");
    require(psx_mod_read_half(Effect+0x82)==3&&psx_mod_read_half(Effect+0x96)==10,"contact stack arguments");
    c=cpu(Effect,0);dreamland::helper(&c,dreamland::ContactUpdate);
    require(rd(Particle)==0x8004CA68&&rd(Effect+0x10)==Particle&&psx_mod_read_half(Effect+0x80)==3,"contact particle native hierarchy");
    require(rd(Particle+0x34)==0&&rd(Particle+0x3C)==0,"contact signed random radius");
    c=cpu(Effect,5);dreamland::helper(&c,dreamland::ContactUpdate);
    require(c.gpr[2]==UINT32_MAX&&!rd(Effect+0x64)&&(rd(Effect+4)&2),"contact retirement");
    wr(Effect,0x8FFF0400);c=cpu(0x80120000,17,0x80130000);
    dreamland::helper(&c,dreamland::ModelEvent);
    require(c.gpr[2]==123&&calls.back().args[0]==Effect&&calls.back().args[1]==17,"legacy child event");
    // Exact ID wins over a closer family match; dead objects are ignored.
    const uint32_t root=0x80140000,node1=root+16,node2=root+32,a=0x80141000,b=0x80142000;
    wr(0x8006B830,root);wr(root,node1);wr(node1,node2);wr(node1+8,a);wr(node2,0);wr(node2+8,b);
    for(auto obj:{a,b}){psx_mod_write_byte(obj+8,2);psx_mod_write_half(obj+0x1C,100);}
    psx_mod_write_half(a+0x1A,uint16_t(-2));psx_mod_write_half(b+0x1A,uint16_t(-3));
    psx_mod_write_byte(0x8006AD75,5);psx_mod_write_byte(0x8006AD76,5);wr(a+0x4C,100);wr(b+0x4C,1);
    c=cpu(Position,uint32_t(-2));dreamland::helper(&c,dreamland::Nearest);require(c.gpr[2]==a,"nearest exact precedence");
    psx_mod_write_half(a+0x1C,0);dreamland::helper(&c,dreamland::Nearest);require(c.gpr[2]==b,"nearest family fallback");

    // Imported file state survives CPU replacement and leaves retail reads alone.
    memory.fill(0);dreamland::active=true;dreamland::force_match=true;
    std::array<uint8_t,7> bytes{1,2,3,4,5,6,7};dreamland::files[0].data=bytes.data();dreamland::files[0].size=bytes.size();
    for(const auto& h:dreamland::Hooks)wr(h.address,h.word);
    c=cpu();dreamland::entry(&c,dreamland::Level);require(dreamland::path_at(c.gpr[4])==dreamland::files[0].path,"level path");
    dreamland::entry(&c,dreamland::Resolve);const auto descriptor=c.gpr[2];
    require(rd(descriptor+12)==dreamland::Extent&&rd(descriptor+16)==7,"native descriptor");
    c=cpu(dreamland::Extent);dreamland::entry(&c,dreamland::Begin);
    c=cpu(Position,3);dreamland::entry(&c,dreamland::Read);
    require(psx_mod_read_byte(Position)==1&&psx_mod_read_byte(Position+2)==3,"partial file read");
    c=cpu();dreamland::entry(&c,dreamland::ReadByte);require(c.gpr[2]==4,"mixed byte/block reads");
    c=cpu(1,1);dreamland::entry(&c,dreamland::Seek);
    c=cpu(Position,4);dreamland::entry(&c,dreamland::Read);
    require(psx_mod_read_byte(Position)==6&&psx_mod_read_byte(Position+1)==7&&!psx_mod_read_byte(Position+2),"seek and sector padding");
    require(rd(c.gpr[28]+0xD64)==9,"saved native byte offset");
    c=cpu();dreamland::entry(&c,dreamland::End);require(!rd(dreamland::Meta+4),"file retirement");
    c=cpu(200);dreamland::entry(&c,dreamland::Begin);require(c.pc==0&&!rd(dreamland::Meta+4),"retail reader changed");
    c=cpu(Position,dreamland::Extent,1);dreamland::entry(&c,dreamland::WholeRead);
    require(c.gpr[2]==Position&&psx_mod_read_byte(Position)==1&&psx_mod_read_byte(Position+6)==7&&!psx_mod_read_byte(Position+2047),"whole-sector read must return buffer for native realloc");
    const auto path=Position;
    for(unsigned i=0;i<=std::strlen(dreamland::files[2].path);++i)
        psx_mod_write_byte(path+i,dreamland::files[2].path[i]);
    for(unsigned arena=0;arena<19;++arena) {
        c=cpu(path);psx_mod_write_byte(c.gpr[28]+0xC28,uint8_t(arena));
        dreamland::entry(&c,dreamland::Resolve);
        require(arena==18?c.gpr[2]==dreamland::Meta+0x140:!c.pc,
                "Dreamland sound bank leaked into a retail arena");
    }
    c=cpu(422848);c.gpr[31]=0x8001DEE8;wr(dreamland::Meta+4,3);wr(c.gpr[28]+0xD64,4);
    wr(0x800BDD78,0x1010);wr(0x800BDD7C,0x71C0);
    dreamland::entry(&c,dreamland::SpuAlloc);
    require(c.gpr[2]==UINT32_MAX&&sound_allocations==1,"failed allocation must not reset/retry over live car banks");
    require(rd(0x800BDD78)==0x1010&&rd(0x800BDD7C)==0x71C0,"live sound allocation overwritten");
    sound_result=0x81D0;c=cpu(100);dreamland::entry(&c,dreamland::SpuAlloc);
    require(c.gpr[2]==0x81D0&&sound_allocations==2,"native allocator result changed");
    // Native recovery event 19 must place the car at an authored start while
    // every existing arena event retains its result and argument contract.
    memory.fill(0);calls.clear();c=cpu();
    const auto gp=c.gpr[28],callback=0x80150000,car=0x80151000,start=0x80152000;
    wr(gp+0x104C,callback);wr(callback,0x27BDFFC8);
    dreamland::install_water_recovery(&c);dreamland::install_water_recovery(&c);
    require(rd(gp+0x104C)==dreamland::ArenaEvent&&rd(dreamland::RecoveryState+4)==callback,"arena wrapper lost original callback");
    c=cpu(car,17,33,44);dreamland::helper(&c,dreamland::ArenaEvent);
    require(c.gpr[2]==123&&calls.back().args==std::array<uint32_t,4>{car,17,33,44},"existing arena event changed");
    psx_mod_write_byte(car+8,2);psx_mod_write_half(car+0x1C,750);
    psx_mod_write_byte(start+8,5);psx_mod_write_half(start+0xA,0xFFFF);psx_mod_write_half(car+0xA,0xFFFF);
    for(unsigned i=0;i<12;i+=4)wr(start+0x4C+i,0x100000+i);
    for(unsigned i=0;i<6;i+=2)psx_mod_write_half(start+0x44+i,uint16_t(100+i));
    for(unsigned i=0;i<0x1C;i+=4)wr(car+0x80+i,0x76543210);
    c=cpu(car,19);const auto saved_recovery=c;dreamland::helper(&c,dreamland::ArenaEvent);
    require(!c.gpr[2]&&c.gpr[4]==saved_recovery.gpr[4]&&c.gpr[31]==saved_recovery.gpr[31],"recovery ABI changed");
    require(calls[calls.size()-2].pc==0x80030250&&calls[calls.size()-2].args[0]==gp+0x1078&&calls[calls.size()-2].args[1]==UINT32_MAX,"recovery must use native authored player start");
    require(calls.back().pc==0x8002CF74&&calls.back().args[0]==car,"native transform update missing");
    for(unsigned i=0;i<12;i+=4)require(rd(car+0x34+i)==rd(start+0x4C+i)&&rd(car+0x4C+i)==rd(start+0x4C+i),"recovery position mismatch");
    for(unsigned i=0;i<6;i+=2)require(psx_mod_read_half(car+0x44+i)==psx_mod_read_half(start+0x44+i),"recovery orientation mismatch");
    for(unsigned i=0;i<0x1C;i+=4)require(!rd(car+0x80+i),"sinking velocity survived recovery");
    require(psx_mod_read_half(car+0x1C)==750,"recovery changed health");
    psx_mod_write_half(start+0xA,0);calls.clear();dreamland::helper(&c,dreamland::ArenaEvent);
    require(calls.size()==5&&calls.back().pc==0x80030250,"invalid authored start applied");
    wr(gp+0x104C,0x80153000);wr(0x80153000,0);dreamland::install_water_recovery(&c);
    require(rd(gp+0x104C)==0x80153000,"unrecognized arena callback replaced");
    std::cout<<"Dreamland native helpers and imported file lifecycle passed\n";
}
