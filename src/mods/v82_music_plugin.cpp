/* Original soundtrack/menu mapping informed by the authorized V82NativeAudio
 * and SoundtrackDisc references. SoundtrackDisc is in the MIT runtime,
 * copyright (c) 2026 flaffy; V82NativeAudio reuse is by author permission.
 * See tools/third_party/NOTICE.md. Native PSX CD playback is retained. */
#include "mod_plugins.h"
#include "cpu_state.h"
#include <cstdint>
#include <cstring>

namespace {
constexpr const char* Plugin="v82.music";
constexpr const char* Package="v82.content.original-music";
constexpr uint32_t GP=0x8006A7F0;
uint32_t storage=0, track_start[31]{}, song_count=0;
int original_first=-1;
bool enabled=false;
const char* const names[]={"V8: Boogie","V8: Beezwax","V8: Convoy","V8: Dave",
    "V8: Houston","V8: John Torque","V8: Loki","V8: Molo","V8: Sheila",
    "V8: Sid","V8: Slick Clyde","V8: Chassey"};
uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
}
uint8_t bcd(uint32_t n) { return uint8_t((n/10)*16+n%10); }
void refresh() {
    // Relocate the bounded retail TOC instead of allowing 28 songs to overwrite
    // neighboring globals. Index zero holds lead-out, like native CdGetToc.
    for(unsigned i=0;i<=song_count+2;++i) {
        const auto msf=track_start[i]+150;
        psx_mod_write_word(storage+i*4,uint32_t(bcd(msf/4500))|
            (uint32_t(bcd(msf/75%60))<<8)|(uint32_t(bcd(msf%75))<<16));
    }
    for(unsigned i=0;i<song_count;++i) {
        uint32_t text=0;
        if(original_first>=0 && i>=unsigned(original_first) && i<unsigned(original_first)+12) {
            text=storage+256+i*32;
            const auto name=names[i-original_first];
            for(unsigned j=0;j<=std::strlen(name);++j)psx_mod_write_byte(text+j,uint8_t(name[j]));
        } else if(i<15) text=psx_mod_read_word(0x800639B0+i*4);
        else {
            text=storage+256+i*32;
            const char name[]="Second Offense 16";
            for(unsigned j=0;j<sizeof(name);++j)psx_mod_write_byte(text+j,uint8_t(name[j]));
        }
        psx_mod_write_word(storage+128+i*4,text);
    }
    psx_mod_write_byte(GP+0x839,uint8_t(song_count));
    if(psx_mod_read_byte(GP+0x83A)>=song_count)psx_mod_write_byte(GP+0x83A,0);
    psx_mod_counter_add("v82.music.refresh",1);
}
struct Site {uint32_t pc,word;};
constexpr Site sites[]={{0x8001D8C0,0x2463DE00},{0x8001D8F8,0x2463DE00},
    {0x8001D964,0x24A5DE00},{0x800122E8,0x8C450000},
    {0x8001D594,0x2604DE00},{0x8001D5AC,0x2610DE00},{0x8001D600,0x8FBF0014}};
void instruction(CPUState* cpu,uint32_t pc) {
    if(!enabled || cpu->gpr[28]!=GP)return;
    if(pc==0x8001D594 || pc==0x8001D5AC)cpu->gpr[16]=storage+0x2200;
    else if(pc==0x8001D600)refresh();
    else if(pc==0x800122E8) {
        const auto index=psx_mod_read_byte(GP+0xC14);
        cpu->gpr[2]=storage+128+(index<song_count?index:0)*4;
    } else cpu->gpr[pc==0x8001D964?5:3]=storage+0x2200;
    psx_mod_counter_add("v82.music.native_table",1);
}
void activate() {
    enabled=false;song_count=0;original_first=-1;
    const uint8_t* bytes=nullptr;uint64_t size=0;
    if(!psx_mod_current_resource_bytes("music",&bytes,&size) || size<104 ||
       u32(bytes)!=0x44433856 || u32(bytes+4)!=12)return;
    uint64_t end=0;
    for(unsigned i=0;i<12;++i) {
        const auto start=u32(bytes+8+i*8),count=u32(bytes+12+i*8);
        if(start!=end || !count || 104+(end+count)*2352>size)return;
        end+=count;
    }
    if(104+end*2352!=size)return;
    storage=psx_mod_alloc_guest_memory(1280,16);
    if(!storage)return;
    char album[16]="Both";
    psx_mod_option_value(Package,"music","album",album,sizeof(album));
    PSXModCDDATrack tracks[28]{};
    uint32_t counts[28]{};
    if(std::strcmp(album,"Original"))for(unsigned track=2;track<=17;++track) {
        tracks[song_count++].disc_track=track;
    }
    if(std::strcmp(album,"Sequel")) {
        original_first=int(song_count);
        for(unsigned i=0;i<12;++i) {
            const auto count=u32(bytes+12+i*8);
            tracks[song_count++]={"music",104+uint64_t(u32(bytes+8+i*8))*2352,count,0};
        }
    }
    if(!psx_mod_append_cdda_tracks(tracks,song_count,track_start+2,counts))return;
    track_start[0]=track_start[song_count+1]+counts[song_count-1];
    // Retail refresh places a second lead-out entry after the last song,
    // two seconds early. Its whole-second CD report callback needs that
    // headroom to restart rotation before the drive reaches physical lead-out.
    track_start[1]=0;
    track_start[song_count+2]=track_start[0]-150;
    enabled=true;
    psx_mod_counter_add("v82.music.tracks",song_count);
}
PSX_MOD_CONSTRUCTOR(register_music) {
    psx_mod_register_activation_plugin(Plugin,activate);
    for(const auto& s:sites)psx_mod_register_instruction_plugin(Plugin,s.pc,s.word,instruction);
}
}
