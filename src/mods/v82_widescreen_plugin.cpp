/* Native-wide compositor adapted from our V8 Custom Renderer. Sequel frustum,
 * row-padding and packet-arena seams follow the authorized V82Compat reference
 * (copyright (c) 2026 flaffy, MIT; tools/third_party/LICENSE_RECOMPONE), checked
 * against SLUS-00868. No generated source or guest instruction is rewritten. */
#include "mod_plugins.h"
#include "cpu_state.h"
#include "gpu.h"
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <iterator>

namespace {
constexpr const char* Plugin="v82.widescreen";
constexpr uint32_t GP=0x8006A7F0, Pool=0x80200000, PoolBytes=0x80000;
// 80200000..802FFFFF is reserved for the two packet arenas. Combined content
// metadata begins at80300000; its heap begins at80400000. Both features opt in
// to8MiB RAM, and ordinary retail code cannot allocate into this reservation.
uint32_t vblanks=0,last_world=0,sky_slot=0,sky_head=0,hud_slot=0,hud_head=0;
bool enabled=false,world_seen=false;
int cull_margin=0;
uint32_t rd(uint32_t p){return psx_mod_read_word(p);}
void wr(uint32_t p,uint32_t v){psx_mod_write_word(p,v);}
// The committed option, not the host window, determines guest geometry. Peers
// using Fit may have different monitors; both generate the same 32:9 scene.
int margin(){return enabled?cull_margin:0;}
int world(){return world_seen && vblanks-last_world<=12;}
void vblank(){++vblanks;}
bool ram(uint32_t p){return (p&3)==0 && (p&0x1FFFFFFF)<0x800000;}
int command_words(uint8_t op){
    if(op>=0x20 && op<=0x3F){int n=(op&8)?4:3;return 1+n+((op&4)?n:0)+((op&16)?n-1:0);}
    if(op>=0x60 && op<=0x7F)return (((op>>3)&3)?2:3)+((op&4)?1:0);
    if(op==0 || op==1 || (op>=0xE1 && op<=0xE6))return 1;
    return 0;
}
int anchor(uint32_t c,uint8_t op){
    int left=int16_t(psx_mod_read_half(c+4)),right=left;
    if(op>=0x20 && op<=0x3F){
        const int n=(op&8)?4:3,stride=4+((op&4)?4:0)+((op&16)?4:0);
        for(int i=1;i<n;++i){const int x=int16_t(psx_mod_read_half(c+4+i*stride));left=std::min(left,x);right=std::max(right,x);}
    }else{
        const unsigned size=(op>>3)&3;
        right+=size==1?1:size==2?8:size==3?16:psx_mod_read_half(c+((op&4)?12:8));
    }
    return 3*(left+right)<640?-1:3*(left+right)>1280?1:0;
}
void tag_chain(uint32_t slot,uint32_t stop,bool sky){
    if(!slot || !ram(slot))return;
    uint32_t packets[512],count=0,current=rd(slot)&0xFFFFFF;
    while(current!=stop){
        if(!ram(current)||current>=0x800000||count==512){psx_mod_counter_add("v82.ws.tag_chain_rejected",1);return;}
        const uint32_t p=current|0x80000000,tag=rd(p),words=tag>>24;
        if(current+4+words*4>0x800000)return;
        for(unsigned i=0;i<words;){int n=command_words(psx_mod_read_byte(p+7+4*i));if(n<=0||unsigned(n)>words-i)return;i+=n;}
        packets[count++]=p;current=tag&0xFFFFFF;
    }
    for(unsigned j=0;j<count;++j)for(unsigned i=0,words=rd(packets[j])>>24;i<words;){
        const auto c=packets[j]+4+4*i;const auto op=psx_mod_read_byte(c+3);
        if(op>=0x20 && op<=0x3F){if(sky)gpu_ws_tag_background_prim(c-4);else gpu_ws_tag_hud_prim(c-4,anchor(c,op));}
        else if(!sky && op>=0x60 && op<=0x7F)gpu_ws_tag_hud_prim(c-4,anchor(c,op));
        i+=command_words(op);
    }
    psx_mod_counter_add(sky?"v82.ws.sky_tagged":"v82.ws.hud_tagged",count);
}
struct Site{uint32_t pc,word;};
constexpr Site instructions[]={
    {0x8001C250,0x00021FC2}, // terrain width was loaded into v0
    {0x8002E024,0x00031823}, // object width was loaded into v1
    {0x80029320,0x1D000006}, // terrain left screen test; coordinate copies only
    {0x80029338,0x05600077}, // final left vertex, after its pending load retires
    {0x80029344,0x0101402A}, // terrain right screen test
    {0x80014B60,0x00711823}, // packet usage relative to original arena
    {0x80014BBC,0xAF820610}, // new packet cursor
    {0x80014BD8,0xAF830CDC}, // new packet limit
    {0x8001CE14,0x03E00008}, // finished sky packets
};
void instruction(CPUState* c,uint32_t pc){
    if(!enabled||c->gpr[28]!=GP)return;
    const auto m=margin();
    if(pc==0x80014B60){
        const auto b=rd(GP+0x20)&1,base=Pool+b*PoolBytes,cur=c->gpr[3];
        if(cur>=base&&cur<=base+PoolBytes)c->gpr[3]=0x80074A68+(b<<17)+(cur-base);
    }else if(pc==0x80014BBC){if(world() && psx_mod_display_width()==320 && rd(GP+0xEDC)==320)c->gpr[2]=Pool+(rd(GP+0x20)&1)*PoolBytes;}
    else if(pc==0x80014BD8){if(world() && psx_mod_display_width()==320 && rd(GP+0xEDC)==320)c->gpr[3]=Pool+((rd(GP+0x20)&1)+1)*PoolBytes;}
    else if(pc==0x8001CE14){if(m)tag_chain(sky_slot,sky_head,true);sky_slot=0;}
    else if(m && psx_mod_display_width()==320 && rd(GP+0xEDC)==320){
        if(pc==0x8001C250 && c->gpr[2]==320){c->gpr[2]+=2*m;psx_mod_counter_add("v82.ws.terrain_widened",1);}
        else if(pc==0x8002E024 && c->gpr[3]==320){c->gpr[3]+=2*m;psx_mod_counter_add("v82.ws.objects_widened",1);}
        else if(pc==0x80029320){for(unsigned i=8;i<=10;++i)c->gpr[i]+=m;}
        else if(pc==0x80029338){c->gpr[11]+=m;}
        else if(pc==0x80029344){
            const unsigned offsets[]={8,0x28,0xA8,0xC8};
            for(unsigned i=0;i<4;++i)c->gpr[8+i]=int16_t(psx_mod_read_half(c->gpr[7]+offsets[i]))+m;
            c->gpr[1]+=2*m;
        }
    }
}
constexpr Site functions[]={
    {0x8001C134,0x3C011F80},{0x8001BE68,0x27BDFFF0},{0x800288E0,0x23BDFFF0},
    {0x8001C910,0x27BDFFA8},{0x80037A00,0x27BDFFB8},{0x8003827C,0x27BDFFE8},
};
void entry(CPUState* c,uint32_t pc){
    if(!enabled||c->gpr[28]!=GP)return;
    const auto guard=std::find_if(std::begin(functions),std::end(functions),[=](auto s){return s.pc==pc;});
    if(guard==std::end(functions)||rd(pc)!=guard->word)return;
    if(pc==0x8001C134){world_seen=true;last_world=vblanks;}
    const auto m=margin();
    if(!m || psx_mod_display_width()!=320 || rd(GP+0xEDC)!=320)return;
    if(pc==0x8001C134){
        // The first gameplay frame follows a native menu/loading frame. Move
        // only subsequent allocations; any already-linked native packets stay
        // valid until submission. Movies and selectors also use 320px modes,
        // so display width alone is not a sufficient lifetime gate.
        const auto b=rd(GP+0x20)&1,native=0x80074A68+(b<<17),cur=rd(GP+0x610);
        if(cur>=native && cur<native+0x20000){
            wr(GP+0x610,Pool+b*PoolBytes+(cur-native));
            wr(GP+0xCDC,Pool+(b+1)*PoolBytes);
        }
    }else if(pc==0x8001BE68){
        // Preserve the native grid's four-sample step and all four cell corners.
        const int first=int32_t(c->gpr[4]),last=int32_t(c->gpr[5]),row=int32_t(c->gpr[6]);
        if(last>first && row>=0 && row<2044){
            c->gpr[4]=uint32_t(std::max(0,first-16));c->gpr[5]=uint32_t(std::min(2044,last+16));
        }
    }else if(pc==0x800288E0){
        // A single4x4 terrain cell can subdivide into bounded native packets.
        // Keep an8KiB reserve before the next cell; count any capacity shortfall.
        const auto base=Pool+(rd(GP+0x20)&1)*PoolBytes,cur=c->gpr[6];
        if(c->gpr[4]>=2044 || c->gpr[5]>=2044 || cur<base || cur>base+PoolBytes-8192){
            c->gpr[2]=cur;psx_mod_finish_function(c);psx_mod_counter_add("v82.ws.terrain_budget_reject",1);
        }
    }else if(pc==0x8001C910){sky_slot=rd(GP+0xCE0)+0x3FFC;sky_head=rd(sky_slot)&0xFFFFFF;}
    else if(pc==0x80037A00){hud_slot=c->gpr[6];hud_head=rd(hud_slot)&0xFFFFFF;}
    else if(pc==0x8003827C){if(hud_slot==c->gpr[6])tag_chain(hud_slot,hud_head,false);hud_slot=0;}
}
void activate(){
    enabled=false;world_seen=false;vblanks=last_world=sky_slot=hud_slot=0;
    if(!psx_mod_set_main_ram_8mb(1))return;
    char view[16]="Fit";
    if(!psx_mod_option_value("v82.enhancement.widescreen","widescreen","aspect",view,sizeof(view)))std::strcpy(view,"Fit");
    const unsigned n=!std::strcmp(view,"32:9")?32:!std::strcmp(view,"21:9")?21:16;
    cull_margin=!std::strcmp(view,"Fit")?267:(240*n/9-320+1)/2;
    psx_mod_set_fixed_display_aspect(n,9);
    if(!std::strcmp(view,"Fit"))psx_mod_set_adaptive_display_aspect(32,9);
    enabled=true;psx_mod_set_world_scene_predicate(world);
}
PSX_MOD_CONSTRUCTOR(register_widescreen){
    psx_mod_register_activation_plugin(Plugin,activate);psx_mod_register_vblank_plugin(Plugin,vblank);
    for(const auto& s:instructions)psx_mod_register_instruction_plugin(Plugin,s.pc,s.word,instruction);
    for(const auto& s:functions)psx_mod_register_function_entry_plugin(Plugin,s.pc,entry);
}
}
