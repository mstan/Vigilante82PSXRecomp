#include "enhancement_runtime.h"
#include "../src/mods/v82_music_plugin.cpp"
static std::vector<uint8_t> audio(104+12*300*2352);
static uint32_t next_lba=150;
extern "C" {
int psx_mod_current_resource_bytes(const char*,const uint8_t** p,uint64_t* n){*p=audio.data();*n=audio.size();return 1;}
uint32_t psx_mod_alloc_guest_memory(uint32_t size,uint32_t){check(size==1280,"TOC allocation");return 0x80200000;}
int psx_mod_append_cdda_tracks(const PSXModCDDATrack* tracks,uint32_t count,uint32_t* lbas,uint32_t* counts){
    for(unsigned i=0;i<count;++i){
        const auto& t=tracks[i];
        if(t.resource_id){check(t.byte_offset+t.sector_count*2352<=audio.size(),"audio bounds");counts[i]=t.sector_count;}
        else{check(t.disc_track>=2&&t.disc_track<=17,"mounted audio track");counts[i]=300;}
        lbas[i]=next_lba;next_lba+=counts[i];
    }
    return 1;
}
}
int main(){
    auto put=[](unsigned at,uint32_t value){for(unsigned j=0;j<4;++j)audio[at+j]=uint8_t(value>>(8*j));};
    put(0,0x44433856);put(4,12);for(unsigned i=0;i<12;++i){put(8+i*8,300*i);put(12+i*8,300);}
    for(const char* album:{"Both","Original","Sequel"}){
        option=album;next_lba=150;activate();check(enabled,"valid soundtrack enables");
        const unsigned expected=option=="Both"?28:option=="Original"?12:16;
        check(song_count==expected,"album has correct track count");
        CPUState c{};c.gpr[28]=GP;c.gpr[31]=0x80010000;psx_mod_write_word(0x8001D580,0x27BDFFF8);
        instruction(&c,0x8001D600);check(psx_mod_read_byte(GP+0x839)==expected,"native refresh publishes full track count");
        instruction(&c,0x8001D594);check(c.gpr[16]-0x2200==storage,"native CdGetToc writes relocated bounded buffer");
        check(track_start[expected+2]+150==track_start[0] && psx_mod_read_word(storage+4*(expected+2)),"rotation has native two-second lead-out headroom");
        if(original_first>=0){
            psx_mod_write_byte(GP+0xC14,uint8_t(original_first));instruction(&c,0x800122E8);
            auto p=psx_mod_read_word(c.gpr[2]);std::string title;
            while(psx_mod_read_byte(p))title+=char(psx_mod_read_byte(p++));
            check(title=="V8: Boogie","native pause selector addresses imported title");
        }
    }
    audio[0]=0;activate();check(!enabled,"malformed media stays disabled");
}
