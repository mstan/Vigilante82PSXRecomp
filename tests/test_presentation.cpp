#include "mod_test_runtime.h"
static void check(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
static std::string text_at(uint32_t p) {
    std::string s;for(unsigned i=0;i<64;++i){auto ch=psx_mod_read_byte(p+i);if(!ch)return s;s+=char(ch);}
    throw std::runtime_error("unterminated text");
}
static uint32_t transfer,bytes_written;static std::vector<uint32_t> voice_calls;
int main() {
    enabled=true;package.vehicles.resize(13);presentation::ready=true;
    for(unsigned i=0;i<13;++i)package.vehicles[i].name="Original "+std::to_string(i);
    CPUState c{};c.gpr[28]=0x8006A7F0;c.gpr[29]=0x801FFF00;c.gpr[31]=0x80010000;
    for(unsigned i=0;i<13;++i) {
        c.gpr[4]=0x80090000;psx_mod_write_byte(c.gpr[4]+0xDC,uint8_t(64+i));
        for(int id:{-1,3}) {
            psx_mod_write_half(c.gpr[4]+10,uint16_t(id));
            check(presentation::object_name(&c)&&text_at(c.gpr[2])==package.vehicles[i].name,
                  "player or NPC destruction name used retail/empty text");
        }
    }
    psx_mod_write_byte(c.gpr[4]+0xDC,17);c.gpr[2]=0x1234;
    check(!presentation::object_name(&c)&&c.gpr[2]==0x1234,"retail name was overridden");
    for(unsigned i=0;i<15;++i)presentation::images[i]=0x80350000+i*64;
    for(unsigned i=0;i<13;++i) {
        presentation::portrait(&c,i,false);
        check(c.gpr[4]==presentation::images[i==12?12:i<6?13:14],"wrong original faction portrait layer");
        if(i<12){presentation::portrait(&c,i,true);check(c.gpr[4]==presentation::images[i],"wrong driver pose layer");}
    }
    std::vector<uint8_t> voice(56+13*16);
    auto put16=[&](unsigned p,unsigned v){voice[p]=uint8_t(v);voice[p+1]=uint8_t(v>>8);};
    put16(0,13);put16(2,26);
    for(unsigned i=0;i<13;++i) {put16(4+i*4,i*2);put16(6+i*4,0x900+i);std::fill_n(voice.begin()+56+i*16,16,uint8_t(i));}
    presentation::sections[16]={voice.data(),uint32_t(voice.size())};
    const uint32_t shell=0x80700000,bank=0x80091000;
    wr(shell+0x16738,bank);psx_mod_write_half(bank,16);psx_mod_write_half(bank+60,0x100);psx_mod_write_half(bank+64,0x104);
    test_spu_read=[](uint32_t p){check(p==0x1F801DA6,"unexpected SPU read");return transfer;};
    test_spu_write=[](uint32_t p,uint32_t v){check(p==0x1F801DA6,"unexpected SPU write");transfer=v;};
    test_spu_dma=[](uint32_t){bytes_written+=4;};
    test_dispatch=[](CPUState* cpu,uint32_t fn,uint32_t){
        voice_calls.push_back(fn);check(cpu->gpr[4]==1||cpu->gpr[4]==2,"wrong speech channel");
        if(fn==0x8001E188)check(cpu->gpr[5]==0x80091000&&cpu->gpr[6]==13+cpu->gpr[4],"selection borrowed retail sample");
    };
    for(unsigned i=0;i<13;++i)for(unsigned channel=1;channel<=2;++channel) {
        c.gpr[4]=channel;transfer=0x7777;bytes_written=0;voice_calls.clear();
        presentation::voice(&c,i,shell);
        check(bytes_written==16&&transfer==0x7777&&voice_calls==std::vector<uint32_t>({0x8001DE78,0x8001E188}),"speech slot transfer or native stop/play lifecycle incorrect");
        check(psx_mod_read_half(bank+4+(13+channel)*4+2)==0x900+i,"authored voice pitch lost");
    }
    std::vector<uint8_t> xa(216+26*8*2336);
    auto put=[&](unsigned p,unsigned v){for(unsigned j=0;j<4;++j)xa[p+j]=uint8_t(v>>(8*j));};
    put(0,0x41583856);put(4,26);
    for(unsigned i=0;i<26;++i){put(8+i*8,i*8);put(12+i*8,8);}
    presentation::load_outcomes({xa.data(),uint32_t(xa.size())});presentation::outcome_lba=300000;
    for(unsigned i=0;i<13;++i)for(unsigned winner=0;winner<2;++winner) {
        c.gpr[18]=0;c.gpr[31]=0x80013268;c.gpr[4]=0x80092000;
        psx_mod_write_byte(c.gpr[28]+0x1104,64+i);psx_mod_write_byte(c.gpr[28]+0x33,winner);
        check(presentation::result_entry(&c,0x8005FA9C),"original result path not replaced");
        const auto text=text_at(c.gpr[4]);check(text[15]==(winner?'V':'D'),"result outcome bit reversed");
        check(presentation::result_entry(&c,0x80018210),"result file not resolved");
        check(rd(c.gpr[2]+12)==300000+(winner*13+i)*8&&rd(c.gpr[2]+16)==8*2048,"wrong driver's XA extent");
        c.gpr[31]=0x80013294;c.gpr[5]=7;
        check(!presentation::result_entry(&c,0x8001DA14)&&c.gpr[5]==0&&!rd(presentation::State+4),"native XA player did not consume channel override");
        c.gpr[5]=7;presentation::result_entry(&c,0x8001DA14);check(c.gpr[5]==7,"channel override leaked to later playback");
    }
    put(8+8,0);bool rejected=false;
    try{presentation::load_outcomes({xa.data(),uint32_t(xa.size())});}catch(...){rejected=true;}
    check(rejected,"overlapping outcome records accepted");
    std::cout<<"13 original identities, two speech slots, 26 outcome mappings and retail fallthrough passed\n";
}
