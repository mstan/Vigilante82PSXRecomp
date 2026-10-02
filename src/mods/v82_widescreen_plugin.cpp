/* Native-wide compositor adapted from our V8 Custom Renderer. Sequel frustum,
 * row-padding and packet-arena seams follow the authorized V82Compat reference
 * (copyright (c) 2026 flaffy, MIT; tools/third_party/LICENSE_RECOMPONE), checked
 * against SLUS-00868. No generated source or guest instruction is rewritten. */
#include "mod_plugins.h"
#include "cpu_state.h"
#include "gpu.h"
#include "psx_netplay.h"
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
// Counters observe the native walker and plane test without changing results.
int64_t object_planes[3]={};
int32_t object_radius=0;
bool object_sample=false;
bool outside_s32(int64_t v){return v<INT32_MIN || v>INT32_MAX;}
void compare_terrain_product(CPUState* c){
    const int64_t product=int64_t(int32_t(c->gpr[3]))*int32_t(c->gpr[2]);
    psx_mod_counter_add("v82.ws.edge_products",1);
    if(outside_s32(product))psx_mod_counter_add("v82.ws.edge_product_overflow",1);
}

uint32_t rd(uint32_t p){return psx_mod_read_word(p);}
void wr(uint32_t p,uint32_t v){psx_mod_write_word(p,v);}
// The committed option, not the host window, determines guest geometry. Peers
// using Fit may have different monitors; both generate the same 32:9 scene.
bool renderer_enabled(){return enabled || psx_netplay_active();}
int viewport_width(){return int(rd(GP+0xEDC));}
bool battle_view(){const int w=viewport_width();return psx_mod_display_width()==320 && (w==320 || (w==160 && gpu_ws_netplay_local_viewport_width()>160));}
int margin(){
    if(!renderer_enabled())return 0;
    if(viewport_width()==160){const int w=gpu_ws_netplay_local_viewport_width();return w>160?(w-160)/2:0;}
    return cull_margin;
}
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
    {0x80024E6C,0x14200005}, // subdivision right bound, all X loads retired
    {0x80024E84,0x05010007}, // subdivision left bound, coordinate copies only
    {0x800250C4,0x14200005}, // subdivision right bound, all X loads retired
    {0x800250DC,0x05010007}, // subdivision left bound, coordinate copies only
    {0x800259F4,0x14200005}, // subdivision right bound, all X loads retired
    {0x80025A0C,0x05010007}, // subdivision left bound, coordinate copies only
    {0x80025C74,0x14200005}, // subdivision right bound, all X loads retired
    {0x80025C8C,0x05010007}, // subdivision left bound, coordinate copies only
    {0x800266C0,0x14200005}, // subdivision right bound, all X loads retired
    {0x800266D8,0x05010007}, // subdivision left bound, coordinate copies only
    {0x80026940,0x14200005}, // subdivision right bound, all X loads retired
    {0x80026958,0x05010007}, // subdivision left bound, coordinate copies only
    {0x800273A8,0x14200005}, // subdivision right bound, all X loads retired
    {0x800273C0,0x05010007}, // subdivision left bound, coordinate copies only
    {0x80027650,0x14200005}, // subdivision right bound, all X loads retired
    {0x80027668,0x05010007}, // subdivision left bound, coordinate copies only

    {0x8001C250,0x00021FC2}, // terrain width was loaded into v0
    {0x8001C304,0x2A220004}, // corner transformed; repair signed-halfword overflow
    {0x8001C400,0x00620018}, // observe lower-height edge interpolation
    {0x8001C43C,0x00620018},
    {0x8001C4B4,0x00620018}, // observe upper-height edge interpolation
    {0x8001C4F0,0x00620018},
    {0x8001C620,0x00901023}, // observe angular-sort determinant
    {0x8001C76C,0x0083102A}, // observe convex-hull product comparison
    {0x8002E2E0,0x03E00008}, // native whole-object visibility result
    {0x8002E024,0x00031823}, // object width was loaded into v1
    {0x80029320,0x1D000006}, // terrain left screen test; coordinate copies only
    {0x80029338,0x05600077}, // final left vertex, after its pending load retires
    {0x80029344,0x0101402A}, // terrain right screen test
    {0x80014B60,0x00711823}, // packet usage relative to original arena
    {0x80014BBC,0xAF820610}, // new packet cursor
    {0x80014BD8,0xAF830CDC}, // new packet limit
    {0x8001CE14,0x03E00008}, // finished sky packets
};
// Four native subdivision emitters, each with a 3D and a screen-space walker.
// Their root objects already passed the widened frustum. The walkers must also
// admit triangles wholly inside either reveal strip, beyond the retail 0..320.
constexpr uint32_t subdivision_right_sites[]={
    0x80024E6C,0x800250C4,0x800259F4,0x80025C74,
    0x800266C0,0x80026940,0x800273A8,0x80027650,
};
bool widen_subdivision(CPUState* c,uint32_t pc,int m){
    for(const auto right:subdivision_right_sites){
        if(pc!=right && pc!=right+24)continue;
        const int width=viewport_width();
        if(!world() || c->gpr[2]!=uint32_t(width))return true;
        if(pc==right){
            // The last X load has retired here. Replace the first branch's
            // predicate with the OR of all three widened right comparisons.
            // If all fail, the remaining native comparisons also fail.
            const int lo=std::min({int32_t(c->gpr[8]),int32_t(c->gpr[9]),int32_t(c->gpr[10])});
            const int hi=std::max({int32_t(c->gpr[8]),int32_t(c->gpr[9]),int32_t(c->gpr[10])});
            psx_mod_counter_add("v82.ws.subdivision_samples",1);
            if(hi>=-m && lo<width+m){
                if(hi<0)psx_mod_counter_add("v82.ws.subdivision_reveal_left",1);
                if(lo>=width)psx_mod_counter_add("v82.ws.subdivision_reveal_right",1);
            }
            c->gpr[1]=uint32_t(lo<width+m);
        }else{
            // Bias only these transient copies for the native >=0 left tests.
            // The 3D walker overwrites them with Y; the screen walker uses
            // pairwise X differences (the common bias cancels). Both emitters
            // reload the unchanged vertex records before writing packets.
            for(unsigned i=8;i<=10;++i)c->gpr[i]+=m;
            psx_mod_counter_add("v82.ws.subdivision_widened",1);
        }
        return true;
    }
    return false;
}
void instruction(CPUState* c,uint32_t pc){
    if(!renderer_enabled()||c->gpr[28]!=GP)return;
    const auto m=margin();
    if(pc==0x80014B60){
        const auto b=rd(GP+0x20)&1,base=Pool+b*PoolBytes,cur=c->gpr[3];
        if(cur>=base&&cur<=base+PoolBytes)c->gpr[3]=0x80074A68+(b<<17)+(cur-base);
    }else if(pc==0x80014BBC){if(world() && battle_view())c->gpr[2]=Pool+(rd(GP+0x20)&1)*PoolBytes;}
    else if(pc==0x80014BD8){if(world() && battle_view())c->gpr[3]=Pool+((rd(GP+0x20)&1)+1)*PoolBytes;}
    else if(pc==0x8001CE14){if(m)tag_chain(sky_slot,sky_head,true);sky_slot=0;}
    else if(m && battle_view()){
        if(widen_subdivision(c,pc,m))return;
        if(pc==0x8001C400 || pc==0x8001C43C || pc==0x8001C4B4 || pc==0x8001C4F0){
            compare_terrain_product(c);
        }else if(pc==0x8001C620){
            const int64_t a=int64_t(int32_t(c->gpr[3]))*(int64_t(int32_t(c->gpr[10]))-int32_t(c->gpr[19]));
            const int64_t b=int64_t(int32_t(c->gpr[2]))*int32_t(c->gpr[8]);
            const int64_t det=a-b;
            const int32_t native=int32_t(c->gpr[4]-c->gpr[16]);
            psx_mod_counter_add("v82.ws.sort_samples",1);
            if(outside_s32(a)||outside_s32(b)||outside_s32(det))psx_mod_counter_add("v82.ws.sort_overflow",1);
            if((det<0)!=(native<0) || (det==0)!=(native==0))psx_mod_counter_add("v82.ws.sort_sign_disagree",1);
        }else if(pc==0x8001C76C){
            const int count=int32_t(c->gpr[7]);
            if(count>=1 && count<16){
                const auto last=c->gpr[9]+count*8,prev=last-8;
                const int64_t dx=int64_t(int32_t(rd(last)))-int32_t(rd(prev));
                const int64_t dy=int64_t(int32_t(rd(last+4)))-int32_t(rd(prev+4));
                const int64_t a=dx*(int64_t(int32_t(c->gpr[11]))-int32_t(rd(last+4)));
                const int64_t b=dy*(int64_t(int32_t(c->gpr[10]))-int32_t(rd(last)));
                psx_mod_counter_add("v82.ws.hull_samples",1);
                if(outside_s32(a)||outside_s32(b))psx_mod_counter_add("v82.ws.hull_overflow",1);
                if((a<b)!=(int32_t(c->gpr[4])<int32_t(c->gpr[3])))psx_mod_counter_add("v82.ws.hull_sign_disagree",1);
            }
        }else if(pc==0x8002E2E0 && object_sample){
            const bool native=c->gpr[3]!=0;
            const bool precise=object_planes[0]<object_radius && object_planes[1]<object_radius && object_planes[2]<object_radius;
            psx_mod_counter_add(native?"v82.ws.object_visible":"v82.ws.object_rejected",1);
            if(native!=precise)psx_mod_counter_add("v82.ws.object_plane_disagree",1);
            if(!native){
                const int r=object_radius;
                psx_mod_counter_add(int32_t(c->gte_data[9])>=r?"v82.ws.object_reject_plane0":
                    int32_t(c->gte_data[10])>=r?"v82.ws.object_reject_plane1":"v82.ws.object_reject_plane2",1);
            }
            object_sample=false;
        }else if(pc==0x8001C250 && c->gpr[2]==uint32_t(viewport_width())){c->gpr[2]+=2*m;psx_mod_counter_add("v82.ws.terrain_widened",1);}
        else if(pc==0x8001C304){
            // The native corner at sp+D0 stores S4/S3 as signed halfwords.
            // At 32:9, range 80 and focal 256, X is 34160: truncation turns
            // the right ray negative and reverses the traversal polygon.
            // Preserve the native transform/side effects, then correct its
            // three 32-bit output coordinates using the untruncated corner.
            const int32_t x=int32_t(c->gpr[20]),y=int32_t(c->gpr[19]);
            psx_mod_counter_add("v82.ws.ray_samples",1);
            psx_mod_counter_add("v82.ws.ray_half_x_total",uint32_t(x));
            if(x>32767 || y>32767){
                const unsigned corner=c->gpr[17]-1;
                // The terrain wrapper moves SP into the 1KiB scratchpad.
                const auto output=c->gpr[5]&0x1FFFFFFF;
                if(corner>=4 || (output&3) || !((output<=0x7FFFF4) ||
                    (output>=0x1F800000 && output<=0x1F8003F4))){
                    psx_mod_counter_add("v82.ws.ray_output_rejected",1);return;
                }
                const int32_t v[3]={(corner&1)?x:-x,(corner&2)?y:-y,
                    int16_t(psx_mod_read_half(GP+0xDB4))*256};
                for(unsigned row=0;row<3;++row){
                    int64_t sum=int64_t(int32_t(c->gte_ctrl[5+row]))*4096;
                    for(unsigned col=0;col<3;++col){
                        unsigned i=row*3+col;
                        sum+=int64_t(int16_t(c->gte_ctrl[i/2]>>(16*(i&1))))*v[col];
                    }
                    wr(c->gpr[5]+row*4,uint32_t(sum>>12));
                }
                psx_mod_counter_add("v82.ws.ray_overflow_corrected",1);
            }
        }
        else if(pc==0x8002E024 && c->gpr[3]==uint32_t(viewport_width())){c->gpr[3]+=2*m;psx_mod_counter_add("v82.ws.objects_widened",1);}
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
    {0x8002E22C,0x00A03021},
    {0x8001C910,0x27BDFFA8},{0x80037A00,0x27BDFFB8},{0x8003827C,0x27BDFFE8},
};
void entry(CPUState* c,uint32_t pc){
    if(!renderer_enabled()||c->gpr[28]!=GP)return;
    const auto guard=std::find_if(std::begin(functions),std::end(functions),[=](auto s){return s.pc==pc;});
    if(guard==std::end(functions)||rd(pc)!=guard->word)return;
    if(pc==0x8001C134){world_seen=true;last_world=vblanks;}
    const auto m=margin();
    if(!m || !battle_view())return;
    if(pc==0x8002E22C){
        const auto p=c->gpr[4];object_sample=false;
        if(!ram(p) || (p&0x1FFFFFFF)>0x7FFFF4)return;
        const int64_t v[]={
            (int64_t(int32_t(rd(p)))-int32_t(rd(GP+0xF3C)))>>8,
            (int64_t(int32_t(rd(p+4)))-int32_t(rd(GP+0xF40)))>>8,
            (int64_t(int32_t(rd(p+8)))-int32_t(rd(GP+0xF44)))>>8};
        object_radius=int32_t(c->gpr[5])>>8;
        for(unsigned row=0;row<3;++row){
            int64_t sum=0;
            for(unsigned col=0;col<3;++col)sum+=int16_t(psx_mod_read_half(GP+0xFD8+(row*3+col)*2))*v[col];
            object_planes[row]=sum>>12;
        }
        if(v[0]<-32768 || v[0]>32767 || v[1]<-32768 || v[1]>32767 || v[2]<-32768 || v[2]>32767)
            psx_mod_counter_add("v82.ws.object_input_wrap",1);
        object_sample=true;
    }else if(pc==0x8001C134){
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
        psx_mod_counter_add("v82.ws.terrain_rows",1);
        if(last<=first)psx_mod_counter_add("v82.ws.terrain_empty_rows",1);
        if(last>first && row>=0 && row<2044){
            c->gpr[4]=uint32_t(std::max(0,first-16));c->gpr[5]=uint32_t(std::min(2044,last+16));
        }
    }else if(pc==0x800288E0){
        psx_mod_counter_add("v82.ws.terrain_cells",1);
        const int64_t cell_x=int64_t(c->gpr[4])*256+int32_t(rd(0x1F800084));
        const int64_t cell_z=int64_t(c->gpr[5])*256+int32_t(rd(0x1F80008C));
        if(cell_x<-32768 || cell_x+1024>32767)psx_mod_counter_add("v82.ws.cell_x_wrap",1);
        if(cell_z<-32768 || cell_z+1024>32767)psx_mod_counter_add("v82.ws.cell_z_wrap",1);
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
    // The fixed netplay renderer must also work when offline widescreen is off.
    constexpr const char* net="v82.netplay-view";
    psx_mod_register_activation_plugin(net,[]{psx_mod_set_main_ram_8mb(1);psx_mod_set_world_scene_predicate(world);});
    psx_mod_register_vblank_plugin(net,[]{if(!enabled && psx_netplay_active())vblank();});
    for(const auto& s:instructions)psx_mod_register_instruction_plugin(net,s.pc,s.word,[](CPUState* c,uint32_t pc){if(!enabled && psx_netplay_active())instruction(c,pc);});
    for(const auto& s:functions)psx_mod_register_function_entry_plugin(net,s.pc,[](CPUState* c,uint32_t pc){if(!enabled && psx_netplay_active())entry(c,pc);});
}
}
