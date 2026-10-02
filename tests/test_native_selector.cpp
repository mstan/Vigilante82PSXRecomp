#include "mod_test_runtime.h"
static void check(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
int main() {
    enabled=true;package.vehicles.resize(13);init();CPUState c{};
    c.gpr[5]=1;c.gpr[28]=0x8006A7F0;selector::begin(&c);
    c.gpr[30]=0;selector::slot(&c);
    // Native wrap from first retail to last enters the final original entry.
    wr(0x8006B508,0x80000000);c.gpr[30]=17;selector::navigate(&c);
    check(selector::guest()==12&&c.gpr[30]==0,"left wrap did not enter original roster");
    selector::navigate(&c);check(selector::guest()==12,"held direction skipped an entry");
    for(int expected=11;expected>=0;--expected) {
        selector::set(selector::Latch,0);c.gpr[30]=17;selector::navigate(&c);
        check(selector::guest()==expected&&c.gpr[30]==0,"original carousel identity changed");
    }
    selector::set(selector::Latch,0);selector::navigate(&c);
    check(selector::guest()==-1&&c.gpr[30]==17,"left exit did not restore last retail");
    wr(0x8006B508,0x20000000);c.gpr[30]=0;selector::navigate(&c);
    check(selector::guest()==0,"right wrap did not enter first original");
    selector::set(selector::Guest,7);selector::set(selector::Accepted,7);
    selector::set(selector::AcceptedProxy,0);selector::set(selector::Enemy,1);
    selector::set(selector::Guest,-1);c.gpr[2]=0;selector::end(&c);
    check(c.gpr[2]==71&&psx_mod_read_byte(c.gpr[28]+0x1104)==71,"enemy phase lost accepted original player");
    c.gpr[5]=2;selector::begin(&c);selector::set(selector::Guest,3);
    c.gpr[2]=0;psx_mod_write_byte(c.gpr[28]+0x1105,0xFF);selector::end(&c);
    check(c.gpr[2]==67&&psx_mod_read_byte(c.gpr[28]+0x1104)==67&&
          psx_mod_read_byte(c.gpr[28]+0x1105)==0xFF,"Survival context was mistaken for player two");
    selector::begin(&c);selector::set(selector::Enemy,1);
    for(unsigned row=0;row<4;++row)psx_mod_write_byte(0x8006B8F6+row,uint8_t(row));
    c.gpr[19]=0;selector::enemy_slot(&c);
    wr(0x8006B508,0x80000000);psx_mod_write_byte(0x8006B8F6,17);selector::enemy_slot(&c);
    check(selector::guest()==12,"NPC wrap did not enter original roster");
    c.gpr[19]=1;selector::enemy_slot(&c);psx_mod_write_byte(0x8006B8F7,0);
    wr(0x8006B508,0x20000000);selector::enemy_slot(&c);
    check(selector::guest()==0,"second NPC did not keep independent identity");
    selector::apply_npcs(&c);
    check(psx_mod_read_byte(0x8006B8F6)==76&&psx_mod_read_byte(0x8006B8F7)==64,"NPC identities not applied to match");
    const auto proxy=uint8_t(rd(selector::npc(0)+8));
    psx_mod_write_byte(0x8006B8F6,proxy);psx_mod_write_byte(0x8006B8F7,proxy);
    psx_mod_write_byte(0x8006B8F8,0xFF);psx_mod_write_byte(0x8006B8F9,proxy);
    selector::apply_npcs(&c);
    check(psx_mod_read_byte(0x8006B8F6)==76&&psx_mod_read_byte(0x8006B8F7)==76&&
          psx_mod_read_byte(0x8006B8F8)==0xFF&&psx_mod_read_byte(0x8006B8F9)==76,
          "NPC quantity expansion lost its proxy identity or filled an empty slot");
    psx_mod_write_byte(c.gpr[28]+0x31,9);psx_mod_write_byte(0x8006B8F6,proxy);
    selector::apply_npcs(&c);
    check(psx_mod_read_byte(0x8006B8F6)==proxy,"four-player mode overwrote a local player");
    psx_mod_write_byte(c.gpr[28]+0x31,0);
    // The simultaneous two-player selector has independent native slots and
    // input words. Navigation and confirmation must not inherit P1 state.
    const uint32_t stack=0x801F0000;c.gpr[29]=stack-24;
    selector::two_begin();c.gpr[23]=0;
    wr(stack+0xE4,stack+0x58);wr(stack+0xE8,stack+0x60);
    wr(stack+0x60,0);wr(stack+0x64,0);wr(stack+0x58,17);
    wr(0x8006B508,0x80000000);wr(0x8006B50C,0);
    selector::two_navigate(&c);
    check(selector::two_guest(0)==12&&selector::two_guest(1)==-1&&rd(stack+0x58)==0,
          "P1 wrap changed P2's independent carousel");
    c.gpr[23]=1;wr(stack+0xE4,stack+0x5C);wr(stack+0xE8,stack+0x64);
    wr(stack+0x5C,17);wr(0x8006B508,0);wr(0x8006B50C,0x80000000);
    selector::two_navigate(&c);
    check(selector::two_guest(0)==12&&selector::two_guest(1)==12,
          "P2 selector ignored P2 input");
    wr(0x8006B50C,0);selector::two_navigate(&c);
    wr(0x8006B50C,0x80000000);wr(stack+0x5C,0);selector::two_navigate(&c);
    check(selector::two_guest(1)==11&&selector::two_identity(0,0)!=selector::two_identity(1,0),
          "distinct originals compared as the same retail proxy");
    selector::two_commit(&c);
    check(psx_mod_read_byte(c.gpr[28]+0x1104)==76&&psx_mod_read_byte(c.gpr[28]+0x1105)==75,
          "two-player confirmation lost an imported identity");
    selector::two_end(&c);
    check(rd(stack+0x58)==76&&rd(stack+0x5C)==75&&!rd(selector::TwoActive),
          "packed native return lost independent players");
    // The native preview cleanup may have already freed its editable group.
    package.banks.resize(1);const auto runtime=allocate(32),bin=allocate(32),group=allocate(32);
    wr(runtime+16,group);wr(bin+4,bin+16);wr(bank_state(0),runtime);wr(bank_state(0)+4,bin);
    release(group);
    test_dispatch=[](CPUState*,uint32_t fn,uint32_t) {check(fn==0x8001FC3C,"unexpected preview teardown call");};
    selector::release_bank(&c,0);
    check(!owned(runtime)&&!owned(bin),"preview cleanup leaked its owned buffers");
    // The reference zoom applies only to the large player preview. Applying
    // it to an NPC thumbnail hides the flying vehicle outside its tiny view.
    selector::set(selector::Base,0x80700000);selector::set(selector::Active,1);selector::set(selector::Guest,12);
    test_dispatch=[](CPUState* cpu,uint32_t fn,uint32_t) {
        check(fn==0x8070536C,"wrong relocated preview scale function");
        check(cpu->gpr[5]==0x80,"NPC thumbnail inherited player zoom");
    };
    c.gpr[31]=0x80705798;c.gpr[5]=0x80;selector::callback(&c,selector::Host+21*4);
    c.gpr[5]=0;c.gpr[4]=0x8000;selector::begin(&c);c.gpr[30]=0;selector::slot(&c);
    c.gpr[30]=17;wr(0x8006B508,0x80000000);selector::navigate(&c);
    check(selector::guest()<0&&c.gpr[4]==0x8000,"unimplemented Quest campaign exposed a guest identity");
    std::cout<<"native roster wrap, hold, player/NPC identities and preview cleanup passed\n";
}
