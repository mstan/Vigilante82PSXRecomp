// Resident Loading contract: the adapter against a transliteration of the
// original stream loader (80018148..80018708) with synchronous sector
// delivery, plus chaining, identity, fallback, snapshot and WholeRead rules.
#include "../src/mods/v82_resident.cpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>
using namespace v82_resident;

struct PSXResidentPack {
    std::vector<std::vector<uint8_t>> files;  // sector-padded
    std::vector<uint32_t> lba, stock;
};
struct PSXResidentSink {};

namespace {
std::vector<uint8_t> ram(0x800000);
std::array<uint8_t, 64> mod_memory{};
PSXResidentPack fake;
bool contract_ok = true, finished = false, fail_prepare = false;
unsigned dma_writes = 0, host_writes = 0;
struct GuestCall { uint32_t function, a0; };
std::vector<GuestCall> guest_calls;
uint8_t* at(uint32_t a) {
    if (a >= 0x9F000000u && a < 0x9F000000u + mod_memory.size()) return &mod_memory[a - 0x9F000000u];
    if ((a & 0x1FFFFFFFu) >= ram.size()) throw std::runtime_error("guest access out of RAM");
    return &ram[a & 0x1FFFFFFFu];
}
void require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
}  // namespace

extern "C" {
uint8_t psx_mod_read_byte(uint32_t a) { return *at(a); }
void psx_mod_write_byte(uint32_t a, uint8_t v) { *at(a) = v; }
uint32_t psx_mod_read_word(uint32_t a) { uint32_t v; std::memcpy(&v, at(a), 4); return v; }
void psx_mod_write_word(uint32_t a, uint32_t v) { std::memcpy(at(a), &v, 4); }
int psx_mod_finish_function(CPUState* c) { c->pc = c->gpr[31]; finished = true; return 1; }
void psx_mod_counter_add(const char*, uint32_t) {}
uint32_t psx_mod_call_guest(CPUState*, uint32_t f, uint32_t, uint32_t a0, uint32_t, uint32_t, uint32_t) {
    guest_calls.push_back({f, a0}); return 0;
}
int psx_mod_dma_write_ram(uint32_t a, const void* d, uint32_t n, int) {
    ++dma_writes; std::memcpy(at(a), d, n); return 1;
}
int psx_mod_host_write_ram(uint32_t a, const void* d, uint32_t n) {
    ++host_writes; if (n) std::memcpy(at(a), d, n); return 1;
}
uint32_t psx_mod_alloc_guest_memory(uint32_t n, uint32_t) { return n <= mod_memory.size() ? 0x9F000000u : 0; }
int psx_mod_register_activation_plugin(const char*, PSXModActivationCallback) { return 1; }
int psx_mod_register_function_entry_plugin(const char*, uint32_t, PSXModFunctionEntryCallback) { return 1; }
const PSXResidentPack* psx_resident_prepare(const PSXResidentSpec* s) {
    require(s->file_count == FileCount && s->policy == PSX_RESIDENT_ALLOW_MODIFIED, "spec");
    return fail_prepare ? nullptr : &fake;
}
uint32_t psx_resident_modified_files(const PSXResidentPack*) { return 0; }
const uint8_t* psx_resident_file(const PSXResidentPack* p, uint32_t f, uint32_t* size, uint32_t* padded) {
    if (f >= p->files.size()) return nullptr;
    if (size) *size = uint32_t(p->files[f].size());
    if (padded) *padded = uint32_t(p->files[f].size());
    return p->files[f].data();
}
uint32_t psx_resident_file_lba(const PSXResidentPack* p, uint32_t f) { return p->lba.at(f); }
int psx_resident_file_stock(const PSXResidentPack* p, uint32_t f) { return int(p->stock.at(f)); }
const uint8_t* psx_resident_find_lba(const PSXResidentPack* p, uint32_t lba, uint32_t bytes, uint32_t* file) {
    for (uint32_t i = 0; i < p->files.size(); ++i) {
        if (lba < p->lba[i]) continue;
        const uint64_t off = uint64_t(lba - p->lba[i]) * 2048u;
        if (off <= p->files[i].size() && bytes <= p->files[i].size() - off) {
            if (file) *file = i;
            return p->files[i].data() + off;
        }
    }
    return nullptr;
}
int psx_resident_guest_ranges_match(const PSXResidentRange*, uint32_t, const char*) { return contract_ok; }
void psx_resident_record(const char*, uint32_t, uint32_t, uint32_t, int) {}
}

namespace {
constexpr uint32_t Gp = 0x8006E000u, Disc = 0x9000u;  // disc image base LBA for the model
std::vector<uint8_t> disc;                            // sectors Disc.. contiguous

// Transliteration of the original loader with a drive that delivers each
// sector the moment Wait needs it (timing-free; only guest data matters).
struct Original {
    uint32_t lba = 0, position = 0, sector = 0;  // D68, D64, D58
    std::array<std::vector<uint8_t>, 2> slot{std::vector<uint8_t>(2048), std::vector<uint8_t>(2048)};
    unsigned toggle = 0;
    uint32_t base = 0;                            // D6C
    void begin(uint32_t l) { lba = l; }
    const uint8_t* wait() {
        auto& s = slot[toggle ^= 1];
        std::memcpy(s.data(), &disc[size_t(lba - Disc) * 2048u], 2048);
        ++lba;
        return s.data();
    }
    void read(uint8_t* dest, int32_t n) {
        if (position & 2047u) {
            const uint32_t head = std::min<uint32_t>(n, 2048u - (position & 2047u));
            std::memcpy(dest, cur() + (position & 2047u), head);
            dest += head; n -= int32_t(head); position += head;
        }
        while (n >= 2048) { std::memcpy(dest, wait(), 2048); dest += 2048; n -= 2048; position += 2048; }
        if (n) { current = wait(); std::memcpy(dest, current, size_t(n)); position += uint32_t(n); }
    }
    uint8_t read_byte() {
        if (!(position & 2047u)) current = wait();
        return cur()[position++ & 2047u];
    }
    void seek(int32_t offset, int whence) {
        int32_t target = whence ? int32_t(position) + offset : offset;
        if (target < int32_t(position) && ((uint32_t(target) ^ position) & ~2047u)) {
            position = uint32_t(target) & ~2047u;
            begin(base + (position >> 11));
        }
        int32_t waits = ((target - 1) >> 11) - ((int32_t(position) - 1) >> 11);
        while (waits-- > 0) current = wait();
        position = uint32_t(target);
    }
    const uint8_t* current = nullptr;
    const uint8_t* cur() const { return current; }
};

CPUState call(uint32_t ra, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0) {
    CPUState c{};
    c.gpr[28] = Gp; c.gpr[31] = ra; c.gpr[4] = a0; c.gpr[5] = a1; c.gpr[6] = a2;
    finished = false;
    return c;
}
bool hook(uint32_t address, CPUState& c) { entry(&c, address); return finished; }
void code_words() {
    for (const auto& h : Hooks) psx_mod_write_word(h.address, h.word);
}
bool open(uint32_t lba, uint32_t ra = OpenBeginReturn) {
    psx_mod_write_word(Gp + GpBase, lba);
    CPUState c = call(ra, lba);
    const bool served = hook(Begin, c);
    if (served) psx_mod_write_word(Gp + GpPosition, 0);  // Open's own store
    return served;
}
}  // namespace

int main() try {
    // Three files at Disc, Disc+3 (stock) and Disc+5 (modified); trailing
    // non-file sector so a read past file 0 has somewhere to fall back to.
    disc.resize(8 * 2048);
    std::mt19937 rng(82);
    for (auto& b : disc) b = uint8_t(rng());
    const uint32_t sectors[3] = {3, 2, 2};
    uint32_t next = Disc;
    for (uint32_t i = 0; i < 3; ++i) {
        fake.lba.push_back(next);
        fake.files.emplace_back(disc.begin() + (next - Disc) * 2048, disc.begin() + (next - Disc + sectors[i]) * 2048);
        fake.stock.push_back(i != 2);
        next += sectors[i];
    }
    fake.files.resize(FileCount); fake.lba.resize(FileCount, 0xFFFFFF00u); fake.stock.resize(FileCount, 1);
    code_words();
    activate();
    require(pack == &fake && record == 0x9F000000u, "activation allocates the record");

    // Randomized operation sequences: adapter vs original, byte for byte.
    for (int round = 0; round < 400; ++round) {
        Original o; o.base = Disc; o.begin(Disc); o.position = 0;
        require(open(Disc), "Open's stream is served");
        require(psx_mod_read_word(record) == RecordMagic && psx_mod_read_word(Gp + GpNextLba) == Disc &&
                psx_mod_read_byte(Gp + GpCdState) == 0xFF, "begin state");
        for (int op = 0; op < 24; ++op) {
            const uint32_t pos = psx_mod_read_word(Gp + GpPosition);
            require(pos == o.position, "position matches the original");
            const uint32_t left = 3 * 2048 - pos;
            const int kind = int(rng() % 3);
            if (kind == 0 && left) {
                const int32_t n = int32_t(rng() % (std::min<uint32_t>(left, 5000) + 1));
                std::vector<uint8_t> want(size_t(n) + 1);
                o.read(want.data(), n);
                CPUState c = call(0x80032E28u, 0x80100000u, uint32_t(n));
                require(hook(Read, c) && c.gpr[2] == 1, "read served");
                require(!std::memcmp(at(0x80100000u), want.data(), size_t(n)), "read bytes");
            } else if (kind == 1 && left) {
                const uint8_t want = o.read_byte();
                CPUState c = call(0x80032E28u);
                require(hook(ReadByte, c) && c.gpr[2] == want, "read byte");
            } else {
                const int whence = int(rng() & 1);
                const int32_t target = int32_t(rng() % (3 * 2048 + 1));
                const int32_t offset = whence ? target - int32_t(pos) : target;
                o.seek(offset, whence);
                CPUState c = call(0x80033048u, uint32_t(offset), uint32_t(whence));
                require(hook(Seek, c), "seek served");
            }
            // D58 holds the sector containing the position, as Wait left it.
            const uint32_t p = psx_mod_read_word(Gp + GpPosition);
            if (p & 2047u)
                require(!std::memcmp(at(psx_mod_read_word(Gp + GpSector)), &disc[size_t(p >> 11) * 2048], 2048),
                        "current sector buffer");
        }
        // Snapshot/restore in the middle of a served stream (all state lives
        // in guest RAM and the record): continue identically afterwards.
        const auto saved_ram = ram; const auto saved_mod = mod_memory;
        std::fill(ram.begin(), ram.end(), 0); mod_memory.fill(0);
        ram = saved_ram; mod_memory = saved_mod;
        if (psx_mod_read_word(Gp + GpPosition) < 3 * 2048) {
            const uint8_t want = o.read_byte();
            CPUState c = call(0x80032E28u);
            require(hook(ReadByte, c) && c.gpr[2] == want, "read after restore");
        }
        CPUState c = call(0x8001845Cu + 8);
        require(hook(End, c) && !psx_mod_read_word(record) && !psx_mod_read_word(Gp + GpNextLba), "end");
    }

    // Past the last resident sector: original CD stream at the next sector.
    require(open(Disc), "reopen");
    { CPUState c = call(0x80033048u, 2 * 2048 + 100, 0); require(hook(Seek, c), "seek near end"); }
    guest_calls.clear();
    { CPUState c = call(0x80032E28u, 0x80100000u, 2048);
      require(!hook(Read, c), "overlong read runs the original body");
      require(guest_calls.size() == 1 && guest_calls[0].function == Begin && guest_calls[0].a0 == Disc + 3,
              "fallback starts the drive at the next unread sector");
      require(!psx_mod_read_word(record), "fallback drops the record");
      require(!std::memcmp(at(psx_mod_read_word(Gp + GpSector)), &disc[2 * 2048], 2048),
              "partial sector left for the original body"); }

    // Declines: Seek's restart, mid-file start, modified file, virtual
    // Dreamland extent, wrong code. Nothing is finished; record cleared.
    require(!open(Disc, 0x800186B0u), "Seek's Begin keeps the drive");
    require(!open(Disc + 1), "base must be the file's first sector");
    require(!open(Disc + 5), "modified file uses the original loader");
    require(!open(0x700000u), "virtual extents belong to other plugins");
    { CPUState c = call(0x80032E28u, 0x80100000u, 16); require(!hook(Read, c), "no stream, no read"); }
    contract_ok = false; require(!open(Disc), "loader code contract"); contract_ok = true;
    psx_mod_write_word(Read, 0); require(open(Disc), "open");
    { CPUState c = call(0x80032E28u, 0x80100000u, 16); require(!hook(Read, c), "hook word guard"); }
    code_words();
    { CPUState c = call(0x8001845Cu + 8); require(hook(End, c), "end"); }

    // WholeRead: completed CD DMA of whole stock sectors only.
    dma_writes = 0;
    { CPUState c = call(0x800183C8u, 0x80120000u, Disc + 3, 2);
      require(hook(WholeRead, c) && c.gpr[2] == 0x80120000u && dma_writes == 1 &&
              !std::memcmp(at(0x80120000u), &disc[3 * 2048], 4096), "whole read served"); }
    { CPUState c = call(0x800183C8u, 0x80120000u, Disc + 3, 3); require(!hook(WholeRead, c), "span past file"); }
    { CPUState c = call(0x800183C8u, 0x80120000u, Disc + 5, 1); require(!hook(WholeRead, c), "modified file"); }
    { CPUState c = call(0x800183C8u, 0x80120002u, Disc, 1); require(!hook(WholeRead, c), "unaligned DMA"); }
    { CPUState c = call(0x800183C8u, 0x80120000u, 16, 1); require(!hook(WholeRead, c), "PVD stays on CD"); }
    { CPUState c = call(0x800183C8u, 0x807FF800u, Disc, 2); require(!hook(WholeRead, c), "RAM bound"); }

    // Unavailable pack: every hook is inert.
    fail_prepare = true; activate();
    require(!open(Disc), "no pack, no service");
    std::puts("v82 resident contract tests passed");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL: %s\n", e.what());
    return 1;
}
