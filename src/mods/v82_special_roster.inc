/* Original V8 weapons in the V8:2 object/event ABI.
 * Adapted with owner-reported author permission from vehicle_init_joints.c,
 * reference revision 6fc8179 (tools/third_party/NOTICE.md). Missing paths and incorrect callback
 * identities checked against the owner's SLUS_005.10. Assets are imported
 * separately from verified owner media. All persistent state lives in guest
 * objects, including timers, targets and the shared bank handle. */
namespace special_roster {
using original_specials::invoke;
using original_specials::sound;
using original_specials::voice;
using original_specials::signed_half;
constexpr uint32_t Weapon=0x8FFF0200, Radar=0x8FFF0240, RadarHit=0x8FFF0244;
constexpr uint32_t Bullet=0x8FFF0248, Fume=0x8FFF024C, Fireball=0x8FFF0250;
constexpr uint32_t Quake=0x8FFF0254, Vortex=0x8FFF0258, Trail=0x8FFF025C;
constexpr uint32_t Horn=0x8FFF0260, Missile=0x8FFF0264, Disco=0x8FFF0268;
constexpr uint32_t Star=0x8FFF026C, Bees=0x8FFF0270, StopAnim=0x8FFF0274;
constexpr uint32_t Blast=0x8FFF0278, Active=0x8006B874;
constexpr uint32_t MissileTrail=0x8FFF027C, Bomblet=0x8FFF0280, BombletHit=0x8FFF0284, BeeDepart=0x8FFF0288;
constexpr const char* Names[]={"chassey","clyde","sheila","torque","dave","convoy","loki","houston","boogie","beezwax","molo","sid","y"};
struct Scratch {
    CPUState* c; uint32_t saved, p;
    explicit Scratch(CPUState* cpu):c(cpu),saved(cpu->gpr[29]),p(saved-0x100){c->gpr[29]=p;}
    ~Scratch(){c->gpr[29]=saved;}
};
int32_t si(uint32_t p){return int32_t(rd(p));}
void half(uint32_t p,int32_t v){psx_mod_write_half(p,uint16_t(v));}
void copy(uint32_t to,uint32_t from,unsigned n){for(unsigned i=0;i<n;i+=4)wr(to+i,rd(from+i));}
int32_t mul(int32_t a,int32_t b){return int32_t(uint32_t(a)*uint32_t(b));}
int32_t add(int32_t a,int32_t b){return int32_t(uint32_t(a)+uint32_t(b));}
int32_t sub(int32_t a,int32_t b){return int32_t(uint32_t(a)-uint32_t(b));}
bool grounded(uint32_t o){return (rd(o+4)&0x20000000)!=0;} // original 8002FC98 -> native 8003F25C
int32_t trig(int angle,bool cosine=false){return signed_half(0x80065C90+((unsigned(angle)&4095)*4)+(cosine?2:0));}
uint32_t random(CPUState* c){return invoke(c,0x8002AC5C);}
void counter(unsigned kind,const char* what){
    // The counter API retains the name pointer for the runtime's lifetime.
    static const auto keys=[] {
        std::array<std::array<std::string,10>,13> result;
        constexpr const char* events[]={"fired","retired","vehicle_hits","terrain_ticks","launched","bomblets","bomblets_retired","stars","stars_launched","stars_retired"};
        for(unsigned k=0;k<13;++k)for(unsigned e=0;e<10;++e)result[k][e]=std::string("v82.special.")+Names[k]+"."+events[e];
        return result;
    }();
    for(const auto& key:keys.at(kind))if(key.compare(key.size()-std::strlen(what),std::strlen(what),what)==0){psx_mod_counter_add(key.c_str(),1);return;}
    fail("v82.special.unknown_counter");
}
uint32_t shared(CPUState* c){
    const auto bank=rd(c->gpr[4]+0x5C);int kind=-1;
    for(unsigned i=0;i<package.vehicles.size();++i)if(bank&&rd(state(i))==bank){kind=int((package.vehicles[i].flags>>17)&31);break;}
    for(unsigned i=0;kind<0&&i<13;++i)if(package.special_effect_banks[i]!=65535&&bank&&rd(bank_state(package.special_effect_banks[i]))==bank)kind=int(i);
    if(kind<0||kind>=13||package.special_effect_banks[kind]==65535)fail("v82.special.missing_effect_bank");
    Scratch s(c);return build_bank(c,package.special_effect_banks[kind]);
}
uint32_t model(CPUState* c,uint32_t bank,unsigned kind,unsigned size=0x98,unsigned flags=8){
    for(unsigned i=0;i<13;++i)if(package.special_effect_banks[i]!=65535&&bank&&rd(bank_state(package.special_effect_banks[i]))==bank){
        if(kind>=50||package.special_effect_kinds[i][kind]==65535)fail("v82.special.unmapped_effect");kind=package.special_effect_kinds[i][kind];break;
    }
    return invoke(c,0x8002C17C,bank,kind,size,flags);
}
void children(uint32_t o,uint32_t callback){for(auto n=rd(o+0x10);n;n=rd(n+0xC))wr(n,callback);}
void play(CPUState* c,uint32_t bank,unsigned sample,uint32_t pos=0){
    if(pos)invoke(c,0x8001E628,voice(c),rd(bank+12),sample,pos);
    else invoke(c,0x8001E14C,voice(c),rd(bank+12),sample);
}
uint32_t sound_cache(){
    if(package.original_effects_bank==65535)return 0;
    const auto p=bank_state(package.original_effects_bank)+16;
    if(!rd(p)){
        const auto count=package.banks[package.original_effects_bank].sound.u16(0);
        const auto cache=allocate(count*12);if(!cache)fail("v82.special.sound_cache_memory");
        for(unsigned i=0;i<count*12;i+=4)wr(cache+i,0);wr(p,cache);
    }
    return rd(p);
}
bool sample_playing(uint32_t entry){
    const auto handle=rd(entry);if(!handle)return false;
    // Match start addresses, not voice IDs: native channels are recycled.
    const auto start=uint32_t(psx_mod_read_half(handle+2))*8,end=start+rd(entry+4);
    for(unsigned v=0;v<24;++v){const auto p=0x1F801C00+v*16;const auto at=spu_read(p+6)*8;
        if(at>=start&&at<end&&spu_read(p+12))return true;
    }
    // The engine also protects newly keyed voices for two audio ticks.
    return uint8_t(rd(0x80065480)-rd(entry+8))<2;
}
void release_sounds(CPUState* c){
    if(package.original_effects_bank==65535)return;
    const auto p=bank_state(package.original_effects_bank)+16,cache=rd(p);if(!cache)return;
    const auto count=package.banks[package.original_effects_bank].sound.u16(0);
    for(unsigned i=0;i<count;++i)if(const auto handle=rd(cache+i*12))invoke(c,0x8001E05C,handle);
    release(cache);wr(p,0);
}
uint32_t common_sample(CPUState* c,unsigned sample){
    const auto& source=package.banks.at(package.original_effects_bank).sound;
    const auto count=source.u16(0);if(sample>=count)fail("v82.special.sound_index");
    const auto cache=sound_cache(),entry=cache+sample*12;
    if(!rd(entry)){
        const unsigned start=source.u16(4+sample*4)*8,end=(sample+1==count?source.u16(2):source.u16(8+sample*4))*8,bytes=end-start;
        auto address=invoke(c,0x80052F9C,(bytes+63)&~63u);
        if(address==UINT32_MAX){
            for(unsigned i=0;i<count;++i){const auto old=cache+i*12;if(rd(old)&&!sample_playing(old)){invoke(c,0x8001E05C,rd(old));wr(old,0);psx_mod_counter_add("v82.special.sound_reclaimed",1);}}
            address=invoke(c,0x80052F9C,(bytes+63)&~63u);
        }
        if(address==UINT32_MAX){psx_mod_counter_add("v82.special.sound_busy",1);return 0;}
        if(!address||address>=0x80000||bytes>0x80000-address)fail("v82.special.sound_range");
        const auto handle=allocate(8);if(!handle)fail("v82.special.sound_handle");
        half(handle,1);half(handle+2,address>>3);half(handle+4,address>>3);half(handle+6,source.u16(6+sample*4));
        const auto transfer=spu_read(0x1F801DA6);spu_write(0x1F801DA6,address>>3);
        for(unsigned n=0;n<bytes;n+=4)spu_dma_write(source.u32(4+count*4+start+n));spu_write(0x1F801DA6,transfer);
        wr(entry,handle);wr(entry+4,bytes);psx_mod_counter_add("v82.special.sound_bytes",bytes);
    }
    wr(entry+8,rd(0x80065480));return rd(entry);
}
void common_sound(CPUState* c,unsigned sample,uint32_t pos){
    const auto handle=common_sample(c,sample);if(!handle)return;
    if(pos)invoke(c,0x8001E628,voice(c),handle,0,pos);else invoke(c,0x8001E14C,voice(c),handle,0);
}
void phrase(CPUState* c,uint32_t o,unsigned sample){invoke(c,0x8001E14C,voice(c),sound(o),sample);}
void move(uint32_t o,unsigned velocity=0x88,bool relative=true){
    const auto p=o+(relative?0x4C:0x34);
    for(unsigned i=0;i<12;i+=4)wr(p+i,rd(p+i)+rd(o+velocity+i));
    copy(o+(relative?0x34:0x4C),p,12);
}
void finish(CPUState* c,uint32_t o,bool ground=false){invoke(c,0x800309C8,o,ground?1:0);}
uint32_t fx(CPUState* c,uint32_t pos,unsigned kind){
    const auto o=model(c,shared(c),kind,0x80);if(!o)return 0;
    psx_mod_write_byte(o+8,1);wr(o+4,0x34);copy(o+0x4C,pos,12);
    wr(o,0x8004CA68);children(o,0x8004CA10);invoke(c,0x8003066C,o);return o;
}
uint32_t blast(CPUState* c,uint32_t pos,unsigned kind,unsigned power){
    const auto o=model(c,shared(c),kind,0x80);if(!o)return 0;
    psx_mod_write_byte(o+8,8);wr(o+4,power?0x104:0x124);
    if(rd(o+0x60))wr(o+0x58,rd(rd(o+0x60)+0x10));
    copy(o+0x4C,pos,12);half(o+0x1C,int(power)/12);wr(o,Blast);
    children(o,0x8004CA10);invoke(c,0x8003066C,o);return o;
}
void blast_event(CPUState* c,uint32_t){
    const auto o=c->gpr[4],event=c->gpr[5],hit=c->gpr[6]?rd(c->gpr[6]):0;c->gpr[2]=0;
    if(event==5){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
    else if(event==3&&hit&&psx_mod_read_byte(hit+8)==2){
        Scratch s(c);invoke(c,0x8002A168,s.p,o+0x34,hit+0x34);
        for(unsigned i=0;i<3;++i)wr(s.p+16+i*4,uint32_t(signed_half(s.p+i*2)*(i==1?12:24)));
        invoke(c,0x8002B370,hit,s.p+16,o+0x34);
        if(signed_half(hit+0xA)<0)invoke(c,0x80015B00,uint32_t(~signed_half(hit+0xA)),255,2,128);
    }
}
void worldshot(uint32_t shot,uint32_t owner,uint32_t callback,uint32_t flags,unsigned damage){
    wr(shot,callback);wr(shot+4,flags);psx_mod_write_byte(shot+8,8);
    half(shot+0xA,signed_half(owner+0xA));half(shot+0x1C,damage);wr(shot+0x80,owner);
}
void velocity(uint32_t shot,uint32_t owner,int scale,int divisor=1){
    for(unsigned i=0;i<3;++i)wr(shot+0x88+i*4,uint32_t(add(si(owner+0x80+i*4)/128,signed_half(shot+0x24+i*6)*scale/divisor)));
}
void recoil(CPUState* c,uint32_t owner,uint32_t mount,uint32_t pos,int strength){
    Scratch s(c);wr(s.p,0);wr(s.p+4,0);wr(s.p+8,uint32_t(-strength));
    invoke(c,0x80024094,mount+0x20,s.p,s.p+16);invoke(c,0x8002B1FC,owner,s.p+16,pos);
}
void consume(CPUState* c,uint32_t o,unsigned kind,bool remove=true){
    const auto ammo=uint16_t(psx_mod_read_half(o+0x1C)-1);half(o+0x1C,ammo);
    counter(kind,"fired");if(!ammo&&remove)invoke(c,0x8003A368,o);
}
void radar_hit(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],hit=c->gpr[6]?rd(c->gpr[6]):0;c->gpr[2]=0;
    if(e==5){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
    else if(e==3&&hit&&psx_mod_read_byte(hit+8)==2){wr(o+4,rd(o+4)|0x20);invoke(c,0x8004E414,o+0x34,0x08808000);invoke(c,0x80039BC4,hit);counter(0,"vehicle_hits");}
}
void radar(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5];c->gpr[2]=0;
    if(e==4){wr(Active,rd(Active)-1);counter(0,"retired");return;}
    if(e!=0&&e!=3)return;
    if(e==0){
        const auto life=uint16_t(psx_mod_read_half(o+0x94)+1);half(o+0x94,life);half(o+0x1C,std::clamp(int(int16_t(life))*5,33,300));
        move(o,0x88,false);wr(o+0x8C,rd(o+0x8C)+56);
        if(const auto child=rd(o+0x10)){half(child+0x48,signed_half(child+0x48)+34);if(c->gpr[6])invoke(c,0x8002CF74,child);}
        if(si(o+0x38)<=int32_t(invoke(c,0x8001B750,rd(o+0x34),rd(o+0x3C))))return;
    }
    if(const auto burst=model(c,shared(c),0x2B,0x80)){
        wr(burst,RadarHit);wr(burst+4,0x514);psx_mod_write_byte(burst+8,8);half(burst+0xA,signed_half(o+0xA));copy(burst+0x4C,o+0x34,12);
        if(const auto child=rd(burst+0x10)){wr(child,0x8004CA10);wr(child+4,0x410);}invoke(c,0x8003066C,burst);
    }
    fx(c,o+0x34,0x1B);common_sound(c,0x41,o+0x34);finish(c,o,e==0);c->gpr[2]=UINT32_MAX;
}
void bullet(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],hit=c->gpr[6]?rd(c->gpr[6]):0;c->gpr[2]=0;
    if(e!=0&&e!=3)return;
    if(e==0){move(o);half(o+0x94,signed_half(o+0x94)-1);if(signed_half(o+0x94)!=-1&&si(o+0x50)<=int32_t(invoke(c,0x8001B750,rd(o+0x4C),rd(o+0x54))))return;}
    if(e==3&&hit&&psx_mod_read_byte(hit+8)==3){c->gpr[2]=UINT32_MAX;return;}
    if(const auto effect=fx(c,o+0x4C,0x10)){if(e==3)wr(effect+4,rd(effect+4)|0x400);half(effect+0x48,random(c));invoke(c,0x8002CF74,effect);}
    common_sound(c,0x39,o+0x4C);
    if(e==3&&hit&&psx_mod_read_byte(hit+8)==2){
        Scratch s(c);for(unsigned i=0;i<12;i+=4)wr(s.p+i,rd(o+0x88+i)<<4);invoke(c,0x8002B1FC,hit,s.p,o+0x34);
        if(signed_half(hit+0xA)<0)invoke(c,0x80015ADC,uint32_t(~signed_half(hit+0xA)),10);
        if(!(random(c)&7)){const auto index=random(c)*3>>15;if(const auto part=rd(hit+0x11C+index*4)){fx(c,invoke(c,0x8002CDF4,part)+0x14,0x2D);invoke(c,0x8003A280,hit,index);}}
        counter(2,"vehicle_hits");
    }
    finish(c,o,e==0);c->gpr[2]=UINT32_MAX;counter(2,"retired");
}
void fume(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],hit=c->gpr[6]?rd(c->gpr[6]):0;c->gpr[2]=0;
    if(e==0){move(o);for(unsigned i=0;i<12;i+=4)wr(o+0x88+i,uint32_t(mul(si(o+0x88+i),0xF80)>>12));half(o+0x48,signed_half(o+0x48)+signed_half(o+0x96));if(c->gpr[6])invoke(c,0x8002CF74,o);}
    else if(e==3&&!(rd(o+4)&0x10000)&&hit&&psx_mod_read_byte(hit+8)==2){wr(o+4,rd(o+4)|0x10000);common_sound(c,0x20,o+0x4C);if(!(random(c)*5>>15))invoke(c,0x80039BC4,hit);counter(10,"vehicle_hits");}
    else if(e==5){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;counter(10,"retired");}
}
void ignite(CPUState* c,uint32_t car){invoke(c,0x80039AF8,car);}
void fireball(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],hit=c->gpr[6]?rd(c->gpr[6]):0;c->gpr[2]=0;
    if(e==0){move(o);wr(o+0x8C,rd(o+0x8C)+56);if(si(o+0x50)<=int32_t(invoke(c,0x8001B750,rd(o+0x4C),rd(o+0x54))))return;
        if(const auto b=blast(c,o+0x4C,0x2E,40))invoke(c,0x8002D114,b,b+0x4C,b+0x20);
    }else if(e==3){blast(c,o+0x4C,0x2E,0);fx(c,o+0x4C,0xC);common_sound(c,0x39,o+0x4C);
        if(hit&&psx_mod_read_byte(hit+8)==2){invoke(c,0x8004E414,hit+0x34,0x080000FF);ignite(c,hit);counter(11,"vehicle_hits");}
    }else return;
    finish(c,o,e==0);c->gpr[2]=UINT32_MAX;counter(11,"retired");
}
uint32_t fire_radar(CPUState* c,uint32_t o,uint32_t owner){
    Scratch s(c);const auto socket=invoke(c,0x8002C5F4,o,0x8000);if(!socket)return 0;
    invoke(c,0x8002C77C,s.p,socket);invoke(c,0x80059AFC,invoke(c,0x8002CDF4,o),s.p,s.p+32);
    unsigned spawned=0;
    for(int x=-2048;x<=2048;x+=2048)for(int y=0;y>=-4096;y-=2048){
        const auto shot=model(c,rd(o+0x5C),0x35);if(!shot)continue;
        half(s.p+64,x);half(s.p+66,y);half(s.p+68,8192);half(s.p+70,0);invoke(c,0x80059FBC,s.p+32,s.p+64,s.p+72);
        worldshot(shot,owner,Radar,0x60000084,0);copy(shot+0x34,s.p+32+0x14,12);
        if(const auto child=rd(shot+0x10))wr(child+4,0x410);
        for(unsigned i=0;i<3;++i)wr(shot+0x88+i*4,uint32_t(add(si(owner+0x80+i*4)/128,signed_half(s.p+72+i*2))));
        invoke(c,0x800305FC,shot);wr(Active,rd(Active)+1);++spawned;
    }
    if(!spawned)return 0;
    if(const auto effect=model(c,shared(c),2,0x80)){wr(effect+4,0x24);copy(effect+0x20,s.p+32,32);wr(effect,0x8004CA68);invoke(c,0x800305FC,effect);}
    common_sound(c,0x2F,s.p+32+0x14);phrase(c,o,0);
    for(unsigned i=0;i<3;++i)wr(s.p+64+i*4,uint32_t(-signed_half(s.p+4+i*6)*8));invoke(c,0x8002B1FC,owner,s.p+64,s.p+0x14);
    consume(c,o,0);return signed_half(owner+0xA)<0?90:180;
}
uint32_t fire_bullet(CPUState* c,uint32_t o,uint32_t owner){
    const auto pivot=rd(o+0x10);if(!pivot)return 0;
    const auto effect=model(c,shared(c),0xD,0x80);if(!effect)return 0;
    const auto shot=invoke(c,0x80042408,owner,pivot,0x37,0x98,effect);
    if(!shot){invoke(c,0x800309A0,effect);return 0;}
    worldshot(shot,owner,Bullet,signed_half(o+0x44)?0x280:0x60000280,20);half(shot+0x94,180);velocity(shot,owner,6);invoke(c,0x800305FC,shot);
    wr(effect,0x8004CA68);if(!(rd(owner+4)&4))invoke(c,0x80030BF0,effect);
    if(const auto drop=model(c,rd(o+0x5C),0x36,0x94,0)){
        const auto socket=invoke(c,0x8002C5F4,pivot,0x8001);
        if(socket)invoke(c,0x8002CEAC,drop+0x20,pivot,socket);else copy(drop+0x20,invoke(c,0x8002CDF4,pivot),32);
        psx_mod_write_byte(drop+8,7);wr(drop+4,0xA0);wr(drop,0x8004CD60);
        for(unsigned i=0;i<3;++i)wr(drop+0x88+i*4,uint32_t(signed_half(drop+0x24+i*6)>>2));psx_mod_write_byte(drop+0x87,2);invoke(c,0x800305FC,drop);
    }
    recoil(c,owner,pivot,o+0x4C,0x8000);
    if(!(rd(o+4)&0x20000)){wr(o+4,rd(o+4)|0x20000);phrase(c,o,1);}play(c,rd(o+0x5C),0,shot+0x4C);
    const auto flags=rd(o+4);const auto count=signed_half(o+0x44);uint32_t delay=180;
    if(signed_half(owner+0xA)<0){wr(o+4,flags|0x40000);delay=12;}
    else if(count<8)half(o+0x44,count+1);else wr(o+4,flags&~0x40000u);
    consume(c,o,2);return delay;
}
uint32_t fire_fume(CPUState* c,uint32_t o,uint32_t owner){
    const auto shot=model(c,shared(c),0xF);if(!shot)return 0;
    worldshot(shot,owner,Fume,0x60000594,(random(c)&1)+1);
    const auto socket=invoke(c,0x8002C5F4,o,0x8000);
    if(socket)invoke(c,0x8002CEAC,shot+0x20,o,socket);else copy(shot+0x20,invoke(c,0x8002CDF4,o),32);
    copy(shot+0x4C,shot+0x34,12);velocity(shot,owner,-1,2);half(shot+0x96,int(random(c)*68>>15)-34);invoke(c,0x800305FC,shot);
    const auto count=signed_half(o+0x44)+1;half(o+0x44,count);if(count==1)phrase(c,o,0);
    const bool rapid=signed_half(owner+0xA)<0||count<20;wr(o+4,(rd(o+4)&~0x40000u)|(rapid?0x40000:0));consume(c,o,10);return rapid?10:180;
}
uint32_t fire_flame(CPUState* c,uint32_t o,uint32_t owner){
    const auto pivot=rd(o+0x10);if(!pivot)return 0;
    const auto socket=invoke(c,0x8002C5F4,pivot,0x8000|(psx_mod_read_half(o+0x1C)&1));if(!socket)return 0;
    const auto shot=model(c,shared(c),0x18);if(!shot)return 0;
    invoke(c,0x8002CEAC,shot+0x20,pivot,socket);worldshot(shot,owner,Fireball,0x60000094,25);copy(shot+0x4C,shot+0x34,12);half(shot+0x94,60);velocity(shot,owner,3);invoke(c,0x800305FC,shot);
    if(const auto effect=model(c,shared(c),4,0x80)){invoke(c,0x8002CA94,pivot,socket,effect);wr(effect,0x8004CA68);if(!(rd(owner+4)&4))invoke(c,0x80030BF0,effect);}
    recoil(c,owner,pivot,o+0x4C,0x18000);common_sound(c,0x30,shot+0x4C);
    if(!(rd(o+4)&0x20000)){wr(o+4,rd(o+4)|0x20000);phrase(c,o,0);}
    const bool rapid=signed_half(owner+0xA)<0||signed_half(o+0x44)<6;
    if(rapid)half(o+0x44,signed_half(o+0x44)+1);wr(o+4,(rd(o+4)&~0x40000u)|(rapid?0x40000:0));consume(c,o,11);return rapid?40:180;
}
uint32_t fire_quake(CPUState*,uint32_t,uint32_t);
uint32_t fire_vortex(CPUState*,uint32_t,uint32_t);
uint32_t fire_horn(CPUState*,uint32_t,uint32_t);
uint32_t fire_missile(CPUState*,uint32_t,uint32_t);
uint32_t fire_disco(CPUState*,uint32_t,uint32_t);
uint32_t fire_bees(CPUState*,uint32_t,uint32_t);
void aim(CPUState* c,uint32_t o,uint32_t owner,unsigned kind){
    const auto target=rd(owner+0xF0),pivot=rd(o+0x10);if(!target||(kind!=0&&!pivot))return;
    if(kind==2){original_specials::aim(c,o,owner,false);return;}
    Scratch s(c);const auto matrix=invoke(c,0x8002CDF4,o);
    for(unsigned i=0;i<12;i+=4)wr(s.p+i,rd(target+0x4C+i)-rd(matrix+0x14+i));invoke(c,0x8002426C,matrix,s.p,s.p);
    if(kind==0){
        const int yaw=original_specials::shift(psx_mod_read_half(o+0x46)+invoke(c,0x8005B9AC,rd(s.p),rd(s.p+8)),20,20);
        const int pitch=original_specials::shift(psx_mod_read_half(o+0x44)-invoke(c,0x8005B9AC,rd(s.p+4),rd(s.p+8)),20,20);
        half(o+0x46,std::clamp(yaw,-512,512));half(o+0x44,std::clamp(pitch,-227,227));invoke(c,0x8002CF74,o);
    }else if(kind==11){
        const int yaw=std::clamp(int(int16_t(invoke(c,0x8005B9AC,rd(s.p),rd(s.p+8)))),-1024,1024);
        // The source scales the horizontal distance before squaring, avoiding
        // overflow in its ballistic elevation calculation.
        const uint64_t square=uint64_t(int64_t(si(s.p))*si(s.p))+uint64_t(int64_t(si(s.p+8))*si(s.p+8));
        const int planar=int(invoke(c,0x8002ABC4,uint32_t(square),uint32_t(square>>32)));
        const int y=planar?int(int64_t(si(s.p+4))*0x3000/planar):0;
        const int pitch=std::clamp(int(int16_t(0u-invoke(c,0x8005B9AC,uint32_t(y-planar*7/24576),0x3000))),-113,341);
        half(pivot+0x46,signed_half(pivot+0x46)+(yaw-signed_half(pivot+0x46))/4);
        half(pivot+0x44,signed_half(pivot+0x44)+(pitch-signed_half(pivot+0x44))/4);invoke(c,0x8002CF74,pivot);
    }
}
bool ai(CPUState* c,uint32_t o,uint32_t owner,unsigned kind){
    const auto target=rd(owner+0xF0);if(!target||rd(Active))return false;
    if(kind==2&&signed_half(o+0x44))return signed_half(o+0x44)<8;
    const int distance=int(invoke(c,0x80029F6C,owner+0x4C,target+0x4C));
    switch(kind){
    case 0:return distance>0x64000&&distance<=0x1F3FFF;
    case 2:return distance<=0x31FFF&&si(owner+0x8C)<0xBEB;
    case 3:return grounded(owner)&&si(owner+0x8C)<0x5F5&&distance<=0xC7FFF;
    case 4:return !(rd(o+4)&1)&&distance>0x64000;
    case 5:return distance>0xFA000&&distance<=0x3E7FFF;
    case 6:return distance>0x96000&&distance<=0x3E7FFF;
    case 8:return distance>0x32000&&distance<=0x12BFFF;
    case 9:return distance>0x96000&&distance<=0x1F3FFF;
    case 10:case 11:{
        if(signed_half(o+0x44)||(kind==11&&si(owner+0x8C)>=0xBEC))return false;
        Scratch s(c);invoke(c,0x80024304,owner+0x20,target+0x34,s.p);
        const int yaw=std::abs(int(int16_t(invoke(c,0x8005B9AC,rd(s.p),rd(s.p+8))))),z=si(s.p+8);
        return kind==10?(z<0&&z>-0x4B000&&yaw>=0x556):(z>0x96000&&z<0x12BFFF&&yaw<0x2AA);
    }default:return false;
    }
}
void weapon(CPUState* c,uint32_t address){
    const unsigned kind=((address&0x1FFFFFFF)-(Weapon&0x1FFFFFFF))/4;
    const auto o=c->gpr[4],event=c->gpr[5],owner=c->gpr[6];uint32_t result=0;
    if(kind>=13)fail("v82.special.invalid_kind");
    if(event==1){
        constexpr int Ammo[]={3,3,50,3,3,3,3,12,3,3,40,16,12};half(o+0x1C,Ammo[kind]);psx_mod_write_byte(o+8,3);psx_mod_write_byte(o+0x18,6);
        if(kind!=3&&kind!=4)wr(o+4,rd(o+4)|0x4000);
        if((kind==8||kind==9)&&rd(o+0x10))wr(rd(o+0x10),0x8004CA10);
    }else if(event==0){
        if(owner&&invoke(c,0x80042330,o,owner)){
            if(kind==0||kind==2||kind==11)aim(c,o,owner,kind);
            if(kind==4&&rd(o+0x10)){const auto p=rd(o+0x10);half(p+0x46,signed_half(p+0x46)+34);invoke(c,0x8002CF74,p);}
        }
        if(kind==6&&(rd(o+4)&0x80)){
            const auto p=rd(o+0x10);const auto phase=psx_mod_read_byte(o+0x19);
            if(p){if(phase<30)half(p+0x44,signed_half(p+0x44)-17);else if(phase>=61)half(p+0x44,signed_half(p+0x44)+17);invoke(c,0x8002CF74,p);}
            psx_mod_write_byte(o+0x19,uint8_t(phase+1));if(phase==90)invoke(c,0x80030BA8,o);
        }
    }else if(event==11){
        wr(o+4,rd(o+4)&~0x20000u);if(kind==2||kind==10||kind==11)wr(o+0x44,0);
    }else if(event==12&&owner&&psx_mod_read_half(o+0x1C)){
        using Fire=uint32_t(*)(CPUState*,uint32_t,uint32_t);
        constexpr Fire Fires[]={fire_radar,nullptr,fire_bullet,fire_quake,fire_vortex,fire_horn,fire_missile,nullptr,fire_disco,fire_bees,fire_fume,fire_flame,nullptr};
        if(Fires[kind])result=Fires[kind](c,o,owner);
    }else if(event==13&&owner)result=ai(c,o,owner,kind);
    else if(event==14)result=6;
    else if(event==15)result=0x801F;
    else if(event==16&&owner&&(kind==5||kind==6)){
        // The pickup's fresh visual rack supplies the spent visible rounds.
        const auto rack=rd(owner+0x10);
        if(kind==5&&rack){
            const auto round=invoke(c,0x8002CCBC,rack);
            half(round+0xA,std::max(0,2-int(psx_mod_read_half(o+0x1C))));
            invoke(c,0x8002CC9C,o,round);
        }else if(kind==6&&rack){
            const auto pivot=rd(o+0x10),child=pivot?rd(pivot+0x10):0;
            if(child){const int want=signed_half(child+0xA)-1;auto round=rd(rack+0x10);
                while(round&&signed_half(round+0xA)!=want)round=rd(round+0xC);
                if(round)invoke(c,0x8002CC9C,pivot,invoke(c,0x8002CCBC,round));
            }
        }
    }
    else if(event==5&&(kind==8||kind==9)){
        if(const auto p=rd(o+0x10))wr(p+0x64,0);invoke(c,0x80030C20,o);result=kind==8?UINT32_MAX:0;
    }
    c->gpr[2]=result;
}
uint32_t terrain_cell(int x,int z){
    // Edge cells outside the map have no storage. Never wrap negative grid
    // coordinates through the chunk pointer table (Dream Land has water edges).
    if(x<0||z<0||x>=2048||z>=2048)return 0;
    const auto chunk=rd(0x800B9470+((unsigned(x)>>6)*32+(unsigned(z)>>6))*4);
    return chunk?chunk+((unsigned(x)&63)<<7)+((unsigned(z)&63)<<1):0;
}
void quake_restore(uint32_t o){
    const int x=si(o+0x4C)/65536-16,z=si(o+0x54)/65536-16;
    for(int dx=0;dx<32;++dx)for(int dz=0;dz<32;++dz){
        const auto p=o+0x888+dx*2+dz*64;const int value=signed_half(p);
        if(value)if(const auto cell=terrain_cell(x+dx,z+dz))half(cell,psx_mod_read_half(cell)-value);
        half(p,0);
    }
}
void quake(CPUState* c,uint32_t){
    const auto o=c->gpr[4],event=c->gpr[5],owner=rd(o+0x80);c->gpr[2]=0;
    if(event==1){
        for(int x=-16;x<16;++x)for(int z=-16;z<16;++z)psx_mod_write_byte(o+0x1088+(x+16)*32+z+16,uint8_t(invoke(c,0x800597BC,(x*x+z*z)*64)));
        return;
    }
    if(event==4){quake_restore(o);wr(Active,rd(Active)-1);counter(3,"retired");return;}
    if(event!=0)return;
    const int tick=si(o+0x84);
    if(owner&&tick<60){
        half(owner+0xAA,-signed_half(owner+0xB0)*2);
        const auto a=rd(owner+0x108),b=rd(owner+0x10C);
        if(a&&b){const auto displacement=rd(a+0x84)+((tick/15)&1?uint32_t(-0x300):0x300);wr(a+0x84,displacement);wr(b+0x84,displacement);}
    }
    if(tick==30||tick==60)play(c,rd(o+0x5C),2,o+0x4C);
    if(tick==0){
        if(const auto pulse=model(c,rd(o+0x5C),0x33)){
            worldshot(pulse,owner,Blast,0x60000004,15);copy(pulse+0x4C,o+0x4C,12);
            if(rd(pulse+0x60))wr(pulse+0x58,rd(rd(pulse+0x60)+0x10));children(pulse,0x8004CA10);invoke(c,0x8003066C,pulse);play(c,rd(o+0x5C),1,o+0x4C);
        }
    }
    if(c->gpr[6]&&tick>=60){
        const int baseX=si(o+0x4C)/65536-16,baseZ=si(o+0x54)/65536-16,center=(tick-60)/2+48;
        for(int x=0;x<32;++x)for(int z=0;z<32;++z){
            const int distance=psx_mod_read_byte(o+0x1088+x+z*32),ring=(128-distance)*(128-distance),phase=(distance-center)*128;
            int profile=0;if(std::abs(phase)<4096)profile=(mul(trig(phase),ring)/1048576)|((-trig(phase,true)/512)*2048);
            const auto p=o+0x888+x*2+z*64;const int16_t delta=int16_t(profile-signed_half(p));
            if(delta)if(const auto cell=terrain_cell(baseX+x,baseZ+z))half(cell,psx_mod_read_half(cell)+delta);
            half(p,profile);
        }
        counter(3,"terrain_ticks");
    }
    wr(o+0x84,tick+1);
    if(tick==316){quake_restore(o);invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
}
uint32_t fire_quake(CPUState* c,uint32_t o,uint32_t owner){
    if(!grounded(owner)){common_sound(c,0x15,0);return 180;}
    const auto shot=invoke(c,0x8002CBE8,0x1488);if(!shot)return 0;
    wr(shot,Quake);wr(shot+4,0xA0);wr(shot+0x80,owner);half(shot+0xA,signed_half(owner+0xA));wr(shot+0x5C,rd(o+0x5C));copy(shot+0x4C,owner+0x4C,12);
    wr(Active,rd(Active)+1);invoke(c,0x8003066C,shot);phrase(c,o,0);consume(c,o,3);return 180;
}
void trail(CPUState* c,uint32_t){
    const auto o=c->gpr[4];c->gpr[2]=0;
    if(c->gpr[5]==0){const int timer=signed_half(o+0x96);if(timer){half(o+0x96,timer-1);if(timer==1){wr(o+4,rd(o+4)&~2u);invoke(c,0x80030BF0,o);}}else move(o,0x88,false);}
    else if(c->gpr[5]==5){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
}
void bounce(CPUState* c,uint32_t o,uint32_t collision,unsigned velocityOffset,bool tornado){
    const auto hit=collision?rd(collision):0;if(!hit||psx_mod_read_byte(hit+8)==3)return;
    invoke(c,0x8002F798,o,collision);
    int32_t product=0;for(unsigned i=0;i<3;++i)product=add(product,mul(si(o+velocityOffset+i*4),signed_half(collision+0x20+i*2)));
    const int32_t dot=product/2048;if(dot>=0)return;
    if(tornado&&psx_mod_read_byte(hit+8)==2){
        Scratch s(c);for(unsigned i=0;i<12;i+=4)wr(s.p+i,rd(o+velocityOffset+i)<<4);invoke(c,0x8002B370,hit,s.p,o+0x4C);
        if(!(rd(o+4)&0x8000)){common_sound(c,5,o+0x4C);fx(c,o+0x4C,0x2D);invoke(c,0x8004E414,o+0x4C,0x08404040);if(signed_half(hit+0xA)<0)invoke(c,0x80015B00,uint32_t(~signed_half(hit+0xA)),192,0,64);}
        wr(o+4,rd(o+4)|0x18000);counter(4,"vehicle_hits");
    }
    for(unsigned i=0;i<3;++i)wr(o+velocityOffset+i*4,uint32_t(sub(si(o+velocityOffset+i*4),mul(dot,signed_half(collision+0x20+i*2))/4096)));
}
void vortex(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],other=c->gpr[6];c->gpr[2]=0;
    if(e==1){const auto v=voice(c);psx_mod_write_byte(o+9,uint8_t(v));invoke(c,0x8001E098,v,sound(o),0,0);}
    else if(e==4){invoke(c,0x8001DE78,uint32_t(int8_t(psx_mod_read_byte(o+9))));wr(Active,rd(Active)-1);counter(4,"retired");}
    else if(e==9){if(rd(o+0x84)==other)wr(o+0x84,rd(o+0x80));}
    else if(e==3)bounce(c,o,other,0x88,true);
    else if(e==0){
        const int timer=signed_half(o+0x96)-1;half(o+0x96,timer);if(timer<-120){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;return;}
        Scratch s(c);for(unsigned i=0;i<8;i+=4)wr(s.p+i,0);
        if(timer<0)half(s.p+2,-4096);else if(rd(o+0x84))invoke(c,0x8002A168,s.p,o+0x4C,rd(o+0x84)+0x4C);
        for(unsigned i=0;i<3;++i){
            int v=add(sub(si(o+0x88+i*4),si(o+0x88+i*4)/128),signed_half(s.p+i*2)/16);
            if(i==1)v=add(v,std::min(0,sub(int32_t(invoke(c,0x8002CFBC,o,o+0x4C)),add(si(o+0x50),0x2800))/16));
            wr(o+0x88+i*4,uint32_t(std::clamp(v,i==1?-0xBEB:-0x3B9A,0x4786)));
        }
        move(o);half(o+0x46,signed_half(o+0x46)+136);
        if(other){invoke(c,0x8002CF74,o);invoke(c,0x8001E2C8,uint32_t(int8_t(psx_mod_read_byte(o+9))),invoke(c,0x8001E7A8,o+0x34));const auto f=rd(o+4);wr(o+4,(f&~0x18000u)|((f&0x10000)?0x8000:0));}
    }
}
uint32_t fire_vortex(CPUState* c,uint32_t o,uint32_t owner){
    const auto pivot=rd(o+0x10);if(!pivot)return 0;const auto socket=invoke(c,0x8002C5F4,pivot,0x8000);if(!socket)return 0;
    Scratch s(c);invoke(c,0x8002CEAC,s.p,pivot,socket);const auto target=rd(owner+0xF0)?rd(owner+0xF0):owner;
    unsigned spawned=0;
    for(unsigned i=0;i<3;++i)if(const auto shot=model(c,rd(o+0x5C),0x3C)){
        worldshot(shot,owner,Vortex,0x60000080,signed_half(target+0xA)<0?7:4);copy(shot+0x4C,target+0x4C,12);wr(shot+0x50,rd(shot+0x50)-0xC0000);
        wr(shot+0x88,uint32_t(trig(i*4096/3,true)));wr(shot+0x8C,0);wr(shot+0x90,uint32_t(trig(i*4096/3)));half(shot+0x96,360+i*60);wr(shot+0x84,target);
        wr(Active,rd(Active)+1);invoke(c,0x8003066C,shot);++spawned;
    }
    if(!spawned)return 0;
    for(unsigned i=0;i<4;++i)if(const auto t=model(c,rd(o+0x5C),0x3D)){
        psx_mod_write_byte(t+8,7);half(t+0xA,i);wr(t+4,0xA2);copy(t+0x20,s.p,32);wr(t,Trail);children(t,0x8004CA10);half(t+0x96,1+i*8);
        velocity(t,owner,1);invoke(c,0x8002D1DC,t);invoke(c,0x800305FC,t);
    }
    phrase(c,o,2);play(c,rd(o+0x5C),1,s.p+0x14);consume(c,o,4);return 720;
}
void horn(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],other=c->gpr[6],target=rd(o+0x84);c->gpr[2]=0;
    if(e==9){if(target==other)wr(o+0x84,rd(o+0x80));return;}
    if(e==4){invoke(c,0x80051CC0,o+0x98);counter(5,"retired");return;}
    bool explode=false;uint32_t hit=0;
    if(e==0){
        Scratch s(c);wr(o+0x90,rd(o+0x90)+56);move(o,0x8C,false);
        const int ground=int32_t(invoke(c,0x8002CFBC,o,o+0x34,s.p,0));
        if(si(o+0x38)>sub(ground,si(o+0x58))){
            if(!(rd(o+4)&1)||!target||(std::abs(int64_t(si(target+0x4C))-si(o+0x34))<=0x27FFF&&std::abs(int64_t(si(target+0x54))-si(o+0x3C))<=0x27FFF))explode=true;
            else{
                int x=si(target+0x4C),z=si(target+0x54);
                if(const auto path=invoke(c,0x80036060,o+0x34,target+0x4C,0x113A0,0)){x=signed_half(path)*65536;z=signed_half(path+2)*65536;invoke(c,0x800520D8,path);}
                half(o+0x46,int(invoke(c,0x8005B9AC,uint32_t(sub(x,si(o+0x34))),uint32_t(sub(z,si(o+0x3C))))));wr(o+0x38,uint32_t(sub(ground,si(o+0x58))));
                wr(o+0x90,uint32_t(add(mul(signed_half(s.p),si(o+0x8C)),mul(signed_half(s.p+4),si(o+0x94)))/4096-512));
                copy(s.p+16,o+0x34,12);wr(s.p+20,uint32_t(ground));fx(c,s.p+16,0x1F);play(c,rd(o+0x5C),0,s.p+16);
            }
        }
        if(!explode){half(o+0x44,signed_half(o+0x44)+136);half(o+0x48,signed_half(o+0x48)+std::clamp(1024-signed_half(o+0x48),-34,34));invoke(c,0x8002CF44,o);
            wr(o+0x8C,uint32_t(trig(signed_half(o+0x46))*2));wr(o+0x94,uint32_t(trig(signed_half(o+0x46),true)*2));return;}
    }else if(e==3){hit=other?rd(other):0;if(!hit)return;explode=psx_mod_read_byte(hit+8)==2;if(!explode){bounce(c,o,other,0x8C,false);common_sound(c,0,o+0x34);return;}}
    else return;
    if(const auto effect=blast(c,o+0x34,0x27,signed_half((hit?hit:(target?target:rd(o+0x80)))+0xA)<0?250:100)){
        invoke(c,0x8002D114,effect,effect+0x4C,effect+0x20);wr(effect+4,rd(effect+4)|0x10);
    }
    fx(c,o+0x34,0);invoke(c,0x8004E414,o+0x34,0x080000FF);common_sound(c,0x39,o+0x34);invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;if(hit)counter(5,"vehicle_hits");
}
uint32_t fire_horn(CPUState* c,uint32_t o,uint32_t owner){
    auto source=rd(o+0x10);const int want=3-psx_mod_read_half(o+0x1C);while(source&&signed_half(source+0xA)!=want)source=rd(source+0xC);if(!source)return 0;
    const auto shot=model(c,rd(source+0x5C),psx_mod_read_half(source+0x1A),0xA8,0);if(!shot)return 0;
    worldshot(shot,owner,Horn,0x60000080,0);copy(shot+0x20,invoke(c,0x8002CDF4,source),32);invoke(c,0x8002A2E0,shot+0x20,shot+0x44);copy(shot+0x4C,shot+0x34,12);
    // Original launch combines the horn's first two basis columns.
    for(unsigned i=0;i<3;++i)wr(shot+0x8C+i*4,uint32_t(add(sub(si(owner+0x80+i*4)/128,signed_half(shot+0x22+i*6)*1750/4096),signed_half(shot+0x20+i*6)*800/4096)));
    const auto target=rd(owner+0xF0)?rd(owner+0xF0):owner;wr(shot+0x84,target);wr(shot+0x88,uint32_t(signed_half(target+0xA)));invoke(c,0x8003066C,shot);invoke(c,0x80030CB0,shot,1800);
    Scratch s(c);wr(s.p,0);wr(s.p+4,0x10000);wr(s.p+8,0);invoke(c,0x8002B1FC,owner,s.p,source+0x4C);
    common_sound(c,0x14,shot+0x4C);phrase(c,o,1);invoke(c,0x8002C4B4,invoke(c,0x8002CCBC,source));consume(c,o,5);return signed_half(owner+0xA)<0?240:180;
}
void missile_trail(CPUState* c,uint32_t){
    const auto o=c->gpr[4];c->gpr[2]=0;if(c->gpr[5]==0){move(o);for(unsigned i=0;i<12;i+=4)wr(o+0x88+i,uint32_t(mul(si(o+0x88+i),0xF80)>>12));half(o+0x48,signed_half(o+0x48)+signed_half(o+0x96));invoke(c,0x8002CF74,o);}
    else if(c->gpr[5]==5){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
}
void bomblet_hit(CPUState* c,uint32_t){
    const auto o=c->gpr[4],hit=c->gpr[6]?rd(c->gpr[6]):0;c->gpr[2]=0;
    if(c->gpr[5]==5){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
    else if(c->gpr[5]==3&&hit&&psx_mod_read_byte(hit+8)==2){
        Scratch s(c);for(unsigned i=0;i<3;++i)wr(s.p+i*4,(rd(hit+0x34+i*4)-rd(o+0x34+i*4))*(i==1?1u:2u));invoke(c,0x8002B370,hit,s.p,o+0x34);
        for(unsigned i=0;i<12;i+=4)wr(s.p+i,0);invoke(c,0x8003A020,hit,0u-psx_mod_read_half(o+0x1C),s.p,1);
        invoke(c,0x8004E414,o+0x34,0x08404040);wr(o+4,rd(o+4)|0x20);counter(6,"vehicle_hits");
    }
}
void bomblet(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],collision=c->gpr[6],hit=collision?rd(collision):0;c->gpr[2]=0;
    if(e==0){move(o,0x88,false);wr(o+0x8C,rd(o+0x8C)+90);if(si(o+0x38)<=int32_t(invoke(c,0x8001B750,rd(o+0x34),rd(o+0x3C))))return;
        if(const auto b=blast(c,o+0x34,0,100)){wr(b,BombletHit);wr(b+4,rd(b+4)|0x110);}common_sound(c,0x38,o+0x34);
    }else if(e==3){
        if(hit&&psx_mod_read_byte(hit+8)==3)return;
        if(hit&&psx_mod_read_byte(hit+8)==2){
            Scratch s(c);const int mass=psx_mod_read_half(hit+0xA6);const int scale=mass?(psx_mod_read_half(o+0x1C)*2048)/mass:0;
            for(unsigned i=0;i<12;i+=4)wr(s.p+i,uint32_t(std::clamp(mul(si(o+0x88+i),scale),-0x80000,0x80000)));invoke(c,0x8002B370,hit,s.p,o+0x34);invoke(c,0x8004E414,o+0x34,0x08404040);counter(6,"vehicle_hits");
        }
        fx(c,o+0x34,0x1A);common_sound(c,0x38,o+0x34);
    }else return;
    finish(c,o,e==0);c->gpr[2]=UINT32_MAX;counter(6,"bomblets_retired");
}
void missile(CPUState* c,uint32_t){
    const auto o=c->gpr[4],event=c->gpr[5];c->gpr[2]=0;
    if(event==4){wr(Active,rd(Active)-1);counter(6,"retired");return;}
    if(event==9){if(rd(o+0x84)==c->gpr[6])wr(o+0x84,rd(o+0x80));return;}
    if(event!=0)return;
    const int delay=signed_half(o+0x96);
    if(delay){
        half(o+0x96,delay-1);if(delay!=1)return;
        const auto launcher=invoke(c,0x8002CD78,invoke(c,0x8002CD78,o)),owner=rd(o+0x80);
        copy(o+0x20,invoke(c,0x8002CDF4,o),32);copy(o+0x4C,o+0x34,12);half(o+0xA,signed_half(owner+0xA));
        for(unsigned i=0;i<12;i+=4)wr(o+0x88+i,uint32_t(si(owner+0x80+i)/128));wr(o+0x84,rd(owner+0xF0)?rd(owner+0xF0):owner);
        invoke(c,0x8002CCBC,o);
        if(const auto exhaust=model(c,rd(o+0x5C),0x3A,0x80)){for(unsigned i=0;i<32;i+=4)wr(exhaust+0x20+i,0);half(exhaust+0x20,4096);half(exhaust+0x28,4096);half(exhaust+0x30,4096);invoke(c,0x8002CC9C,o,exhaust);}
        // The launcher already registered this object's update callback.
        // Add only rendering and world ownership, as original 8003970C does;
        // RegisterWorld here duplicates its tick entry and leaves a stale one
        // after retirement (repeated clusters from an already freed missile).
        invoke(c,0x80030BF0,o);invoke(c,0x80030080,c->gpr[28]+0x1040,o);
        if(launcher&&!psx_mod_read_half(launcher+0x1C))invoke(c,0x8003A368,launcher);counter(6,"launched");return;
    }
    move(o);const int tick=signed_half(o+0x94)+1;half(o+0x94,tick);
    if(!(tick&3))if(const auto smoke=model(c,shared(c),0x13)){
        psx_mod_write_byte(smoke+8,7);wr(smoke+4,0x4B4);wr(smoke,MissileTrail);copy(smoke+0x4C,o+0x4C,12);half(smoke+0x48,tick*96);
        for(unsigned i=0;i<3;++i)wr(smoke+0x88+i*4,uint32_t(-signed_half(o+0x24+i*6)));invoke(c,0x8003066C,smoke);
    }
    if(tick>=9)wr(o+0x8C,uint32_t(std::min(0,add(si(o+0x8C),56))));
    if(tick<30){for(unsigned i=0;i<3;++i)wr(o+0x88+i*4,uint32_t(add(si(o+0x88+i*4),signed_half(o+0x24+i*6)*224/4096)));return;}
    Scratch s(c);const auto target=rd(o+0x84)?rd(o+0x84):rd(o+0x80);
    wr(s.p,rd(target+0x4C)+uint32_t(signed_half(target+0x24)*32)-rd(o+0x34));wr(s.p+4,rd(target+0x50)-rd(o+0x38)-0x32000);wr(s.p+8,rd(target+0x54)+uint32_t(signed_half(target+0x30)*32)-rd(o+0x3C));
    invoke(c,0x8002426C,o+0x20,s.p,s.p);flight::rotate(c,o+0x20,si(s.p+4)<0?256:-256,si(s.p)>0?256:-256,0);
    // Renormalize at the source's 32-tick cadence; keep the phase per object.
    if(!(tick&31))invoke(c,0x80059A0C,o+0x20,o+0x20);
    for(unsigned i=0;i<3;++i){const int delta=sub(signed_half(o+0x24+i*6)*11444/4096,si(o+0x88+i*4))/16;wr(o+0x88+i*4,uint32_t(add(si(o+0x88+i*4),std::clamp(delta,-256,256))));}
    if(tick<=600&&(tick<=60||std::max(std::abs(int64_t(si(s.p))),std::abs(int64_t(si(s.p+8))))>65535))return;
    for(unsigned i=0;i<5;++i)if(const auto b=model(c,rd(o+0x5C),0x39,0x98,0)){
        worldshot(b,rd(o+0x80),Bomblet,0x60000080,psx_mod_read_half(o+0x1C));copy(b+0x4C,o+0x4C,12);
        wr(b+0x88,random(c)*0x989/32768-0x4C4);wr(b+0x8C,random(c)*0x17D7/32768+0xBEB);wr(b+0x90,random(c)*0x989/32768-0x4C4);invoke(c,0x8003066C,b);counter(6,"bomblets");
    }
    fx(c,o+0x4C,0xC);invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;
}
uint32_t fire_missile(CPUState* c,uint32_t o,uint32_t owner){
    if(rd(o+4)&0x80){common_sound(c,0x15,0);return signed_half(owner+0xA)<0?300:360;}
    const auto pivot=rd(o+0x10),source=pivot?rd(pivot+0x10):0;if(!source)return 0;
    const auto shot=model(c,rd(source+0x5C),psx_mod_read_half(source+0x1A),0x98,0);if(!shot)return 0;
    copy(shot+0x20,source+0x20,32);psx_mod_write_byte(shot+8,8);half(shot+0xA,3);half(shot+0x1C,100);wr(shot,Missile);half(shot+0x96,30);wr(shot+0x80,owner);wr(shot+0x84,owner);
    invoke(c,0x80030B78,shot);invoke(c,0x8002C4B4,invoke(c,0x8002CCBC,source));invoke(c,0x8002CC48,pivot,shot);
    common_sound(c,0x2E,owner+0x34);phrase(c,o,0);wr(Active,rd(Active)+1);consume(c,o,6,false);invoke(c,0x80030B78,o);psx_mod_write_byte(o+0x19,0);return signed_half(owner+0xA)<0?300:360;
}
void stop_anim(CPUState* c,uint32_t){c->gpr[2]=0;if(c->gpr[5]==5){wr(c->gpr[4]+0x64,0);c->gpr[2]=UINT32_MAX;}}
void star(CPUState* c,uint32_t){
    const auto o=c->gpr[4],e=c->gpr[5],hit=c->gpr[6]?rd(c->gpr[6]):0;c->gpr[2]=0;
    if(e==5){
        Scratch s(c);copy(s.p,invoke(c,0x8002CDF4,o),32);wr(o+0x64,0);wr(o+4,rd(o+4)|0x80);copy(o+0x34,s.p+0x14,12);
        if(const auto target=rd(o+0x84)){
            invoke(c,0x8002A168,s.p+32,o+0x34,target+0x4C);
            for(unsigned i=0;i<3;++i)wr(o+0x88+i*4,uint32_t(mul(signed_half(s.p+32+i*2),0x5968)>>12));
        }else{
            invoke(c,0x80024094,s.p,o+0x4C,o+0x88);for(unsigned i=0;i<12;i+=4)wr(o+0x88+i,uint32_t(int32_t(rd(o+0x88+i)<<9)>>12));
        }
        invoke(c,0x8002CCBC,o);invoke(c,0x800305FC,o);play(c,rd(o+0x5C),0,s.p+0x14);
        constexpr uint32_t Colors[]={0x08000080,0x08008000,0x08800000};invoke(c,0x8004E414,s.p+0x14,Colors[psx_mod_read_byte(o+0x18)%3]);counter(8,"stars_launched");c->gpr[2]=UINT32_MAX;return;
    }
    if(e==0){move(o,0x88,false);if(si(o+0x38)<=int32_t(invoke(c,0x8001B750,rd(o+0x34),rd(o+0x3C))))return;}
    else if(e==3){if(hit&&psx_mod_read_byte(hit+8)==2){wr(hit+0x84,uint32_t(-0x2FA80));wr(hit+0x94,0xC350);if(signed_half(hit+0xA)<0)invoke(c,0x80015B00,uint32_t(~signed_half(hit+0xA)),255,2,128);counter(8,"vehicle_hits");}}
    else if(e!=2&&e!=9)return;
    if(e==9){if(rd(o+0x84)==c->gpr[6])wr(o+0x84,0);return;}
    if(e!=2){common_sound(c,0x38,o+0x34);fx(c,o+0x34,0x16);}invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;counter(8,"stars_retired");
}
void disco(CPUState* c,uint32_t){
    const auto o=c->gpr[4];c->gpr[2]=0;
    if(c->gpr[5]==4){wr(Active,rd(Active)-1);counter(8,"retired");return;}
    if(c->gpr[5]==9){if(rd(o+0x84)==c->gpr[6])wr(o+0x84,0);return;}if(c->gpr[5]!=0)return;
    wr(o+0x50,rd(o+0x50)+rd(o+0x8C));half(o+0x44,signed_half(o+0x44)+91);half(o+0x46,signed_half(o+0x46)+68);if(c->gpr[6])invoke(c,0x8002CF74,o);
    wr(o+0x8C,si(o+0x8C)<0?rd(o+0x8C)+56:0);const int tick=signed_half(o+0x94)+1;half(o+0x94,tick);
    if(!(tick&15)){
        constexpr unsigned Kinds[]={0x30,0x2E,0x2F};const unsigned color=random(c)*3>>15;
        const auto shot=model(c,rd(o+0x5C),Kinds[color]);const int b=int(random(c)),a=int(random(c));
        if(shot){
            worldshot(shot,rd(o+0x80),Star,0x60000410,psx_mod_read_half(o+0x1C));half(shot+0x1A,0x30);psx_mod_write_byte(shot+0x18,uint8_t(color));
            // The original stores the cosine/cosine result into X twice.
            wr(shot+0x4C,uint32_t(mul(mul(trig(a,true),trig(b,true))/4096,si(o+0x58))/4096));wr(shot+0x50,uint32_t(mul(trig(a),si(o+0x58))/4096));
            wr(shot+0x84,rd(o+0x84));invoke(c,0x8002CF74,shot);invoke(c,0x8002CC48,o,shot);invoke(c,0x80030CB0,shot,120);counter(8,"stars");
        }
    }
    if(tick==240){
        if(const auto launcher=invoke(c,0x8002CD78,o)){if(!psx_mod_read_half(launcher+0x1C))invoke(c,0x8003A368,launcher);else wr(launcher+4,rd(launcher+4)&~0x80000u);}
        invoke(c,0x8004DC94,o);invoke(c,0x800307CC,invoke(c,0x8002CCBC,o));c->gpr[2]=UINT32_MAX;
    }
}
uint32_t fire_disco(CPUState* c,uint32_t o,uint32_t owner){
    const auto socket=invoke(c,0x8002C5F4,o,0x8000);if(!socket)return 0;
    const auto shot=model(c,rd(o+0x5C),0x31);if(!shot)return 0;
    const auto effect=model(c,shared(c),2,0x80);if(!effect){invoke(c,0x800309A0,shot);return 0;}
    half(shot+0xA,signed_half(owner+0xA));copy(shot+0x4C,socket+4,12);wr(shot+4,0x80);wr(shot,Disco);half(shot+0x1C,50);wr(shot+0x8C,uint32_t(-2048));wr(shot+0x80,owner);wr(shot+0x84,rd(owner+0xF0));
    for(auto p=rd(shot+0x10);p;p=rd(p+0xC)){wr(p,StopAnim);if(rd(p+4)&0x10)wr(p+4,rd(p+4)|0x400);}
    invoke(c,0x8002D1DC,shot);invoke(c,0x80030B78,shot);invoke(c,0x8002CC48,o,shot);
    wr(effect+4,rd(effect+4)|0x10);wr(effect,0x8004CA68);invoke(c,0x8002CA94,o,socket,effect);if(rd(o+0x10))invoke(c,0x8002C05C,rd(o+0x10));if(!(rd(owner+4)&4))invoke(c,0x80030BF0,o);
    phrase(c,o,1);wr(Active,rd(Active)+1);consume(c,o,8,false);wr(o+4,rd(o+4)|0x80000);return 360;
}
void bee_depart(CPUState* c,uint32_t){
    const auto o=c->gpr[4];c->gpr[2]=0;if(c->gpr[5])return;move(o,0x88,false);const auto ticks=uint8_t(psx_mod_read_byte(o+0x86)-1);psx_mod_write_byte(o+0x86,ticks);if(!ticks){invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
}
void bees(CPUState* c,uint32_t){
    const auto o=c->gpr[4],event=c->gpr[5],other=c->gpr[6];c->gpr[2]=0;
    if(event==1){
        for(unsigned i=0;i<16;++i)if(const auto bee=model(c,rd(o+0x5C),0x3A,0x94)){
            for(unsigned n=0;n<12;n+=4)wr(bee+0x4C+n,random(c)*2-0x8000);copy(bee+0x34,bee+0x4C,12);wr(bee+4,0x410);psx_mod_write_byte(bee+0x86,uint8_t((random(c)*8>>15)+24));invoke(c,0x8002CC48,o,bee);
        }
        // AABB +/- 0x4000, native collision stream followed by terminator.
        wr(o+0x60,o+0x98);wr(o+0x98,1);for(unsigned i=0;i<3;++i){wr(o+0x9C+i*4,uint32_t(-0x4000));wr(o+0xA8+i*4,0x4000);}wr(o+0xB4,0);wr(o+0x58,0x8000);
        const auto v=voice(c);psx_mod_write_byte(o+9,uint8_t(v));invoke(c,0x8001E580,v,sound(o),0,o+0x4C);wr(Active,rd(Active)+1);return;
    }
    if(event==4){invoke(c,0x8001DE78,uint32_t(int8_t(psx_mod_read_byte(o+9))));wr(Active,rd(Active)-1);counter(9,"retired");return;}
    if(event==9){if(rd(o+0x84)==other)wr(o+0x84,rd(o+0x80));return;}
    if(event==0){
        move(o);const int surface=sub(int32_t(invoke(c,0x8001B750,rd(o+0x4C),rd(o+0x54))),si(o+0x58));if(si(o+0x50)>surface)wr(o+0x50,uint32_t(surface));
        half(o+0x46,signed_half(o+0x46)+136);half(o+0x44,signed_half(o+0x44)+91);
        if(other){invoke(c,0x8002CF74,o);invoke(c,0x8001E2C8,uint32_t(int8_t(psx_mod_read_byte(o+9))),invoke(c,0x8001E478,o+0x34));}
        const auto target=rd(o+0x84);if(target)for(unsigned i=0;i<12;i+=4)wr(o+0x88+i,uint32_t(std::clamp(add(si(o+0x88+i),sub(si(target+0x4C+i),si(o+0x4C+i))/256),-0x13AB,0x13AB)));
        const auto timer=uint16_t(psx_mod_read_half(o+0x96)-1);half(o+0x96,timer);if(timer)return;
        if(const auto bee=rd(o+0x10)){
            invoke(c,0x8002CCBC,bee);for(unsigned i=0;i<12;i+=4)wr(bee+0x4C+i,rd(bee+0x4C+i)+rd(o+0x4C+i));wr(bee,BeeDepart);wr(bee+4,rd(bee+4)|0x84);
            wr(bee+0x88,(random(c)*1024>>15)-512);wr(bee+0x8C,0u-((random(c)*1024>>15)+1024));wr(bee+0x90,(random(c)*1024>>15)-512);psx_mod_write_byte(bee+0x86,240);invoke(c,0x8003066C,bee);half(o+0x96,15);
        }else{invoke(c,0x800309A0,o);c->gpr[2]=UINT32_MAX;}
    }else if(event==3&&other){
        Scratch s(c);invoke(c,0x8002F798,o,other);invoke(c,0x80024148,o+0x20,other+0x14,s.p);fx(c,s.p,0x17);
        for(unsigned i=0;i<3;++i)wr(o+0x88+i*4,uint32_t(add(si(o+0x88+i*4),signed_half(other+0x20+i*2)/2)));
        const auto hit=rd(other);if(hit&&psx_mod_read_byte(hit+8)==2){
            wr(hit+0x80,uint32_t(si(hit+0x80)>>2));const int surface=int32_t(invoke(c,0x8002CFBC,hit,hit+0x4C,0,0));if(si(hit+0x38)>sub(surface,0xF000))wr(hit+0x84,0xFFFA0A80);wr(hit+0x88,uint32_t(si(hit+0x88)>>2));common_sound(c,0x3E,o+0x4C);
            if(signed_half(hit+0xA)<0)invoke(c,0x80015ADC,uint32_t(~signed_half(hit+0xA)),8);if(!(rd(o+4)&0x10000)){wr(o+4,rd(o+4)|0x10000);half(o+0x96,240);}counter(9,"vehicle_hits");
        }
    }
}
uint32_t fire_bees(CPUState* c,uint32_t o,uint32_t owner){
    const auto effect=model(c,shared(c),2,0x80);if(!effect)return 0;
    const auto shot=invoke(c,0x80042408,owner,o,0xFFFF,0xB8,effect);if(!shot){invoke(c,0x800309A0,effect);return 0;}
    worldshot(shot,owner,Bees,0x60000084,5);wr(shot+0x5C,rd(o+0x5C));for(unsigned i=0;i<12;i+=4)wr(shot+0x88+i,uint32_t(original_specials::shift(rd(owner+0x80+i),5,12)));wr(shot+0x8C,rd(shot+0x8C)-128);
    wr(shot+0x84,rd(owner+0xF0)?rd(owner+0xF0):owner);half(shot+0x96,600);invoke(c,0x800305FC,shot);
    // RegisterWorld does not send initialize (unlike RegisterLocal).
    const auto saved=*c;c->gpr[4]=shot;c->gpr[5]=1;c->gpr[6]=0;bees(c,Bees);*c=saved;
    wr(effect+4,rd(effect+4)|0x10);wr(effect,0x8004CA68);invoke(c,0x8004E414,shot+0x4C,0x08008080);phrase(c,o,1);common_sound(c,0x14,shot+0x4C);
    if(rd(o+0x10))invoke(c,0x8002C05C,rd(o+0x10));if(!(rd(owner+4)&4))invoke(c,0x80030BF0,o);consume(c,o,9);return 600;
}
void register_callbacks(){
    for(unsigned kind:{0u,2u,3u,4u,5u,6u,8u,9u,10u,11u})psx_mod_register_guest_function_plugin(Plugin,Weapon+kind*4,weapon);
    struct Binding{uint32_t address;void(*callback)(CPUState*,uint32_t);};
    constexpr Binding bindings[]={{Radar,radar},{RadarHit,radar_hit},{Bullet,bullet},{Fume,fume},{Fireball,fireball},{Quake,quake},{Vortex,vortex},{Trail,trail},{Horn,horn},{Missile,missile},{Disco,disco},{Star,star},{Bees,bees},{StopAnim,stop_anim},{Blast,blast_event},{MissileTrail,missile_trail},{Bomblet,bomblet},{BombletHit,bomblet_hit},{BeeDepart,bee_depart}};
    for(const auto& b:bindings)psx_mod_register_guest_function_plugin(Plugin,b.address,b.callback);
}
}
