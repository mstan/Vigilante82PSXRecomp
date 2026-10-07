#pragma once
// A receipt is a lookup accelerator, never an authority for resource bytes.
// resolve() still loads the returned files and verifies their manifest SHA.
#include "host_file_identity.h"
#include "host_path.h"
#include "cue_sheet.h"
#include "disc_path.h"
#include "psx_sha256.h"
#include "toml.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace v82_media_cache {
namespace fs = std::filesystem;
using Stamps = std::map<std::string, std::string>;
using Paths = std::map<std::string, fs::path>;

inline std::string path_text(const fs::path& p) {
    return PSXRecompV4::host_absolute(p).u8string();
}
inline std::string quote(const std::string& text) {
    std::ostringstream out; out << '"';
    const char* digits = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << char(c);
        else if (c < 32) out << "\\u00" << digits[c >> 4] << digits[c & 15];
        else out << char(c);
    }
    return out.str() + '"';
}
inline std::string hash_text(const std::string& text) {
    uint8_t digest[32];
    psx_sha256_compute(reinterpret_cast<const uint8_t*>(text.data()), text.size(), digest);
    std::string result;
    for (uint8_t c : digest) { result += "0123456789abcdef"[c >> 4]; result += "0123456789abcdef"[c & 15]; }
    return result;
}
inline void add_file(Stamps& stamps, const fs::path& path) {
    std::string identity;
    if (!PSXRecompV4::host_file_identity(path, identity))
        throw std::runtime_error("Media identity is unavailable");
    stamps[path_text(path)] = std::move(identity);
}
inline void add_disc(Stamps& stamps, const fs::path& picked) {
    using namespace PSXRecompV4;
    add_file(stamps, picked);
    auto mounted = resolve_disc_path(picked).mount;
    add_file(stamps, mounted);
    auto add_cue = [&](const fs::path& cue) {
        add_file(stamps, cue);
        const auto sheet = parse_cue_sheet(cue);
        if (!sheet.opened) throw std::runtime_error("Cannot read media CUE");
        // All referenced files are conservative dependencies, including audio.
        for (const auto& file : sheet.files) add_file(stamps, file.path);
    };
    if (path_has_extension_ci(mounted, ".cue")) add_cue(mounted);
    // Both resolvers can upgrade a raw selection to an adjacent CUE. Recording
    // the candidate set also invalidates a receipt when a new CUE is added.
    if (path_has_extension_ci(mounted, ".bin") || path_has_extension_ci(mounted, ".img") ||
        path_has_extension_ci(mounted, ".iso")) {
        for (const auto& entry : fs::directory_iterator(mounted.parent_path()))
            if (path_has_extension_ci(entry.path(), ".cue")) add_cue(entry.path());
    }
}
inline Stamps refresh(const Stamps& before) {
    Stamps result;
    for (const auto& [path, ignored] : before) { (void)ignored; add_file(result, fs::u8path(path)); }
    return result;
}
inline std::string read_revision(const fs::path& tools) {
    const auto path = tools / "preparer-revision";
    if (fs::file_size(path) != 65) throw std::runtime_error("Bundled media revision is missing");
    std::ifstream in(path, std::ios::binary);
    std::string revision; std::getline(in, revision);
    if (revision.size() != 64 || revision.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("Bundled media revision is invalid");
    return revision;
}
inline toml::value read_receipt(const fs::path& path) {
    if (fs::file_size(path) > 128 * 1024) throw std::runtime_error("Media receipt is too large");
    return toml::parse(path.string());
}
inline Stamps read_stamps(const toml::value& value, const char* field) {
    Stamps result;
    for (const auto& [path, identity] : toml::find(value, field).as_table())
        result.emplace(path, toml::get<std::string>(identity));
    return result;
}
inline void atomic_write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    static std::atomic<unsigned> serial{0};
#if defined(_WIN32)
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = getpid();
#endif
    auto temp = path; temp += "." + std::to_string(pid) + "-" + std::to_string(++serial) + ".tmp";
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code ec; fs::remove(p, ec); } } cleanup{temp};
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << text; out.close();
    if (!out) throw std::runtime_error("Cannot write media receipt");
#if defined(_WIN32)
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot publish media receipt");
#else
    fs::rename(temp, path);
#endif
}
inline void write_stamps(std::ostream& out, const char* field, const Stamps& stamps) {
    out << '[' << field << "]\n";
    for (const auto& [path, identity] : stamps) out << quote(path) << " = " << quote(identity) << '\n';
}
inline void publish(const fs::path& path, const std::string& key,
                    const Stamps& sources, const Stamps& outputs, const Paths& files,
                    const Paths& discs) {
    if (sources != refresh(sources) || outputs != refresh(outputs))
        throw std::runtime_error("Selected media changed during preparation; retry");
    std::ostringstream out; out << "version = 1\nkey = " << quote(key) << '\n';
    write_stamps(out, "sources", sources); write_stamps(out, "output_stamps", outputs);
    out << "[outputs]\n";
    for (const auto& [id, file] : files) out << quote(id) << " = " << quote(path_text(file)) << '\n';
    out << "[discs]\n";
    for (const auto& [id, file] : discs) out << quote(id) << " = " << quote(path_text(file)) << '\n';
    atomic_write(path, out.str());
}
inline bool probe(const fs::path& path, const std::string& key, Stamps sources,
                  const std::map<std::string, uint64_t>& expected, Paths& files) {
    files.clear();
    try {
        const auto receipt = read_receipt(path);
        if (toml::find<int>(receipt, "version") != 1 || toml::find<std::string>(receipt, "key") != key) return false;
        for (const auto& [id, value] : toml::find(receipt, "discs").as_table()) {
            (void)id; add_disc(sources, fs::u8path(toml::get<std::string>(value)));
        }
        if (sources != read_stamps(receipt, "sources")) return false;
        Paths result; Stamps output_stamps;
        for (const auto& [id, value] : toml::find(receipt, "outputs").as_table()) {
            auto found = expected.find(id);
            if (found == expected.end()) return false;
            const auto file = fs::u8path(toml::get<std::string>(value));
            if (fs::file_size(file) != found->second) return false;
            add_file(output_stamps, file); result[id] = file;
        }
        if (result.size() != expected.size() || output_stamps != read_stamps(receipt, "output_stamps")) return false;
        files = std::move(result); return true;
    } catch (...) { return false; }
}
} // namespace v82_media_cache
