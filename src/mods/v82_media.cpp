/* Launcher-side content preparation. The worker and encoder ship with the
 * application; only user-owned disc images/ROMs are selected in Mods. */
#include "mod_packages.h"
#include "mod_plugins.h"
#include "disc_path.h"
#include "iso_reader.h"
#include "psx_sha256.h"
#include "toml.hpp"
#include "v82_media_receipt.h"
#include <array>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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
using v82_media_cache::Stamps;
using v82_media_cache::Paths;
std::string quoted(const std::string& text);

struct ReceiptContext {
    std::string key;
    fs::path receipt;
    Stamps sources;
    std::map<std::string, uint64_t> expected;
};

bool receipt_context(const ModPrepareContext& context, ReceiptContext& result) {
    try {
        const auto tools = context.application_dir / "media-tools";
        std::ostringstream key;
        key << "v82-receipt-1\n" << quoted(context.package.id) << '\n'
            << quoted(context.package.version) << '\n' << quoted(context.feature_id) << '\n'
            << quoted(v82_media_cache::path_text(context.cache_root)) << '\n'
            << quoted(v82_media_cache::read_revision(tools)) << '\n';
        Stamps sources;
        v82_media_cache::add_file(sources, tools / "preparer-revision");
#if defined(_WIN32)
        v82_media_cache::add_file(sources, tools / "v82-media.exe");
        v82_media_cache::add_file(sources, tools / "psxavenc.exe");
#else
        v82_media_cache::add_file(sources, tools / "v82-media");
        v82_media_cache::add_file(sources, tools / "psxavenc");
#endif
        key << quoted(v82_media_cache::path_text(context.disc_path)) << '\n';
        v82_media_cache::add_disc(sources, context.disc_path);
        for (const auto& [id, path] : context.inputs) {
            key << quoted(id) << ' ' << quoted(v82_media_cache::path_text(path)) << '\n';
            if (id == "original") v82_media_cache::add_disc(sources, path);
            else v82_media_cache::add_file(sources, path);
        }
        std::map<std::string, uint64_t> expected;
        for (const auto& resource : context.package.resources) if (resource.feature_id == context.feature_id) {
            key << quoted(resource.id) << ' ' << quoted(resource.format) << ' ' << resource.size << ' '
                << quoted(resource.sha256) << ' ' << resource.input_only << ' ' << resource.required << '\n';
            if (!resource.input_only) expected[resource.id] = resource.size;
        }
        result.key = v82_media_cache::hash_text(key.str());
        result.receipt = context.cache_root / "v82-receipts-v1" / (result.key + ".toml");
        result.sources = std::move(sources); result.expected = std::move(expected);
        return true;
    } catch (...) { return false; } // Unsupported identity always takes the verified cold path.
}

bool probe(const ModPrepareContext& context, Paths& outputs, std::string& error) {
    outputs.clear(); error.clear();
    ReceiptContext receipt;
    const bool hit = receipt_context(context, receipt) && v82_media_cache::probe(
        receipt.receipt, receipt.key, receipt.sources, receipt.expected, outputs);
    psx_mod_counter_add(hit ? "v82.media.receipt_hits" : "v82.media.receipt_misses", 1);
    return hit;
}

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
    const auto receipt = directory / "extraction.toml";
    Stamps chd_source;
    bool reliable = false;
    try { v82_media_cache::add_file(chd_source, path); reliable = true; } catch (...) {}
    if (reliable) {
        try {
            const auto value = v82_media_cache::read_receipt(receipt);
            auto stamps = chd_source; v82_media_cache::add_disc(stamps, cue);
            if (toml::find<int>(value, "version") == 1 &&
                stamps == v82_media_cache::read_stamps(value, "sources")) return cue;
        } catch (...) {} // Old/partial/modified extraction is rebuilt from the CHD.
    }
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
    std::error_code ec; fs::remove(cue, ec); fs::rename(directory/"disc.cue.tmp",cue);
    if (reliable) {
        if (chd_source != v82_media_cache::refresh(chd_source))
            throw std::runtime_error("CHD changed during extraction; retry");
        auto stamps = chd_source; v82_media_cache::add_disc(stamps, cue);
        std::ostringstream receipt_text; receipt_text << "version = 1\n";
        v82_media_cache::write_stamps(receipt_text, "sources", stamps);
        v82_media_cache::atomic_write(receipt, receipt_text.str());
    }
    return cue;
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
    psx_mod_counter_add("v82.media.helper_spawns", 1);
    CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    if(code)throw std::runtime_error("The media preparer stopped before completing");
#else
    pid_t child=fork();
    if(child<0)throw std::runtime_error("Cannot start the bundled media preparer");
    if(!child){execl(executable.c_str(),executable.c_str(),request.c_str(),response.c_str(),nullptr);_exit(127);}
    psx_mod_counter_add("v82.media.helper_spawns", 1);
    int status=0;if(waitpid(child,&status,0)<0 || !WIFEXITED(status) || WEXITSTATUS(status))
        throw std::runtime_error("The media preparer stopped before completing");
#endif
}

using Worker = void (*)(const fs::path&, const fs::path&, const fs::path&);
bool prepare_with_worker(const ModPrepareContext& context,Paths& outputs,std::string& error, Worker worker){
    if (probe(context, outputs, error)) return true;
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
        ReceiptContext receipt;
        const bool cacheable = receipt_context(context, receipt);
        Paths discs;
        discs["disc"] = worker_disc(context.disc_path, context.cache_root);
        for (const auto& [id, path] : context.inputs)
            if (id == "original") discs[id] = worker_disc(path, context.cache_root);
        Stamps sources = receipt.sources;
        bool stable_cacheable = cacheable;
        if (cacheable) {
            try { for (const auto& [id, path] : discs) { (void)id; v82_media_cache::add_disc(sources, path); } }
            catch (...) { stable_cacheable = false; }
        }
        fs::create_directories(context.cache_root/"requests");
        static std::atomic<unsigned> serial{0};
        auto request=context.cache_root/"requests"/(std::to_string(pid)+"-"+std::to_string(++serial)+".toml");
        auto response=request;response.replace_extension("result.toml");
        struct Cleanup {fs::path a,b;~Cleanup(){std::error_code ec;fs::remove(a,ec);fs::remove(b,ec);}} cleanup{request,response};
        std::ofstream out(request,std::ios::binary);
        out << "feature = " << quoted(context.feature_id) << "\ncache = " << quoted(context.cache_root.u8string())
            << "\ndisc = " << quoted(discs.at("disc").u8string())
            << "\nencoder = " << quoted(encoder.u8string()) << "\n[inputs]\n";
        for(const auto& [id,path]:context.inputs)
            out << quoted(id) << " = " << quoted((id=="original"?discs.at(id):path).u8string()) << '\n';
        for(const auto& resource:context.package.resources)if(resource.feature_id==context.feature_id && !resource.input_only)
            out << "[outputs." << quoted(resource.id) << "]\nsize = " << resource.size
                << "\nsha256 = " << quoted(resource.sha256) << '\n';
        out.close();if(!out)throw std::runtime_error("Cannot write preparation request");
        worker(helper,request,response);
        auto result=toml::parse(response.string());
        if(result.contains("error"))throw std::runtime_error(toml::find<std::string>(result,"error"));
        Paths prepared;
        for(const auto& [id,path]:toml::find(result,"outputs").as_table())prepared[id]=fs::u8path(toml::get<std::string>(path));
        // Bind a persistent receipt to verified bytes, closing the gap between
        // helper validation and the parent observing the output's identity.
        // This extra check occurs only when establishing a cold receipt; the
        // framework independently snapshots and validates files on resolution.
        if (stable_cacheable) {
            ReceiptContext current;
            if (!receipt_context(context, current) || current.key != receipt.key || current.sources != receipt.sources)
                throw std::runtime_error("Selected media changed during preparation; retry");
            Stamps output_stamps;
            if (prepared.size() != receipt.expected.size()) throw std::runtime_error("Media preparer returned different resources");
            for (const auto& [id, path] : prepared) {
                auto expected = receipt.expected.find(id);
                if (expected == receipt.expected.end() || fs::file_size(path) != expected->second)
                    throw std::runtime_error("Media preparer returned an invalid resource");
                try { v82_media_cache::add_file(output_stamps, path); }
                catch (...) { stable_cacheable = false; }
            }
            if (stable_cacheable) {
                for (const auto& resource : context.package.resources)
                    if (resource.feature_id == context.feature_id && !resource.input_only &&
                        file_hash(prepared.at(resource.id)) != resource.sha256)
                        throw std::runtime_error("Prepared output changed after verification; retry");
                if (!receipt_context(context, current) || current.key != receipt.key || current.sources != receipt.sources)
                    throw std::runtime_error("Selected media changed during preparation; retry");
                v82_media_cache::publish(receipt.receipt, receipt.key, sources, output_stamps, prepared, discs);
            }
        }
        outputs = std::move(prepared);
        return true;
    } catch(const std::exception& ex){outputs.clear();error=ex.what();return false;}
}
bool prepare(const ModPrepareContext& context, Paths& outputs, std::string& error) {
    return prepare_with_worker(context, outputs, error, run_worker);
}
PSX_MOD_CONSTRUCTOR(register_media){mod_register_media_preparer("v82.media",prepare,probe);}
}
