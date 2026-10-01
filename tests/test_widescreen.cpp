#include "enhancement_runtime.h"
#include "../src/mods/v82_widescreen_plugin.cpp"
static std::vector<std::pair<uint32_t,int>> hud_tags;
static std::vector<uint32_t> sky_tags;
extern "C" {
void gpu_ws_tag_hud_prim(uint32_t p,int edge){hud_tags.emplace_back(p,edge);}
void gpu_ws_tag_background_prim(uint32_t p){sky_tags.push_back(p);}
}
int main(){
    activate();check(aspect_n==16&&adaptive_n==32,"Fit is capped at 32:9");
    for(auto s:functions)wr(s.pc,s.word);
    wr(GP+0xEDC,320);
    CPUState c{};c.gpr[28]=GP;c.gpr[31]=0x80010000;
    check(!world_callback(),"menus start in 4:3");entry(&c,0x8001C134);
    check(world_callback(),"terrain identifies world");for(int i=0;i<13;++i)vblank();
    check(!world_callback(),"leaving gameplay releases wide classification");
    for(int reveal:{0,54,120,267}){
        live_margin=reveal;c.gpr[2]=320;instruction(&c,0x8001C250);
        check(c.gpr[2]==854,"Fit guest frustum is independent of local window");
        c.gpr[3]=320;instruction(&c,0x8002E024);check(c.gpr[3]==854,"objects match terrain frustum");
    }
    c.gpr[8]=uint32_t(-200);c.gpr[9]=uint32_t(-300);c.gpr[10]=uint32_t(-280);c.gpr[11]=123;
    instruction(&c,0x80029320);
    check(int32_t(c.gpr[8])>0&&int32_t(c.gpr[9])<0&&c.gpr[11]==123,"left reveal preserves pending fourth load");
    c.gpr[11]=uint32_t(-230);instruction(&c,0x80029338);check(c.gpr[11]==37,"fourth vertex after load retirement");
    c.gpr[7]=0x80010000;const unsigned offsets[]={8,0x28,0xA8,0xC8};
    for(auto offset:offsets)wr(c.gpr[7]+offset,500);c.gpr[1]=320;
    instruction(&c,0x80029344);check(c.gpr[1]==854&&c.gpr[8]==767,"right reveal admits geometry beyond native edge");
    c.gpr[4]=4;c.gpr[5]=2040;c.gpr[6]=200;entry(&c,0x8001BE68);
    check(c.gpr[4]==0&&c.gpr[5]==2044,"row padding keeps every cell corner inside grid");
    wr(GP+0x20,0);wr(GP+0x610,0x80074A68+64);entry(&c,0x8001C134);
    check(rd(GP+0x610)==Pool+64,"first world draw transitions from native arena");
    for(unsigned parity=0;parity<2;++parity){
        wr(GP+0x20,parity);instruction(&c,0x80014BBC);check(c.gpr[2]==Pool+parity*PoolBytes,"packet arena parity");
        instruction(&c,0x80014BD8);check(c.gpr[3]==Pool+(parity+1)*PoolBytes,"packet arena limit");
        c.gpr[3]=Pool+parity*PoolBytes+1234;instruction(&c,0x80014B60);
        check(c.gpr[3]==0x80074A68+(parity<<17)+1234,"high-water accounting remains native");
        c.pc=0;c.gpr[4]=0;c.gpr[5]=0;c.gpr[6]=Pool+(parity+1)*PoolBytes-4096;
        entry(&c,0x800288E0);check(c.pc==c.gpr[31]&&c.gpr[2]==c.gpr[6],"full arena rejects before emission");
    }
    // Native four-word monochrome triangle, followed by an existing OT tail.
    wr(0x80020000,0x020100);wr(0x80020100,0x04FFFFFF);wr(0x80020104,0x20000000);
    wr(0x80020108,10);wr(0x8002010C,20);wr(0x80020110,30);
    tag_chain(0x80020000,0xFFFFFF,false);
    check(hud_tags.size()==1&&hud_tags[0].second==-1,"HUD chain anchors left");
    tag_chain(0x80020000,0xFFFFFF,true);check(sky_tags.size()==1,"sky provenance tags polygon");
    wr(0x80020100,0x04FFFFFF);wr(0x80020104,0xA0000000);
    tag_chain(0x80020000,0xFFFFFF,true);check(sky_tags.size()==1,"malformed packet never tagged");
    enabled=false;c.gpr[2]=320;instruction(&c,0x8001C250);check(c.gpr[2]==320,"disabled renderer is inert");
    enabled=true;display_width=640;c.gpr[2]=0x80074A68;c.gpr[3]=0x80094A68;
    instruction(&c,0x80014BBC);instruction(&c,0x80014BD8);
    check(c.gpr[2]==0x80074A68&&c.gpr[3]==0x80094A68,"menus retain native packet arenas");
}
