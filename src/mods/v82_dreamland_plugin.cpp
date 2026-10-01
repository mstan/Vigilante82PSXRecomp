/* Native imported-file seams adapted from LibCd.cs and
 * V82Compat.ResolveVirtualLooseFile. Copyright (c) 2026 flaffy, MIT;
 * tools/third_party/LICENSE_RECOMPONE. */
#include "v82_dreamland_helpers.h"
#include <array>
#include <cstring>
#include <string>
extern "C" bool v82_presentation_file(const uint8_t**,uint64_t*);

namespace dreamland {
constexpr auto Plugin="v82.dreamland", Package="v82.content.dreamland";
constexpr uint32_t Meta=0x80310000, Magic=0x444C4E44, Extent=0x700000;
constexpr uint32_t Resolve=0x80018210, Begin=0x80018148, End=0x800181E0;
constexpr uint32_t Read=0x80018478, Seek=0x80018644, Level=0x80033550;
constexpr uint32_t SpuAlloc=0x80052F9C, WholeRead=0x80017F50, ReadByte=0x800185F0;
struct Hook { uint32_t address,word; };
constexpr Hook Hooks[]={{Resolve,0x27BDFFD8},{Begin,0x27BDFFE8},{End,0x27BDFFF8},
                       {Read,0x8F820D64},{Seek,0x27BDFFF0},{Level,0x27BDFFF0},
                       {SpuAlloc,0x3C028006},{WholeRead,0x27BDFFE8},{ReadByte,0x8F820D64}};
struct File {const char* path;const char* resource;const uint8_t* data=nullptr;uint64_t size=0;};
std::array<File,6> files={File{"LEVELS\\N64\\DREAMLND.EXP","arena"},File{"LEVELS\\N64\\DREAMLND.DLL","overlay"},
                         File{"SHARED\\SOUNDS.SND","sounds"},File{"SHELL\\SHELL.DLL","menu"},
                         File{"SHELL\\LEVELSEL.TBL","previews"},File{"SHELL\\SOUNDS.SND",nullptr}};
bool active=false,force_match=false;
bool allocating_sound=false;
void read(const File& file,uint32_t offset,uint32_t length,uint32_t destination) {
    if(uint64_t(offset)+length>((file.size+2047)&~uint64_t(2047)))fail("v82.dreamland.read_out_of_bounds");
    for(uint32_t i=0;i<length;++i)psx_mod_write_byte(destination+i,offset+i<file.size?file.data[offset+i]:0);
    psx_mod_counter_add("v82.dreamland.bytes_read",length);
}
void init() {
    if(rd(Meta)==Magic)return;
    for(uint32_t p=Meta;p<Meta+0x300;p+=4)wr(p,0);
    wr(Meta,Magic);
    constexpr char path[]="Levels\\N64\\DreamLnd.exp";
    for(unsigned i=0;i<sizeof path;++i)psx_mod_write_byte(Meta+0x200+i,uint8_t(path[i]));
}
std::string path_at(uint32_t address) {
    std::string path;
    for(unsigned i=0;i<256;++i) {
        char ch=char(psx_mod_read_byte(address+i));if(!ch)return path;
        if(ch=='/')ch='\\';if(ch>='a'&&ch<='z')ch=char(ch-'a'+'A');
        if(ch=='\\'&&path.empty())continue;
        path+=ch;
    }
    fail("v82.dreamland.invalid_path");
}
void entry(CPUState* c,uint32_t address) {
    if(!active || (address==SpuAlloc && allocating_sound))return;
    for(const auto& h:Hooks)if(h.address==address&&rd(address)!=h.word) {
        psx_mod_counter_add("v82.dreamland.hook_word_mismatch",1);return;
    }
    init();
    // Host resource pointers are reconstructed after a state load too,
    // including a state taken partway through reading the selector bank.
    if(!v82_presentation_file(&files[5].data,&files[5].size)) {
        files[5].data=nullptr;files[5].size=0;
    }
    const auto gp=c->gpr[28];
    if(address==SpuAlloc) {
        // Observe the native allocator without resetting it on failure: car
        // banks are already live when the shared match bank is loaded.
        const auto requested=c->gpr[4];
        allocating_sound=true;
        const auto result=invoke(c,SpuAlloc,requested);
        if(result==UINT32_MAX) {
            psx_mod_counter_add("v82.spu.allocation_failed",1);
            psx_mod_counter_add("v82.spu.failed_bytes",requested);
        }
        allocating_sound=false;c->gpr[2]=result;psx_mod_finish_function(c);return;
    }
    if(address==WholeRead) {
        const auto lba=c->gpr[5];
        for(unsigned i=0;i<files.size();++i) {
            const auto base=Extent+i*0x10000;
            if(lba>=base && lba-base<(files[i].size+2047)/2048) {
                if(c->gpr[6]>UINT32_MAX/2048)fail("v82.dreamland.read_out_of_bounds");
                read(files[i],(lba-base)*2048,c->gpr[6]*2048,c->gpr[4]);
                c->gpr[2]=c->gpr[4];psx_mod_finish_function(c);return;
            }
        }
        return;
    }
    if(address==Level) {
        if(force_match) {c->gpr[4]=Meta+0x200;psx_mod_counter_add("v82.dreamland.match_requested",1);}
        return;
    }
    if(address==Resolve) {
        const auto path=path_at(c->gpr[4]);
        for(unsigned i=0;i<files.size();++i)if(path==files[i].path) {
            if(i==5&&!v82_presentation_file(&files[i].data,&files[i].size))return;
            // SHELL stores the accepted location index here before the match
            // opens shared sounds. Only index 18 owns the appended N64 sounds;
            // retail arenas must retain their smaller, original disc bank.
            if(i==2 && psx_mod_read_byte(gp+0xC28)!=18) {
                psx_mod_counter_add("v82.dreamland.retail_sounds",1);return;
            }
            const auto descriptor=Meta+0x100+i*32;
            wr(descriptor+0xC,Extent+i*0x10000);wr(descriptor+0x10,uint32_t(files[i].size));
            c->gpr[2]=descriptor;psx_mod_finish_function(c);
            constexpr const char* counters[]={"v82.dreamland.arena_resolved","v82.dreamland.overlay_resolved",
                "v82.dreamland.sounds_resolved","v82.dreamland.menu_resolved","v82.dreamland.previews_resolved","v82.selector.sounds_resolved"};
            psx_mod_counter_add(counters[i],1);return;
        }
        return;
    }
    if(address==Begin) {
        const auto lba=c->gpr[4];wr(Meta+4,0);
        for(unsigned i=0;i<files.size();++i)if(lba==Extent+i*0x10000) {
            wr(Meta+4,i+1);wr(gp+0xD64,0);wr(gp+0xD68,lba);wr(gp+0xD6C,lba);
            c->gpr[2]=1;psx_mod_finish_function(c);psx_mod_counter_add("v82.dreamland.file_begin",1);return;
        }
        return;
    }
    const auto file_id=rd(Meta+4);
    if(!file_id)return;
    if(file_id>files.size())fail("v82.dreamland.invalid_saved_file");
    const auto& file=files[file_id-1];
    if(address==End) {
        wr(Meta+4,0);psx_mod_finish_function(c);psx_mod_counter_add("v82.dreamland.file_end",1);
    } else if(address==Read) {
        const uint32_t offset=rd(gp+0xD64), length=c->gpr[5], destination=c->gpr[4];
        read(file,offset,length,destination);
        wr(gp+0xD64,offset+length);c->gpr[2]=1;psx_mod_finish_function(c);
    } else if(address==ReadByte) {
        const auto offset=rd(gp+0xD64);
        if(offset>=((file.size+2047)&~uint64_t(2047)))fail("v82.dreamland.read_out_of_bounds");
        c->gpr[2]=offset<file.size?file.data[offset]:0;
        wr(gp+0xD64,offset+1);psx_mod_finish_function(c);
        psx_mod_counter_add("v82.dreamland.bytes_read",1);
    } else if(address==Seek) {
        const uint32_t target=c->gpr[5]?rd(gp+0xD64)+c->gpr[4]:c->gpr[4];
        if(target>((file.size+2047)&~uint64_t(2047)))fail("v82.dreamland.seek_out_of_bounds");
        wr(gp+0xD64,target);psx_mod_finish_function(c);
    }
}
void activate() {
    active=false;force_match=false;
#ifndef PSX_NO_DEBUG_TOOLS
    const auto stress=std::getenv("PSX_V82_TEST_DREAMLAND_PROJECTILES");
    stress_enabled=stress&&!std::strcmp(stress,"1");
    if(stress_enabled)psx_mod_counter_add("v82.stress.enabled",1);
#endif
    for(auto& file:files)if(file.resource&&(!psx_mod_current_resource_bytes(file.resource,&file.data,&file.size)||file.size>UINT32_MAX)) {
        psx_mod_counter_add("v82.dreamland.missing_resource",1);return;
    }
    if(!psx_mod_set_main_ram_8mb(1)) {psx_mod_counter_add("v82.dreamland.ram_failed",1);return;}
    // The native nineteenth location supplies its own path. No match override.
    active=true;
    psx_mod_counter_add("v82.dreamland.activated",1);
}
PSX_MOD_CONSTRUCTOR(register_dreamland) {
    psx_mod_register_activation_plugin(Plugin,activate);
    for(const auto& h:Hooks)psx_mod_register_function_entry_plugin(Plugin,h.address,entry);
    for(uint32_t pc=Hazard;pc<=ArenaEvent;pc+=4)psx_mod_register_guest_function_plugin(Plugin,pc,helper);
}
}
