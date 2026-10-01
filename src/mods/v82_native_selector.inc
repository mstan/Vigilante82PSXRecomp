/* V82VehicleRegistry native carousel seams, adapted for psxrecomp guest calls.
 * Copyright (c) 2026 flaffy. MIT; tools/third_party/LICENSE_RECOMPONE. */
namespace selector {
constexpr uint32_t Host=0x8FFF0300, S=Meta+0x100, Text=Meta+0x400, Colors=Meta+0x500;
enum Field { Base, Active, Guest, Previous, First, Last, Proxy, Latch,
             Enemy, Accepted, AcceptedProxy, Player, Preview, Context };
int get(unsigned field) { return int32_t(rd(S+field*4)); }
void set(unsigned field,int value) { wr(S+field*4,uint32_t(value)); }
uint32_t npc(unsigned row) { return S+0x100+row*16; }
const char* Names[]={"'67 Rattler","'70 Clydesdale","'74 Strider","'69 Jefferson",
    "'70 Van","'72 Moth Truck","'73 Glenn 4x4","'75 Palamino","'76 Leprechaun",
    "'70 Stag Pickup","'66 School Bus","'69 Manta","'64 Luxo Saucer"};
int guest() { const auto i=get(Guest); return get(Active)&&i>=0&&unsigned(i)<package.vehicles.size()?i:-1; }
uint32_t caller(CPUState* c) { return c->gpr[31]-uint32_t(get(Base))+0x80100000; }
void native(CPUState* c,uint32_t fn) {
    auto ra=c->gpr[31];c->pc=0;psx_dispatch_call(c,fn,ra);c->gpr[31]=ra;
}
void clear_npcs() { for(unsigned i=0;i<4;++i) {wr(npc(i),UINT32_MAX);wr(npc(i)+4,UINT32_MAX);wr(npc(i)+8,0);} }
void begin(CPUState* c) {
    if(c->gpr[5]==1||c->gpr[5]==2)c->gpr[4]=0;
    set(Active,c->gpr[5]==1||c->gpr[5]==2);set(Guest,-1);set(Previous,-1);set(First,0);set(Last,17);
    set(Proxy,0);set(Latch,0);set(Enemy,0);set(Accepted,-1);set(AcceptedProxy,-1);
    // This native carousel always writes P1 (SHELL+7998). A1 selects the
    // enemy-editor policy: 1 is Arcade, 2 is Survival, not player two.
    set(Player,0);set(Context,c->gpr[5]);
    wr(S+0xF0,Magic);clear_npcs();psx_mod_counter_add("v82.selector.begin",1);
}
void release_previews(CPUState* c);
void slot(CPUState* c) {
    release_previews(c);
    auto current=int(c->gpr[30]);
    if(get(Previous)<0) {set(First,current);set(Proxy,current);set(Last,17);}
    if(guest()>=0)current=get(Proxy);
    set(Previous,current);c->gpr[30]=uint32_t(current);
}
void navigate(CPUState* c) {
    if(!get(Active)||get(Previous)<0)return;
    const auto input=rd(0x8006B508);bool left=input&0x80000000,right=input&0x20000000;
    if(left==right)return;
    const int direction=right?1:-1, target=int(c->gpr[30]), previous=get(Previous);
    int index=guest();
    if(index>=0&&get(Latch)==direction) {c->gpr[30]=get(Proxy);return;}
    if(index<0) {
        if(!((right&&target<previous)||(left&&target>previous))) {set(Previous,target);return;}
        set(First,right?target:previous);set(Last,right?previous:target);set(Proxy,get(First));
        index=right?0:int(package.vehicles.size())-1;
    } else {
        index+=direction;
        if(index<0||unsigned(index)>=package.vehicles.size()) {
            const int retail=right?get(First):get(Last);
            set(Guest,-1);set(Previous,retail);set(Latch,direction);c->gpr[30]=retail;return;
        }
    }
    set(Guest,index);set(Previous,get(Proxy));set(Latch,direction);c->gpr[30]=get(Proxy);
    psx_mod_counter_add("v82.selector.original_highlighted",1);
}
void ensure_preview(CPUState* c,unsigned i) {
    const auto& v=package.vehicles.at(i);
    if(v.transform!=65535)wr(state(i)+4,build_bank(c,v.transform));
    wr(state(i)+124,build_bank(c,v.preview));
}
void release_bank(CPUState* c,unsigned i) {
    const auto p=bank_state(i), runtime=rd(p), bin=rd(p+4), anm=rd(p+12);
    // The native selector already retired textures and may have freed its
    // editable group block without clearing the pointer. Match the reference
    // ownership checks instead of invoking the full gameplay bank destructor.
    if(owned(runtime)) {
        call(c,0x8001FC3C,runtime); // Also retire wheel-bank texture handles.
        const auto groups=rd(runtime+16), original=rd(bin+4);
        if(groups!=original&&owned(groups))release(groups);
        release(runtime);
    }
    if(owned(bin))release(bin);
    if(owned(anm))release(anm);
    for(unsigned off=0;off<32;off+=4)wr(p+off,0);
}
void release_previews(CPUState* c) {
    for(unsigned i=0;i<package.vehicles.size();++i) {
        if(!rd(state(i)+124))continue;
        release_bank(c,package.vehicles[i].preview);
        if(package.vehicles[i].transform!=65535)release_bank(c,package.vehicles[i].transform);
        wr(state(i)+124,0);wr(state(i)+4,0);
    }
}
void end(CPUState* c) {
    const int index=get(Enemy)?get(Accepted):guest();
    const int proxy=get(Enemy)?get(AcceptedProxy):get(Proxy);
    if(index>=0&&c->gpr[2]==unsigned(proxy)) {
        c->gpr[2]=64+index;
        psx_mod_write_byte(c->gpr[28]+0x1104+get(Player),uint8_t(64+index));
        psx_mod_counter_add("v82.selector.original_accepted",1);
    }
    release_previews(c);set(Active,0);set(Guest,-1);set(Enemy,0);
}
void enemy_slot(CPUState* c) {
    const unsigned row=c->gpr[19];if(!get(Active)||row>=4)return;
    const auto p=npc(row), address=0x8006B8F6+row;
    int current=int8_t(psx_mod_read_byte(address)), previous=int32_t(rd(p+4)), index=int32_t(rd(p));
    if(current<0||current>=18) {wr(p,UINT32_MAX);set(Guest,-1);return;}
    const auto input=rd(0x8006B508);bool left=input&0x80000000,right=input&0x20000000;
    if(previous<0) {wr(p+4,current);set(Guest,-1);return;}
    if(index<0) {
        if(right&&current<previous) {index=0;wr(p+8,current);}
        else if(left&&current>previous) {index=int(package.vehicles.size())-1;wr(p+8,current);}
    } else if(current!=previous&&(left!=right)) {
        index+=right?1:-1;
        if(index<0||unsigned(index)>=package.vehicles.size())index=-1;
        else current=int(rd(p+8));
    } else current=int(rd(p+8));
    wr(p,index);wr(p+4,current);psx_mod_write_byte(address,uint8_t(current));set(Guest,index);
    // Each native row uses a distinct retail proxy, preserving native quantity
    // edits without making two different imported cars the same identity.
    bool used[18]{};
    for(unsigned r=0;r<4;++r)if(int32_t(rd(npc(r)))<0) {
        auto t=psx_mod_read_byte(0x8006B8F6+r);if(t<18)used[t]=true;
    }
    for(unsigned r=0;r<4;++r)if(int32_t(rd(npc(r)))>=0) {
        unsigned proxy=rd(npc(r)+8);
        if(proxy>=18||used[proxy])for(proxy=0;proxy<18&&used[proxy];++proxy){}
        if(proxy>=18)fail("v82.selector.no_npc_proxy");
        used[proxy]=true;wr(npc(r)+8,proxy);wr(npc(r)+4,proxy);psx_mod_write_byte(0x8006B8F6+r,uint8_t(proxy));
    }
}
void preview(CPUState* c,int index) {
    ensure_preview(c,unsigned(index));const auto& v=package.vehicles[index];
    c->gpr[4]=rd(state(index)+124);c->gpr[5]=v.preview_kind;c->gpr[6]=state(index)+8;
    const int previous=constructing;constructing=index;
    { WordScope wheels(WheelSlot,rd(state(index)+4)?rd(state(index)+4):rd(WheelSlot));native(c,0x8003C464); }
    constructing=previous;
    if(c->gpr[2]) {remember(c->gpr[2],index);psx_mod_write_byte(c->gpr[2]+0xDC,uint8_t(64+index));set(Preview,c->gpr[2]);}
    psx_mod_counter_add("v82.selector.preview_created",1);
}
void callback(CPUState* c,uint32_t pc) {
    const unsigned kind=(pc-Host)/4;
    if(kind<16) {
        const auto base=c->gpr[31]-rd(c->gpr[31]+24);
        if(enabled)init();
        set(Base,base);
        if(!enabled)return;
        switch(kind) {
        case 0:begin(c);break;
        case 1:slot(c);break;
        case 2:if(guest()>=0)c->gpr[2]=0;break;
        case 3:navigate(c);break;
        case 4:end(c);break;
        case 5:if(guest()>=0)c->gpr[2]=state(guest())+8+c->gpr[18];break;
        case 6:if(guest()>=0)c->gpr[4]=Colors+guest()*16;break;
        case 7:if(guest()>=0)c->gpr[2]=Colors+guest()*16;break;
        }
        return;
    }
    const auto ra=caller(c);int index=enabled?guest():-1;
    switch(kind) {
    case 16:
        if(enabled&&ra==0x80107A1C) {
            set(Accepted,index);set(AcceptedProxy,get(Proxy));set(Enemy,1);set(Guest,-1);index=-1;
            release_previews(c);
        } else if(enabled&&ra==0x8010689C&&get(Enemy)) {
            set(Guest,get(Accepted));set(Enemy,0);set(Previous,-1);clear_npcs();index=guest();
        }
        if(index>=0&&(ra==0x80106A7C||ra==0x80106B14)) {
            const auto name=ra==0x80106B14?std::string(Names[index]):package.vehicles[index].name;
            for(unsigned n=0;n<=name.size();++n)psx_mod_write_byte(Text+n,n<name.size()?uint8_t(name[n]):0);
            c->gpr[5]=Text;
        }
        native(c,0x8001A3B0);break;
    case 17:
        if(index>=0&&(ra==0x80106BA8||ra==0x80106C58||ra==0x801075E8)) {
            ensure_preview(c,index);c->gpr[4]=rd(state(index)+124);
            const auto color=Colors+index*16;
            c->gpr[5]=psx_mod_read_byte(color+2)|(psx_mod_read_byte(color+3)<<16)|(psx_mod_read_byte(color+4)<<8);
        }
        native(c,0x8001EF34);break;
    case 18:if(index>=0)preview(c,index);else native(c,0x8003C464);break;
    case 19:
        if(enabled&&get(Enemy))enemy_slot(c);
        index=enabled?guest():-1;
        if(index>=0)preview(c,index);else native(c,0x80036BE0);break;
    case 20:
        native(c,0x800149AC);
        if(!(rd(0x8006B508)&0xA0000000))set(Latch,0);
        if(enabled&&get(Enemy)&&ra==0x80105B20) {
            const auto row=c->gpr[22]>>1;if(row<4)set(Guest,int32_t(rd(npc(row))));
        }
        break;
    case 21:if(index>=0&&ra==0x80106CD4)c->gpr[5]=0x15A;native(c,uint32_t(get(Base))+0x536C);break;
    case 22:
        if(index>=0&&!get(Enemy)&&(ra==0x801069EC||ra==0x80106A00)) {
            if(index==12&&ra==0x80106A00)break;
            presentation::portrait(c,index,ra==0x80106A00);
        }
        native(c,uint32_t(get(Base))+0x109FC);break;
    case 23:
        if(index>=0&&package.vehicles[index].flying()&&(ra==0x80106DE0||ra==0x8010838C))break;
        native(c,0x8001E28C);break;
    case 24:
        if(index<0||get(Enemy)||ra!=0x8010798C||!presentation::voice(c,unsigned(index),uint32_t(get(Base))))native(c,0x8001E188);
        break;
    }
}
void apply_npcs(CPUState* c) {
    if(rd(S+0xF0)!=Magic)return;
    const unsigned players=int8_t(psx_mod_read_byte(c->gpr[28]+0x31))<9?2:4;
    for(unsigned participant=players;participant<6;++participant) {
        const auto address=c->gpr[28]+0x1104+participant;
        const int type=int8_t(psx_mod_read_byte(address));
        if(type<0||type>=18)continue;
        // Native quantity expansion may repeat/reorder a row. Translate its
        // unique proxy wherever it occurs, preserving empty participants.
        for(unsigned row=0;row<4;++row) {
            const auto index=int32_t(rd(npc(row)));
            if(index>=0&&unsigned(index)<package.vehicles.size()&&rd(npc(row)+8)==unsigned(type)) {
                psx_mod_write_byte(address,uint8_t(64+index));break;
            }
        }
    }
}
}
