// Exercise the actual native preparer/probe and real filesystem identities.
// Only the conversion subprocess is substituted; framework byte verification
// is tested with load_mod_media rather than trusting a receipt's metadata.
#include "../src/mods/v82_media.cpp"
#include "mod_media.h"
#include <iostream>
#include <cstdlib>
#include <chrono>

namespace {
struct Registered { PSXRecompV4::ModMediaPreparer prepare, probe; };
Registered& registered() { static Registered result; return result; }
std::map<std::string, uint32_t>& counters() { static std::map<std::string, uint32_t> result; return result; }
unsigned worker_calls = 0;
bool mutate_in_worker = false;
bool corrupt_in_worker = false;
fs::path produced, selected;
void replace_preserving_time(const fs::path& path, const std::string& data);
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary); out << bytes; out.close(); check(bool(out), "fixture write");
}
void fake_worker(const fs::path&, const fs::path& request, const fs::path& response) {
    ++worker_calls;
    const auto value = toml::parse(request.string());
    check(toml::find<std::string>(value, "feature") == "menus", "real request feature");
    const auto& output = toml::find(value, "outputs", "menu");
    check(toml::find<uint64_t>(output, "size") == 4, "real request expected size");
    check(toml::find<std::string>(output, "sha256") == v82_media_cache::hash_text("good"), "real request expected SHA");
    write(produced, "good");
    check(file_hash(produced) == toml::find<std::string>(output, "sha256"), "helper verified its output");
    if (mutate_in_worker) write(selected, "changed");
    write(response, "[outputs]\nmenu = " + quoted(produced.u8string()) + "\n");
    // A success response exists and the helper checked SHA. Replace the file
    // before the parent can bind its receipt to a filesystem identity.
    if (corrupt_in_worker) replace_preserving_time(produced, "evil");
}
void replace_preserving_time(const fs::path& path, const std::string& data) {
    const auto time = fs::last_write_time(path);
    auto replacement = path; replacement += ".replacement";
    write(replacement, data); fs::last_write_time(replacement, time);
#ifdef _WIN32
    check(MoveFileExW(replacement.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING), "replace fixture");
#else
    fs::rename(replacement, path);
#endif
}
}

namespace PSXRecompV4 {
bool mod_register_media_preparer(const std::string& id, ModMediaPreparer prepare, ModMediaPreparer probe) {
    check(id == "v82.media", "registered actual V82 provider");
    registered() = {std::move(prepare), std::move(probe)}; return true;
}
}
extern "C" void psx_mod_counter_add(const char* name, uint32_t delta) { counters()[name] += delta; }

int main() {
    try {
        const auto root = fs::temp_directory_path() / ("v82-media-receipt-" + std::to_string(
#ifdef _WIN32
            GetCurrentProcessId()
#else
            getpid()
#endif
        ) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        // No cleanup of arbitrary paths: fixture creation must be exclusive.
        check(fs::create_directory(root), "unique fixture directory");
        const auto tools = root / "app/media-tools";
#ifdef _WIN32
        write(tools / "v82-media.exe", "fixture"); write(tools / "psxavenc.exe", "encoder");
#else
        write(tools / "v82-media", "fixture"); write(tools / "psxavenc", "encoder");
#endif
        write(tools / "preparer-revision", std::string(64, 'a') + "\n");
        selected = root / "source/game.iso"; write(selected, std::string(4096, 's'));
        produced = root / "cache/import/menu.bin";
        ModPackage package; package.id = "v82.content.menus"; package.version = "0.1.0";
        ModResource resource; resource.feature_id = "menus"; resource.id = "menu";
        resource.format = "file"; resource.size = 4; resource.sha256 = v82_media_cache::hash_text("good"); resource.required = true;
        package.resources.push_back(resource);
        ModPrepareContext context{package, "menus", selected, root / "cache", root / "app", {}};
        Paths outputs; std::string error;
        check(!registered().probe(context, outputs, error) && outputs.empty(), "initial probe misses transactionally");
        check(prepare_with_worker(context, outputs, error, fake_worker) && worker_calls == 1, "cold verified helper path");
        std::shared_ptr<const std::vector<uint8_t>> bytes;
        check(load_mod_media(outputs.at("menu"), "file", 4, resource.sha256, bytes, &error), "framework validates output bytes");
        const auto snapshot = bytes;
        check(registered().probe(context, outputs, error), "persistent receipt hit");
        // This is the actual registered production callback, with an invalid
        // helper executable: any subprocess attempt would fail the test.
        check(registered().prepare(context, outputs, error) && worker_calls == 1, "warm callback never invokes helper");
        check(counters()["v82.media.helper_spawns"] == 0 && counters()["v82.media.receipt_hits"] >= 2, "warm counters prove no actual spawn");
        ReceiptContext receipt; check(receipt_context(context, receipt), "receipt fixture context");

        replace_preserving_time(produced, "evil");
        check(!registered().probe(context, outputs, error) && outputs.empty(), "same size/mtime output replacement misses");
        check(!load_mod_media(produced, "file", 4, resource.sha256, bytes, &error), "corrupt output SHA fails independently");
        check(std::string(snapshot->begin(), snapshot->end()) == "good", "immutable prior snapshot preserved");
        check(prepare_with_worker(context, outputs, error, fake_worker), "cold output repair");
        replace_preserving_time(selected, std::string(4096, 'x'));
        check(!registered().probe(context, outputs, error), "same size/mtime source replacement misses");
        check(prepare_with_worker(context, outputs, error, fake_worker), "changed source reverified by worker");
        write(receipt.receipt, "version = [torn");
        check(!registered().probe(context, outputs, error) && outputs.empty(), "torn receipt misses");
        check(prepare_with_worker(context, outputs, error, fake_worker), "torn receipt recovered");
        fs::remove(produced);
        check(!registered().probe(context, outputs, error), "missing output misses");
        check(prepare_with_worker(context, outputs, error, fake_worker), "missing output regenerated");
        write(tools / "preparer-revision", std::string(64, 'b') + "\n");
        check(!registered().probe(context, outputs, error), "importer revision invalidates");
        check(prepare_with_worker(context, outputs, error, fake_worker), "new importer establishes receipt");
        package.version = "0.2.0";
        check(!registered().probe(context, outputs, error), "package version invalidates");
        package.version = "0.1.0"; package.resources[0].sha256 = std::string(64, 'f');
        check(!registered().probe(context, outputs, error), "expected resource SHA invalidates");
        package.resources[0].sha256 = resource.sha256;
        auto changed_cache = context; changed_cache.cache_root = root / "other-cache";
        check(!registered().probe(changed_cache, outputs, error), "cache root invalidates");
        auto unsupported = context; unsupported.disc_path = selected.parent_path();
        check(!registered().probe(unsupported, outputs, error) && outputs.empty(), "unreliable non-file identity misses safely");
        write(selected, "dirty"); mutate_in_worker = true;
        check(!prepare_with_worker(context, outputs, error, fake_worker) && outputs.empty(), "concurrent source mutation never publishes/launches");
        mutate_in_worker = false;
        corrupt_in_worker = true;
        check(!prepare_with_worker(context, outputs, error, fake_worker) && outputs.empty(), "unverified helper output never gets a receipt");
        corrupt_in_worker = false;

        // Real CUE parser and path resolver, including referenced audio and a
        // newly added sibling CUE that changes a previously raw BIN selection.
        const auto data = root / "cue/data.bin", audio = root / "cue/audio.bin", cue = root / "cue/game.cue";
        write(data, std::string(2352, 'd')); write(audio, std::string(2352, 'a'));
        Stamps raw; v82_media_cache::add_disc(raw, data);
        write(cue, "FILE \"data.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nFILE \"audio.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n");
        Stamps with_cue; v82_media_cache::add_disc(with_cue, data);
        // CUE resolution/directory iteration may use a different Windows
        // separator spelling. Assert file ownership, not string spelling.
        bool contains_audio = false;
        for (const auto& [path, stamp] : with_cue) {
            (void)stamp;
            if (fs::equivalent(fs::u8path(path), audio)) contains_audio = true;
        }
        check(raw != with_cue && contains_audio, "new sibling CUE/audio dependency included");
        const auto before = with_cue;
        replace_preserving_time(audio, std::string(2352, 'z'));
        Stamps changed; v82_media_cache::add_disc(changed, data);
        check(before != changed, "audio replacement invalidates dependency closure");
        const auto n64 = root / "donor.z64"; write(n64, "rom1");
        Stamps rom; v82_media_cache::add_file(rom, n64);
        replace_preserving_time(n64, "rom2"); check(rom != v82_media_cache::refresh(rom), "N64 source replacement invalidates");
        // Extraction receipts are checked against the original CHD plus its
        // cached CUE/tracks; these dependency functions do not trust CUE alone.
        const auto chd = root / "donor.chd"; write(chd, "chd1");
        Stamps extraction; v82_media_cache::add_file(extraction, chd); v82_media_cache::add_disc(extraction, cue);
        fs::remove(audio);
        bool missing_rejected = false;
        try { v82_media_cache::refresh(extraction); } catch (...) { missing_rejected = true; }
        check(missing_rejected, "missing extracted track rejects cached extraction");
        std::cout << "V82 native receipt and output-integrity contracts passed\n";
        return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 3; }
}
