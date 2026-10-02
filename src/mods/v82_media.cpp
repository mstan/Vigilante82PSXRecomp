/* Launcher-side content preparation. The worker and encoder ship with the
 * application; only user-owned disc images/ROMs are selected in Mods. */
#include "mod_packages.h"
#include "disc_path.h"
#include "iso_reader.h"
#include "psx_sha256.h"
#include "toml.hpp"
#include <array>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
using namespace PSXRecompV4;

std::string quoted(const std::string& text) {
    std::ostringstream out; out << '"';
    for(unsigned char c:text) {
        if(c=='"' || c=='\\') out << '\\' << char(c);
        else if(c<32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
        else out << char(c);
    }
    return out.str()+'"';
}

std::string file_hash(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("Cannot open disc image");
    psx_sha256_ctx ctx;psx_sha256_init(&ctx);
    std::array<uint8_t,65536> buffer{};
    while(in){in.read(reinterpret_cast<char*>(buffer.data()),buffer.size());psx_sha256_update(&ctx,buffer.data(),size_t(in.gcount()));}
    if(!in.eof()) throw std::runtime_error("Cannot read complete disc image");
    uint8_t hash[32];psx_sha256_final(&ctx,hash);
    std::ostringstream out;
    for(uint8_t b:hash)out << std::hex << std::setw(2) << std::setfill('0') << unsigned(b);
    return out.str();
}

fs::path worker_disc(const fs::path& source,const fs::path& cache) {
    auto path=resolve_disc_path(source).mount;
    auto extension=path.extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(extension!=".chd")return path;
    // Reuse the framework's CHD reader; users need no external extraction tool.
    auto directory=cache/"v82-disc"/file_hash(path),cue=directory/"disc.cue";
    if(fs::is_regular_file(cue))return cue;
    PS1::ISOReader disc;
    if(!disc.Open(path.string()))throw std::runtime_error("Cannot open CHD image");
    fs::create_directories(directory);
    std::ostringstream toc;
    const int count=disc.TrackCount();
    if(count<1 || count>99 || disc.TrackIsAudio(1))throw std::runtime_error("Invalid data disc TOC");
    for(int track=1;track<=count;++track){
        auto start=disc.TrackStartLBA(track);
        auto end=track<count?disc.TrackPregapLBA(track+1):disc.GetSectorCount();
        if(end<=start || end-start>450000)throw std::runtime_error("Invalid disc track bounds");
        std::string name="track-"+std::to_string(track)+".bin";
        auto temporary=directory/(name+".tmp");
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        std::array<uint8_t,2352> sector{};
        for(auto lba=start;lba<end;++lba){
            if(!disc.ReadRawSector(lba,sector.data()))throw std::runtime_error("Cannot extract CHD sector");
            out.write(reinterpret_cast<const char*>(sector.data()),sector.size());
        }
        out.close();if(!out)throw std::runtime_error("Cannot cache CHD track");
        std::error_code ec;fs::remove(directory/name,ec);fs::rename(temporary,directory/name);
        toc << "FILE \"" << name << "\" BINARY\n  TRACK " << std::setw(2) << std::setfill('0') << track
            << (disc.TrackIsAudio(track)?" AUDIO\n":" MODE2/2352\n") << "    INDEX 01 00:00:00\n";
    }
    std::ofstream out(directory/"disc.cue.tmp",std::ios::binary);out << toc.str();out.close();
    if(!out)throw std::runtime_error("Cannot cache disc TOC");
    fs::rename(directory/"disc.cue.tmp",cue);return cue;
}

void run_worker(const fs::path& executable,const fs::path& request,const fs::path& response) {
#if defined(_WIN32)
    // Native argv quoting, never ShellExecute or cmd.exe. No console windows.
    auto arg=[](const fs::path& p){
        std::wstring result=L"\"";unsigned slashes=0;
        for(wchar_t c:p.wstring()){
            if(c==L'\\'){++slashes;continue;}
            result.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
            if(c==L'"')result+=L'\\';result+=c;
        }
        result.append(slashes*2,L'\\');return result+L'"';
    };
    std::wstring command=arg(executable)+L" "+arg(request)+L" "+arg(response);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
                       nullptr,executable.parent_path().c_str(),&startup,&process))
        throw std::runtime_error("Cannot start the bundled media preparer");
    CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    if(code)throw std::runtime_error("The media preparer stopped before completing");
#else
    pid_t child=fork();
    if(child<0)throw std::runtime_error("Cannot start the bundled media preparer");
    if(!child){execl(executable.c_str(),executable.c_str(),request.c_str(),response.c_str(),nullptr);_exit(127);}
    int status=0;if(waitpid(child,&status,0)<0 || !WIFEXITED(status) || WEXITSTATUS(status))
        throw std::runtime_error("The media preparer stopped before completing");
#endif
}

bool prepare(const ModPrepareContext& context,std::map<std::string,fs::path>& outputs,std::string& error){
    try {
        const auto tools=context.application_dir/"media-tools";
#if defined(_WIN32)
        const auto helper=tools/"v82-media.exe",encoder=tools/"psxavenc.exe";
        const auto pid=GetCurrentProcessId();
#else
        const auto helper=tools/"v82-media",encoder=tools/"psxavenc";
        const auto pid=getpid();
#endif
        if(!fs::is_regular_file(helper))throw std::runtime_error("Bundled media-tools are missing. Restore them from this build's download.");
        if(context.disc_path.empty())throw std::runtime_error("Select the 2nd Offense disc on the main page first");
        fs::create_directories(context.cache_root/"requests");
        static std::atomic<unsigned> serial{0};
        auto request=context.cache_root/"requests"/(std::to_string(pid)+"-"+std::to_string(++serial)+".toml");
        auto response=request;response.replace_extension("result.toml");
        struct Cleanup {fs::path a,b;~Cleanup(){std::error_code ec;fs::remove(a,ec);fs::remove(b,ec);}} cleanup{request,response};
        std::ofstream out(request,std::ios::binary);
        out << "feature = " << quoted(context.feature_id) << "\ncache = " << quoted(context.cache_root.u8string())
            << "\ndisc = " << quoted(worker_disc(context.disc_path,context.cache_root).u8string())
            << "\nencoder = " << quoted(encoder.u8string()) << "\n[inputs]\n";
        for(const auto& [id,path]:context.inputs)
            out << quoted(id) << " = " << quoted((id=="original"?worker_disc(path,context.cache_root):path).u8string()) << '\n';
        for(const auto& resource:context.package.resources)if(resource.feature_id==context.feature_id && !resource.input_only)
            out << "[outputs." << quoted(resource.id) << "]\nsize = " << resource.size
                << "\nsha256 = " << quoted(resource.sha256) << '\n';
        out.close();if(!out)throw std::runtime_error("Cannot write preparation request");
        run_worker(helper,request,response);
        auto result=toml::parse(response.string());
        if(result.contains("error"))throw std::runtime_error(toml::find<std::string>(result,"error"));
        for(const auto& [id,path]:toml::find(result,"outputs").as_table())outputs[id]=fs::u8path(toml::get<std::string>(path));
        return true;
    } catch(const std::exception& ex){error=ex.what();return false;}
}
PSX_MOD_CONSTRUCTOR(register_media){mod_register_media_preparer("v82.media",prepare);}
}
