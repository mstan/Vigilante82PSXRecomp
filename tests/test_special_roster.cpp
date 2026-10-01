#include "mod_test_runtime.h"
#include <cmath>
namespace {
using namespace special_roster;
constexpr uint32_t W=0x80100000,O=0x80101000,P=0x80102000,T=0x80103000,C=0x80104000;
constexpr uint32_t B=0x80106000,S=0x80107000,Socket=0x80108000,Stack=0x801F0000,Chunk=0x80340000;
struct Call{uint32_t pc,a,b,d,e;};std::vector<Call> calls;std::vector<uint32_t> made;
uint32_t next;bool allocations;
unsigned spuSlots=100,spuNext=0x20000,playingSample=0,uploadWords=0;
std::array<uint8_t,4+66*4+66*16> samples{};
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
CPUState cpu(uint32_t o,unsigned event,uint32_t arg=0){CPUState c{};c.gpr[4]=o;c.gpr[5]=event;c.gpr[6]=arg;c.gpr[29]=Stack;c.gpr[31]=0x80020000;return c;}
void event(uint32_t o,unsigned e,uint32_t arg=0){
    auto c=cpu(o,e,arg);switch(rd(o)){
    case Quake:quake(&c,Quake);break;case Vortex:vortex(&c,Vortex);break;case Bees:bees(&c,Bees);break;
    case Radar:radar(&c,Radar);break;case Bullet:bullet(&c,Bullet);break;case Fume:fume(&c,Fume);break;
    case Fireball:fireball(&c,Fireball);break;case Horn:horn(&c,Horn);break;case Missile:missile(&c,Missile);break;
    case Disco:disco(&c,Disco);break;case Star:star(&c,Star);break;case Blast:blast_event(&c,Blast);break;
    default:break;
    }
}
uint32_t alloc(uint32_t bank,unsigned kind,unsigned size){
    if(!allocations)return 0;const auto o=next;next+=(size+255)&~255u;made.push_back(o);
    wr(o+0x5C,bank);half(o+0x1A,kind);half(o+0x20,4096);half(o+0x28,4096);half(o+0x30,4096);return o;
}
void engine(CPUState* c,uint32_t pc,uint32_t){
    const auto a=c->gpr[4],b=c->gpr[5],d=c->gpr[6],e=c->gpr[7];calls.push_back({pc,a,b,d,e});uint32_t result=0;
    check(c->gpr[29]<=Stack-0x40,"native ABI frame missing");
    switch(pc){
    case 0x8002C17C:result=alloc(a,b,d);break;
    case 0x8002CBE8:result=alloc(0,0,a);break;
    case 0x80042408:result=alloc(rd(b+0x5C),d,e);break;
    case 0x8002C5F4:result=Socket;break;
    case 0x8002CDF4:result=a+0x20;break;
    case 0x8002C77C:copy(a,P+0x20,32);break;
    case 0x80059AFC:copy(d,a,32);break;
    case 0x80059FBC:copy(d,b,8);break;
    case 0x8002CEAC:copy(a,b+0x20,32);break;
    case 0x80024094:copy(d,b,12);break;
    case 0x8002CCBC:result=a;break;
    case 0x8002CC48:case 0x8002CC9C:wr(b+0xC,rd(a+0x10));wr(a+0x10,b);wr(b+0x14,a);break;
    case 0x8002CD78:result=rd(a+0x14);break;
    case 0x8003066C:copy(a+0x34,a+0x4C,12);event(a,1);break;
    case 0x800309A0:case 0x800309C8:event(a,4);wr(a,0);break;
    case 0x800597BC:result=uint32_t(std::sqrt(double(a)));break;
    case 0x8001DD9C:result=9;break;case 0x8002AC5C:result=0x4000;break;
    case 0x80052F9C:if(spuSlots){--spuSlots;result=spuNext;spuNext+=a;}else result=UINT32_MAX;break;
    case 0x8001E05C:++spuSlots;break;
    case 0x8001B750:case 0x8002CFBC:result=0x70000000;break;
    case 0x80030B78:wr(a+4,rd(a+4)|0x80);break;
    case 0x80030BA8:wr(a+4,rd(a+4)&~0x80u);break;
    case 0x8002C4B4:case 0x8002CA94:case 0x8002CF74:case 0x8002D1DC:case 0x8002A2E0:
    case 0x8002C05C:case 0x800305FC:case 0x80030BF0:case 0x80030080:case 0x8002B1FC:case 0x80030CB0:case 0x8004E414:
    case 0x8003A368:case 0x8001E14C:case 0x8001E580:case 0x8001E628:case 0x8001E098:
    case 0x8001DE78:case 0x8001E2C8:case 0x8001E478:case 0x8001E7A8:break;
    default:throw std::runtime_error("unexpected special engine service "+std::to_string(pc));
    }
    c->gpr[2]=result;c->gpr[4]=0xBAD;c->gpr[31]=0xBAD;c->pc=0xBAD;
}
void reset(unsigned kind){
    memory.fill(0);calls.clear();made.clear();next=0x80500000;allocations=true;spuSlots=100;spuNext=0x20000;playingSample=0;uploadWords=0;
    package.vehicles.resize(13);package.banks.resize(1);package.original_effects_bank=0;init();wr(bank_state(0),S);wr(B+12,B+256);wr(S+12,S+256);
    samples.fill(0);samples[0]=66;samples[2]=132;
    for(unsigned i=0;i<66;++i){samples[4+i*4]=uint8_t(i*2);samples[7+i*4]=4;}package.banks[0].sound={samples.data(),uint32_t(samples.size())};
    for(unsigned i=0;i<13;++i)package.vehicles[i].flags=(1u<<23)|(i<<17);
    wr(state(kind),B);package.special_effect_banks.fill(0);for(auto& map:package.special_effect_kinds)for(unsigned i=0;i<50;++i)map[i]=uint16_t(i);
    enabled=true;remember(O,kind);wr(W+0x5C,B);wr(W+0x10,P);wr(P+0x5C,B);wr(P+0x10,C);wr(C+0x5C,B);
    half(P+0x20,4096);half(P+0x28,4096);half(P+0x30,4096);half(C+0x1A,0x39);
    wr(O+4,0x20000000);half(O+0xA,-1);wr(O+0xF0,T);wr(O+0x4C,100<<16);wr(O+0x54,100<<16);
    half(T+0xA,0);for(unsigned i=0;i<3;++i)wr(O+0x80+i*4,uint32_t(-256));
    auto c=cpu(W,1);weapon(&c,Weapon+kind*4);
}
unsigned count(uint32_t pc){return unsigned(std::count_if(calls.begin(),calls.end(),[=](auto c){return c.pc==pc;}));}
uint32_t find(uint32_t callback){for(auto o:made)if(rd(o)==callback)return o;throw std::runtime_error("weapon did not spawn its callback");}
}
int main(){
    test_dispatch=engine;
    test_spu_read=[](uint32_t p){return p==0x1F801C06?playingSample/8:p==0x1F801C0C?unsigned(playingSample!=0):0u;};
    test_spu_write=[](uint32_t,uint32_t){};test_spu_dma=[](uint32_t){++uploadWords;};
    constexpr unsigned Kinds[]={0,2,3,4,5,6,8,9,10,11};
    constexpr uint32_t Shots[]={Radar,Bullet,Quake,Vortex,Horn,Missile,Disco,Bees,Fume,Fireball};
    for(unsigned j=0;j<10;++j){
        const unsigned kind=Kinds[j];reset(kind);auto c=cpu(W,0);c.gpr[18]=O;c.gpr[17]=0x80000000;instruction(&c,0x8004AF0C);check(c.gpr[17]==Weapon+kind*4,"original still selects retail callback");
        const auto ammo=psx_mod_read_half(W+0x1C);allocations=false;c=cpu(W,12,O);weapon(&c,Weapon+kind*4);
        check(psx_mod_read_half(W+0x1C)==ammo&&!rd(Active)&&!count(0x8001E14C),"allocation failure consumed ammo, leaked active state or spoke");
        allocations=true;calls.clear();c=cpu(W,12,O);weapon(&c,Weapon+kind*4);
        check(c.gpr[2]&&c.gpr[29]==Stack&&c.gpr[31]==0x80020000&&c.gpr[4]==W,"fire lost cooldown or CPU state");
        check(psx_mod_read_half(W+0x1C)==ammo-1&&count(0x8001E14C)==1,"successful trigger must spend one round and speak");
        check(find(Shots[j])!=0,"missing original effect");
    }
    // Sheila's projectile, repeated-fire cadence, and final empty-weapon path.
    reset(2);auto c=cpu(W,12,O);weapon(&c,Weapon+8);const auto bullet=find(Bullet);
    check(c.gpr[2]==12&&psx_mod_read_half(bullet+0x1C)==20&&psx_mod_read_half(bullet+0x94)==180,"Sheila cadence, damage or lifetime changed");
    check(si(bullet+0x88)==-2&&si(bullet+0x8C)==-2&&si(bullet+0x90)==24574,"Sheila lost inherited velocity or muzzle axis");
    for(unsigned n=1;n<50;++n){c=cpu(W,12,O);weapon(&c,Weapon+8);}check(!psx_mod_read_half(W+0x1C)&&count(0x8003A368)==1&&count(0x8001E14C)==1,"held trigger voice/ammo/empty retirement changed");
    // Torque's terrain edits must restore exactly, including early teardown.
    for(bool early:{false,true}){
        reset(3);wr(0x800B9470+(1*32+1)*4,Chunk);for(unsigned i=0;i<8192;i+=2)half(Chunk+i,0x1234);
        // Populate the native sine/cosine table for a meaningful wave.
        for(unsigned a=0;a<4096;++a){half(0x80065C90+a*4,int(std::sin(a*6.283185307179586/4096)*4096));half(0x80065C90+a*4+2,int(std::cos(a*6.283185307179586/4096)*4096));}
        c=cpu(W,12,O);weapon(&c,Weapon+12);const auto q=find(Quake);bool changed=false;
        for(unsigned tick=0;tick<(early?100u:317u);++tick){event(q,0,1);for(unsigned i=0;i<8192;i+=2)changed|=psx_mod_read_half(Chunk+i)!=0x1234;}
        if(early){event(q,4);wr(q,0);}check(changed,"Torque never modified terrain");
        for(unsigned i=0;i<8192;i+=2)check(psx_mod_read_half(Chunk+i)==0x1234,"Torque left permanent terrain deformation");check(!rd(Active),"Torque leaked active-special gate");
    }
    reset(3);wr(O+4,0);c=cpu(W,12,O);weapon(&c,Weapon+12);check(psx_mod_read_half(W+0x1C)==3&&made.empty(),"airborne quake spent ammo");
    check(!terrain_cell(-1,0)&&!terrain_cell(2048,0)&&!terrain_cell(0,-1)&&!terrain_cell(0,2048),"quake wraps map-edge storage");
    // Fill a two-sample SPU budget, preserve keyed/playing audio, reclaim only
    // the finished sample, and release the cache on a match transition.
    reset(2);spuSlots=2;c=cpu(W,12,O);const auto first=common_sample(&c,0),second=common_sample(&c,1);
    check(first&&second&&uploadWords==8,"sample uploads lost source lengths");
    check(common_sample(&c,0)==first&&uploadWords==8,"cached sound uploaded twice");
    check(!common_sample(&c,2)&&!count(0x8001E05C),"newly keyed samples were reclaimed");
    playingSample=psx_mod_read_half(first+2)*8;wr(0x80065480,5);
    check(common_sample(&c,2)&&count(0x8001E05C)==1&&rd(sound_cache())==first&&!rd(sound_cache()+12),"SPU pressure did not preserve playing sound");
    release_sounds(&c);check(!rd(bank_state(0)+16)&&spuSlots==2,"match teardown leaked cached samples");
    // Refilled weapon racks must regain their visible rounds, not only ammo.
    reset(5);half(W+0x1C,2);wr(T+0x10,C);c=cpu(W,16,T);weapon(&c,Weapon+20);
    check(rd(W+0x10)==C&&signed_half(C+0xA)==0,"Convoy refill lost visible horn");
    reset(6);half(C+0xA,2);wr(T+0x10,T+256);wr(T+256+0x10,T+512);half(T+512+0xA,1);
    c=cpu(W,16,T);weapon(&c,Weapon+24);check(rd(P+0x10)==T+512,"Loki refill lost visible missile");
    reset(6);c=cpu(W,12,O);weapon(&c,Weapon+24);const auto missile=find(Missile);
    for(unsigned tick=0;tick<30;++tick)event(missile,0,1);
    check(count(0x80030080)==1&&count(0x80030BF0)==1&&!count(0x800305FC),"Loki launch duplicated the already registered update callback");
    // Destroying an owner during a special must release the shared AI gate.
    for(unsigned kind:{0u,6u,8u}){
        reset(kind);c=cpu(W,12,O);weapon(&c,Weapon+kind*4);const auto active=rd(Active);
        check(active!=0,"missing active-special ownership");
        for(auto o:made)if(rd(o)==Radar||rd(o)==Missile||rd(o)==Disco)event(o,4);
        check(!rd(Active),"early destruction leaked the active-special gate");
    }
    // Chassey's direct-color effect is a 1x1 16-bit TIM, without a CLUT.
    // Omitting it from the reservation pass used to abort on first impact.
    reset(0);constexpr uint32_t bin=0x80200000;
    wr(bin+16,1);wr(bin+20,24);wr(bin+24,8);wr(bin+36,2);wr(bin+40,15);
    half(bin+48,1);half(bin+50,1);half(bin+52,0x7FFF);
    v82::Bank effectBank{};effectBank.bin={memory.data()+(bin&0x1FFFFFFF),72};requests.clear();scan_bank(effectBank);scan_bank(effectBank);
    check(requests.size()==1&&requests[0].args==std::array<uint32_t,6>{1,1,64,256,256,256}&&requests[0].pixels.size==3,"direct-color effect reservation missing or duplicated");
    std::cout<<"All ten weapons: native selection, spawn, voice, ammo, failure cleanup; Sheila cadence; Torque terrain restoration passed\n";
}
