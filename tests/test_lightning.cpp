#include "mod_test_runtime.h"
namespace {
constexpr uint32_t W=0x80100000,O=0x80101000,J=0x80102000,C=0x80103000;
constexpr uint32_t D=0x80104000,B=0x80105000,H=0x80106000,T=0x80107000;
constexpr uint32_t Bank=0x80108000,Stack=0x801F0000;
constexpr uint32_t Matrix=0x80109000;
struct Call {uint32_t pc,a0,a1,a2,a3;};
std::vector<Call> calls;
bool fail_device=false;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void engine(CPUState* c,uint32_t pc,uint32_t) {
    check(c->gpr[29]==Stack-0x40,"outgoing ABI frame missing");
    calls.push_back({pc,c->gpr[4],c->gpr[5],c->gpr[6],c->gpr[7]});
    uint32_t value=0;
    switch(pc) {
    case 0x8002C17C:value=c->gpr[5]==0x3F?(fail_device?0:D):B;break;
    case 0x8002CBE8:value=H;break;
    case 0x8002AC5C:value=0x4000;break;
    case 0x8001DD9C:value=9;break;
    case 0x8002CD78:value=c->gpr[4]==D?J:W;break;
    case 0x8002CDF4:value=Matrix;break;
    case 0x8002CCBC:case 0x800305FC:case 0x8003A368:break;
    case 0x8002C05C:case 0x8002CC9C:case 0x8001E580:case 0x8001E14C:
    case 0x8003066C:case 0x800309A0:case 0x80030BA8:case 0x8002FEE8:break;
    default:throw std::runtime_error("unexpected lightning engine service");
    }
    c->gpr[2]=value;
}
CPUState cpu(uint32_t object,unsigned event,uint32_t owner=0) {
    CPUState c{};c.gpr[4]=object;c.gpr[5]=event;c.gpr[6]=owner;
    c.gpr[29]=Stack;c.gpr[31]=0x80020000;return c;
}
unsigned count(uint32_t pc){return unsigned(std::count_if(calls.begin(),calls.end(),[=](auto a){return a.pc==pc;}));}
}
int main() {
    test_dispatch=engine;enabled=true;package.vehicles.resize(1);
    package.vehicles[0].flags=(1u<<23)|(1u<<17);remember(O,0);
    auto c=cpu(O,0);c.gpr[18]=O;c.gpr[17]=0x80049880;instruction(&c,0x8004AF0C);
    check(c.gpr[17]==lightning::Weapon,"Clyde still selects the ammo-only fallback");
    wr(W+0x5C,Bank);wr(Bank+12,Bank+0x100);wr(W+0x10,J);wr(J+0x10,C);
    wr(O+0xF0,T);psx_mod_write_half(O+0xA,0xFFFF);
    c=cpu(W,1);lightning::weapon(&c,lightning::Weapon);
    check(psx_mod_read_half(W+0x1C)==3&&psx_mod_read_byte(W+8)==3&&psx_mod_read_byte(W+0x18)==0,"initial ammo/category corrupted the animation field");
    calls.clear();fail_device=true;c=cpu(W,12,O);lightning::weapon(&c,lightning::Weapon);
    check(psx_mod_read_half(W+0x1C)==3&&rd(lightning::Active)==0&&count(0x8001E14C)==0,"allocation failure spent ammo or spoke");
    fail_device=false;calls.clear();c=cpu(W,12,O);lightning::weapon(&c,lightning::Weapon);
    check(c.gpr[2]==120&&psx_mod_read_half(W+0x1C)==2&&rd(lightning::Active)==1,"successful shot did not consume exactly one round");
    check(count(0x8001E580)==1&&count(0x8001E14C)==1,"shot effect or original voice missing");
    check(rd(D)==lightning::Device&&rd(D+0x84)==T&&psx_mod_read_half(D+0x1C)==300,"device target/damage missing");
    wr(D+0x5C,Bank);wr(T+0x4C,0x100000);wr(T+0x50,0x200000);wr(T+0x54,0x300000);
    wr(D+0xC,C);wr(C+0x64,0x80110000);
    psx_mod_write_half(Matrix+2,4096);psx_mod_write_half(Matrix+8,uint16_t(-4096));
    calls.clear();c=cpu(D,5);lightning::device(&c,lightning::Device);
    check(c.gpr[2]==UINT32_MAX&&rd(C+0x64)==0&&rd(D+4)==0x90,"animation completion did not release the device");
    check(int32_t(rd(D+0x88))==-6103&&rd(D+0x8C)==6103&&rd(D+0x90)==0,"launch velocity differs from source");
    check(count(0x8002CCBC)==1&&count(0x800305FC)==1&&count(0x8003A368)==0,"launch registration or ammo retention wrong");
    psx_mod_write_half(W+0x1C,0);calls.clear();c=cpu(D,5);lightning::device(&c,lightning::Device);
    check(count(0x8003A368)==1,"last round did not retire its empty weapon at launch");
    unsigned strikes=0;
    for(unsigned tick=1;tick<=121;++tick) {
        calls.clear();c=cpu(D,0);lightning::device(&c,lightning::Device);
        const auto allocations=count(0x8002C17C);
        check(allocations==unsigned(tick==30||tick==60||tick==90),"strike cadence differs from source at30/60/90");
        strikes+=allocations;
        if(allocations) {
            check(rd(B)==lightning::Bolt&&rd(H)==lightning::Hit&&rd(B+0x80)==H,"visual/collider not linked");
            check(rd(B+0x50)==0x138000&&rd(H+0x50)==0x138000,"strike height or collider transform differs");
            check(psx_mod_read_half(H+0x1C)==300&&rd(H+0x5C)==Bank,"damage or sound/impact ownership lost");
            check(count(0x8001E580)==1&&count(0x8003066C)==2,"thunder or world registration missing");
        }
    }
    check(strikes==3&&rd(lightning::Active)==0&&c.gpr[2]==UINT32_MAX,"device retirement leaks active count");
    std::cout<<"Clyde callback, original voice, ammo failure, three native strikes and retirement passed\n";
}
