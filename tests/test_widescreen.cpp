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
    // Exercise the actual instruction callbacks at every subdivision seam.
    // These cover large near faces, and the smaller triangles they subdivide
    // into; all three vertices can be outside 4:3 while visible in the reveal.
    entry(&c,0x8001C134);
    for(int m:{53,120,267}){
        cull_margin=m;
        for(auto right:subdivision_right_sites){
            struct Case{int x[3];bool right,inside;};
            const Case cases[]={
                {{-m,-m,-m},true,true}, {{-m-1,-m-1,-m-1},true,false},
                {{-m-20,-10,-5},true,true}, {{320,321,322},true,true},
                {{320+m,321+m,322+m},false,false}, {{700,400,319+m},true,true},
                {{-900,10,900},true,true}, {{-1200,-1100,-1000},true,false},
            };
            for(const auto& sample:cases){
                const auto& xs=sample.x;
                c.gpr[2]=320;c.gpr[1]=0;
                for(unsigned i=0;i<3;++i)c.gpr[8+i]=uint32_t(xs[i]);
                instruction(&c,right);
                // Execute the native branch chain, including its unmodified
                // second and third comparisons if the first branch falls through.
                const bool right_pass=c.gpr[1]!=0 || int32_t(c.gpr[9])<320 || int32_t(c.gpr[10])<320;
                check(right_pass==sample.right,"subdivision right bound includes reveal and rejects beyond it");
                bool inside=false;
                if(right_pass){
                    instruction(&c,right+24);
                    inside=int32_t(c.gpr[8])>=0 || int32_t(c.gpr[9])>=0 || int32_t(c.gpr[10])>=0;
                    check(int32_t(c.gpr[8]-c.gpr[9])==xs[0]-xs[1] &&
                          int32_t(c.gpr[9]-c.gpr[10])==xs[1]-xs[2],
                          "subdivision edge lengths remain unchanged");
                }
                check(inside==sample.inside,"native branch chains retain exactly the widened horizontal interval");
                check(c.gpr[2]==320,"native width remains unchanged");
            }
            c.gpr[2]=160;c.gpr[1]=123;c.gpr[8]=uint32_t(-25);
            instruction(&c,right);instruction(&c,right+24);
            check(c.gpr[1]==123 && int32_t(c.gpr[8])==-25,"other subdivision viewport widths remain native");
        }
    }
    cull_margin=267;
    c.gpr[2]=320;c.gpr[1]=123;c.gpr[8]=uint32_t(-25);
    for(int i=0;i<13;++i)vblank();
    instruction(&c,subdivision_right_sites[0]);instruction(&c,subdivision_right_sites[0]+24);
    check(c.gpr[1]==123 && int32_t(c.gpr[8])==-25,"subdivision correction requires a current world scene");
    entry(&c,0x8001C134);
    // Identity camera: an ultrawide right ray must stay right of the camera.
    c.gte_ctrl[0]=4096;c.gte_ctrl[2]=4096;c.gte_ctrl[4]=4096;
    c.gte_ctrl[5]=100;c.gte_ctrl[6]=200;c.gte_ctrl[7]=300;
    c.gpr[20]=34160;c.gpr[19]=9600;c.gpr[5]=0x1F8002F0;wr(GP+0xDB4,80);
    for(unsigned corner=0;corner<4;++corner){
        c.gpr[17]=corner+1;instruction(&c,0x8001C304);
        check(int32_t(rd(c.gpr[5]))==100+((corner&1)?34160:-34160),"wide ray does not wrap signed halfword");
        check(int32_t(rd(c.gpr[5]+4))==200+((corner&2)?9600:-9600),"corner height preserved");
        check(rd(c.gpr[5]+8)==20780,"far reach preserved");
    }
    c.gpr[20]=17040;wr(c.gpr[5],123);instruction(&c,0x8001C304);
    check(rd(c.gpr[5])==123,"representable native rays unchanged");
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
    // Both native cameras retain their 160px projection. The enhanced
    // compositor's fixed target determines the matching culling window.
    netplay_active=1;wr(GP+0xEDC,160);entry(&c,0x8001C134);
    for(int width:{426,560}){
        netplay_wide_width=width;
        // Retail vertical-split HUD modes use a compact group spanning the
        // full 160px half. Its weapon sprite and ammo text straddle the old
        // 320px left/center cutoff, but must keep their authored spacing.
        for(int mode:{4,5}){
            const uint32_t slot=0x80020000,weapon=0x80020100,ammo=0x80020200;
            for(int digits:{1,2,3}){
                hud_tags.clear();wr(slot,0xFFFFFF);
                c.gpr[5]=mode;c.gpr[6]=slot;entry(&c,0x80037A00);
                wr(slot,weapon&0xFFFFFF);wr(weapon,0x04000000|(ammo&0xFFFFFF));
                wr(weapon+4,0x64000000);wr(weapon+8,mode==4?98:90);
                wr(weapon+12,0);wr(weapon+16,12|(12<<16));
                for(int digit=0;digit<digits;++digit){
                    const auto p=ammo+32*digit;
                    wr(p,0x04000000|(digit+1==digits?0xFFFFFF:((p+32)&0xFFFFFF)));
                    wr(p+4,0x64000000);wr(p+8,(mode==4?113:105)+8*digit);
                    wr(p+12,0);wr(p+16,8|(12<<16));
                }
                entry(&c,0x8003827C);
                check(hud_tags.size()==unsigned(1+digits),"every weapon and ammo packet is tagged");
                const int offset=(width-160)/2;
                const int weapon_x=(mode==4?98:90)+offset+hud_tags[0].second*offset;
                for(int digit=0;digit<digits;++digit){
                    const int ammo_x=(mode==4?113:105)+8*digit+offset+hud_tags[1+digit].second*offset;
                    check(ammo_x-weapon_x==15+8*digit,"weapon and ammo retain native spacing in both enhanced views");
                }
                check(hud_tags[0].second==-1,"compact HUD remains attached to the local view's left edge");
            }
        }
        c.gpr[2]=160;instruction(&c,0x8001C250);check(c.gpr[2]==uint32_t(width),"netplay terrain matches enhanced target with offline widescreen disabled");
        c.gpr[3]=160;instruction(&c,0x8002E024);check(c.gpr[3]==uint32_t(width),"netplay object frustum matches terrain");
        c.gpr[2]=160;c.gpr[8]=200;c.gpr[9]=201;c.gpr[10]=202;c.gpr[1]=0;
        instruction(&c,subdivision_right_sites[0]);check(c.gpr[1]==1,"netplay reveal triangles survive subdivision");
        c.gpr[2]=320;instruction(&c,0x8001C250);check(c.gpr[2]==320,"full-screen widths are not widened inside a split camera");
    }
    netplay_wide_width=0;c.gpr[2]=160;instruction(&c,0x8001C250);check(c.gpr[2]==160,"non-wide split has native culling");
    netplay_active=0;wr(GP+0xEDC,320);
    enabled=true;hud_tags.clear();wr(0x80020000,0xFFFFFF);
    c.gpr[5]=0;c.gpr[6]=0x80020000;entry(&c,0x80037A00);
    wr(0x80020000,0x020100);wr(0x80020100,0x04FFFFFF);
    wr(0x80020104,0x64000000);wr(0x80020108,288);
    wr(0x8002010C,0);wr(0x80020110,12|(12<<16));
    entry(&c,0x8003827C);
    check(hud_tags.size()==1&&hud_tags[0].second==1,"single-player right HUD retains its native edge after a split HUD");
    enabled=true;display_width=640;c.gpr[2]=0x80074A68;c.gpr[3]=0x80094A68;
    instruction(&c,0x80014BBC);instruction(&c,0x80014BD8);
    check(c.gpr[2]==0x80074A68&&c.gpr[3]==0x80094A68,"menus retain native packet arenas");
}
