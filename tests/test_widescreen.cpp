#include "enhancement_runtime.h"
#include "../src/mods/v82_widescreen_plugin.cpp"
static std::vector<std::pair<uint32_t,int>> hud_tags;
static std::vector<uint32_t> sky_tags;
extern "C" {
void psx_dispatch_call(CPUState*,uint32_t,uint32_t){throw std::runtime_error("unexpected guest call in widescreen unit test");}
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
    enabled=true;
    {
        // Mode 0/1 native HUD layout: radar x16, two-slot backing x80-164,
        // target car x76 and weapon x122. Additional ammo/ability glyph states
        // exercise the same producer interval, including changing bar lengths.
        // The actual entry hooks choose the group anchor; display X includes
        // the native-wide origin and anchor*live-margin, as in the renderer.
        struct R{uint8_t op;int x,y,w,h;};
        const uint32_t slot=0x80020000,packet=0x80020100,tail=0x80021000;
        auto emit=[&](int mode,int green,int digits){
            std::vector<R> rects={
                {0x64,16,16,64,55},{0x60,42,40,2,2}, // radar and enemy blip
                {0x64,80,20,84,34},{0x64,76,22,40,16}, // backing and target car
                {0x60,80,40,green,2},{0x60,80+green,40,32-green,2},
                {0x64,122,25,12,12},{0x64,154,25,8,12}, // weapon and ability
            };
            for(int digit=0;digit<digits;++digit)
                rects.push_back({0x64,138+8*digit,42,8,12});
            uint32_t words=0;for(const auto& r:rects)words+=(r.op&4)?4:3;
            hud_tags.clear();wr(slot,tail&0xFFFFFF);
            c.gpr[5]=mode;c.gpr[6]=slot;entry(&c,0x80037A00);
            wr(slot,packet&0xFFFFFF);wr(packet,(words<<24)|(tail&0xFFFFFF));
            // This previously linked packet is deliberately not part of HUD.
            wr(tail,0x04FFFFFF);wr(tail+4,0x64000000);wr(tail+8,260);
            wr(tail+12,0);wr(tail+16,12|(12<<16));
            uint32_t p=packet+4;std::vector<uint32_t> cmds;
            for(const auto& r:rects){
                cmds.push_back(p);wr(p,uint32_t(r.op)<<24);
                wr(p+4,uint32_t(r.x)|(uint32_t(r.y)<<16));p+=8;
                if(r.op&4){wr(p,0);p+=4;}
                wr(p,uint32_t(r.w)|(uint32_t(r.h)<<16));p+=4;
            }
            std::vector<uint32_t> original;
            for(uint32_t a=packet;a<p;a+=4)original.push_back(rd(a));
            std::array<uint32_t,5> original_tail;
            for(unsigned i=0;i<original_tail.size();++i)original_tail[i]=rd(tail+4*i);
            entry(&c,0x8003827C);
            check(hud_tags.size()==rects.size(),"complete HUD interval tagged without its old OT tail");
            for(unsigned i=0;i<rects.size();++i)
                check(hud_tags[i].first==cmds[i]-4,"HUD tags preserve native emission order");
            for(unsigned i=0;i<original.size();++i)
                check(rd(packet+4*i)==original[i],"HUD anchoring never edits guest packet words");
            for(unsigned i=0;i<original_tail.size();++i)
                check(rd(tail+4*i)==original_tail[i],"existing non-HUD OT tail stays native");
            check(rd(slot)==(packet&0xFFFFFF),"OT links stay native");
            check(c.gpr[5]==unsigned(mode) && c.gpr[6]==slot,"HUD call arguments stay native");
            return rects;
        };
        const int saved_live_margin=live_margin;
        for(int mode:{0,1})for(int green:{1,16,31})for(int digits:{1,2,3})for(int m:{0,53,120,267}){
            live_margin=m;
            entry(&c,0x8001C134);
            const auto rects=emit(mode,green,digits);
            const int radar=rects[0].x+m+hud_tags[0].second*m;
            for(unsigned i=0;i<rects.size();++i){
                const int x=rects[i].x+m+hud_tags[i].second*m;
                check(x-radar==rects[i].x-rects[0].x,
                      "radar, backing, target, both health colours, weapon/ammo/ability keep authored spacing");
                check(x==rects[i].x,"complete HUD stays at native left offsets at every wide aspect, including 4:3");
            }
        }
        live_margin=saved_live_margin;
        // Horizontal-split/other layouts and a netplay full-width camera keep
        // their existing container policy; only the offline 0/1 group changes.
        for(int mode:{2,3}){
            emit(mode,16,2);
            check(hud_tags[0].second==-1 && hud_tags[2].second==0,
                  "other native HUD layouts retain their existing anchors");
        }
        netplay_active=1;
        for(int mode:{0,1}){
            emit(mode,16,2);
            check(hud_tags[0].second==-1 && hud_tags[2].second==0,
                  "offline group override does not change netplay cameras");
        }
        netplay_active=0;
        for(int i=0;i<13;++i)vblank();
        emit(0,16,2);
        check(hud_tags[2].second==0,"expired world scene cannot force a menu HUD left");
        entry(&c,0x8001C134);
        // The existing display and instruction guards still fail closed.
        auto rejected_interval=[&](uint32_t next_slot){
            hud_tags.clear();wr(slot,tail&0xFFFFFF);c.gpr[5]=0;c.gpr[6]=slot;
            entry(&c,0x80037A00);
            wr(slot,packet&0xFFFFFF); // Valid emitted HUD makes every guard check nonvacuous.
            c.gpr[6]=next_slot;entry(&c,0x8003827C);
        };
        display_width=640;rejected_interval(slot);
        check(hud_tags.empty(),"full-screen high-resolution menus are not retagged");display_width=320;
        const auto producer_word=rd(0x80037A00);
        wr(0x80037A00,0);rejected_interval(slot);
        check(hud_tags.empty(),"unexpected HUD producer instruction is not trusted");wr(0x80037A00,producer_word);
        rejected_interval(slot+4);
        check(hud_tags.empty(),"a different next OT slot cannot consume the saved HUD interval");
        c.gpr[6]=slot;
    }
    // Every cell belongs to exactly one bounded native batch. Adjacent batches
    // share their edge vertices, with no transparent-water overlap or holes.
    const auto saved_sp=c.gpr[29];c.gpr[29]=0x80030000;
    for(int dx:{16,31,32,41,48,64,93})for(int dz:{12,31,32,47,63}){
        water_grid={};water_grid.active=true;
        c.gpr[14]=850;wr(c.gpr[29]+0xA0,1250);
        wr(c.gpr[29]+0x68,850+dx);wr(c.gpr[29]+0x6C,1250+dz);
        std::vector<unsigned> seen(dx*dz);
        for(;water_grid.tile<water_grid.count;++water_grid.tile){
            plan_water_grid(&c);
            const int x=int(c.gpr[14])-850,z=int(rd(c.gpr[29]+0xA0))-1250;
            const int xe=int(rd(c.gpr[29]+0x68))-850,ze=int(rd(c.gpr[29]+0x6C))-1250;
            check(x>=0 && z>=0 && xe<=dx && ze<=dz && xe>x && ze>z,"water batch stays inside requested grid");
            check(xe-x<=31 && ze-z<=31 && ze-z<=xe-x,"water batch fits native buffers and row setup");
            for(int zz=z;zz<ze;++zz)for(int xx=x;xx<xe;++xx)++seen[zz*dx+xx];
        }
        for(auto count:seen)check(count==1,"water grid has no omitted or duplicated cells");
    }
    water_grid={};c.gpr[29]=saved_sp;
    // Both water layers cover the same reveal interval without moving the
    // native camera centre or changing Y/depth/colour. Exercise flat and rolled
    // strip shapes, and the horizontal outcodes used by textured water.
    for(int width:{320,160})for(int m:{53,120,267}){
        wr(GP+0xEDC,width);netplay_active=width==160;netplay_wide_width=width+2*m;
        cull_margin=m;const auto p=Pool+0x400;
        c.gpr[13]=width;c.gpr[8]=p;instruction(&c,0x80015FB0);
        check(c.gpr[13]==uint32_t(width+2*m) && rd(GP+0xEDC)==uint32_t(width),"water widens its local viewport only");
        for(unsigned j=0;j<2;++j){
            const auto a=p+j*28;wr(a,0x06000000|(j?0xFFFFFF:((p+28)&0xFFFFFF)));
            wr(a+4,0xE1000600);wr(a+8,0x2A405060);
            for(unsigned i=0;i<4;++i){const int x=j?int(i)*20-40:(i&1?width+2*m:0);wr(a+12+4*i,uint16_t(x)|((70+i)<<16));}
        }
        wr(p+56,0x12345678);c.gpr[8]=p+56;instruction(&c,0x8001665C);
        check(int16_t(rd(p+12))==-m && int16_t(rd(p+16))==width+m,"water base reaches both reveal edges");
        check(int16_t(rd(p+40))==-40-m,"rolled water uses the same origin shift");
        check(rd(p+12)>>16==70 && rd(p+8)==0x2A405060 && rd(p+56)==0x12345678,"water changes only its four X fields");
        c.gpr[23]=width;instruction(&c,0x800166BC);check(c.gpr[23]==uint32_t(width+2*m),"wave traversal matches base coverage");
        for(int x:{-m-1,-m,width+m,width+m+1}){
            c.gpr[5]=uint32_t(x);instruction(&c,0x80016FE8);
            check((int32_t(c.gpr[5])<0)==(x<-m),"wave left outcode includes reveal boundary");
            c.gpr[2]=0;instruction(&c,0x80016FF4);
            check((c.gpr[2]!=0)==(x<=width+m),"wave right outcode uses actual local viewport");
        }
        c.gpr[13]=width;c.gpr[8]=p;instruction(&c,0x80015FB0);
        wr(p+28+8,0);const auto before=rd(p+12);c.gpr[8]=p+56;instruction(&c,0x8001665C);
        check(rd(p+12)==before,"malformed water interval is rejected before any coordinate edits");
    }
    netplay_active=0;netplay_wide_width=0;wr(GP+0xEDC,320);
    enabled=true;display_width=640;c.gpr[2]=0x80074A68;c.gpr[3]=0x80094A68;
    instruction(&c,0x80014BBC);instruction(&c,0x80014BD8);
    check(c.gpr[2]==0x80074A68&&c.gpr[3]==0x80094A68,"menus retain native packet arenas");
}
