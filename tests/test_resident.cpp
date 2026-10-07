// Resident Loading contract: the adapter against a transliteration of the
// original stream loader (80018148..80018708) with synchronous sector
// delivery, plus chaining, identity, fallback, snapshot and WholeRead rules,
// and the load floor shims A-E (v82_resident.h) with the texture decoder
// checked against an independent LZSS reference.
#include "../src/mods/v82_resident.cpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>
using namespace v82_resident;

struct FakeDerived { uint32_t file, tag, meta[4]; std::vector<uint8_t> bytes; };
struct PSXResidentPack {
    std::vector<std::vector<uint8_t>> files;  // sector-padded
    std::vector<uint32_t> lba, stock;
    std::vector<FakeDerived> derived;
};
struct PSXResidentSink { PSXResidentPack* pack; };

namespace {
std::vector<uint8_t> ram(0x800000);
std::array<uint8_t, 64> mod_memory{};
std::array<uint8_t, 4> spu_delay_reg{};
PSXResidentPack fake;
bool contract_ok = true, finished = false, fail_prepare = false;
unsigned dma_writes = 0, host_writes = 0;
struct GuestCall { uint32_t function, a0; };
std::vector<GuestCall> guest_calls;
struct UnchargedCall { uint32_t function, ra, a[4], budget; bool nested_ran_body; };
std::vector<UnchargedCall> uncharged_calls;
uint32_t uncharged_result = 0;
std::vector<uint16_t> vram(1024 * 512);
unsigned load_images = 0;
int fail_image_after = -1;
struct SpuUpload { uint32_t address, source, bytes; int stop; };
std::vector<SpuUpload> spu_uploads;
uint8_t* at(uint32_t a) {
    if ((a & 0x1FFFFFFFu) == 0x1F801014u) return spu_delay_reg.data();
    if (a >= 0x9F000000u && a < 0x9F000000u + mod_memory.size()) return &mod_memory[a - 0x9F000000u];
    if ((a & 0x1FFFFFFFu) >= ram.size()) throw std::runtime_error("guest access out of RAM");
    return &ram[a & 0x1FFFFFFFu];
}
void require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
}  // namespace

extern "C" {
uint8_t psx_mod_read_byte(uint32_t a) { return *at(a); }
uint16_t psx_mod_read_half(uint32_t a) { uint16_t v; std::memcpy(&v, at(a), 2); return v; }
void psx_mod_write_half(uint32_t a, uint16_t v) { std::memcpy(at(a), &v, 2); }
void psx_mod_write_byte(uint32_t a, uint8_t v) { *at(a) = v; }
uint32_t psx_mod_read_word(uint32_t a) { uint32_t v; std::memcpy(&v, at(a), 4); return v; }
void psx_mod_write_word(uint32_t a, uint32_t v) { std::memcpy(at(a), &v, 4); }
int psx_mod_finish_function(CPUState* c) { c->pc = c->gpr[31]; finished = true; return 1; }
void psx_mod_counter_add(const char*, uint32_t) {}
uint32_t psx_mod_call_guest(CPUState*, uint32_t f, uint32_t, uint32_t a0, uint32_t, uint32_t, uint32_t) {
    guest_calls.push_back({f, a0}); return 0;
}
// The runtime dispatches the callee, whose generated entry consults the
// function-entry hooks again: the allocator hook must let that body run.
uint32_t psx_mod_call_guest_uncharged(CPUState* c, uint32_t f, uint32_t ra, uint32_t a0, uint32_t a1,
                                      uint32_t a2, uint32_t a3, uint32_t budget, int* charged) {
    CPUState callee = *c;
    callee.gpr[4] = a0; callee.gpr[5] = a1; callee.gpr[6] = a2; callee.gpr[7] = a3; callee.gpr[31] = ra;
    const bool outer_finished = finished;
    finished = false;
    entry(&callee, f);
    uncharged_calls.push_back({f, ra, {a0, a1, a2, a3}, budget, !finished});
    finished = outer_finished;
    if (charged) *charged = 0;
    return uncharged_result;
}
int psx_mod_psyq_load_image(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t src) {
    if (!w || !h || x + w > 1024u || y + h > 512u || (src & 3u)) return 0;
    if (fail_image_after >= 0 && load_images == unsigned(fail_image_after)) return 0;
    ++load_images;
    for (uint32_t r = 0; r < h; ++r)
        for (uint32_t i = 0; i < w; ++i) vram[(y + r) * 1024u + x + i] = psx_mod_read_half(src + 2u * (r * w + i));
    return 1;
}
int psx_mod_spu_upload(uint32_t address, uint32_t source, uint32_t bytes, int stop) {
    spu_uploads.push_back({address, source, bytes, stop}); return 1;
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
int psx_resident_emit(PSXResidentSink* s, uint32_t file, uint32_t tag, const uint32_t meta[4],
                      const void* bytes, uint32_t size) {
    FakeDerived d{file, tag, {meta[0], meta[1], meta[2], meta[3]},
                  std::vector<uint8_t>(static_cast<const uint8_t*>(bytes), static_cast<const uint8_t*>(bytes) + size)};
    s->pack->derived.push_back(std::move(d));
    return 1;
}
uint32_t psx_resident_derived_count(const PSXResidentPack* p) { return p ? uint32_t(p->derived.size()) : 0; }
const uint8_t* psx_resident_derived(const PSXResidentPack* p, uint32_t i, uint32_t* file, uint32_t* tag,
                                    uint32_t meta[4], uint32_t* size) {
    if (!p || i >= p->derived.size()) return nullptr;
    const auto& d = p->derived[i];
    if (file) *file = d.file;
    if (tag) *tag = d.tag;
    if (meta) std::memcpy(meta, d.meta, sizeof d.meta);
    if (size) *size = uint32_t(d.bytes.size());
    return d.bytes.data();
}
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

// Independent reference for the texture format: a plain 2 KiB-ring LZSS
// (flag byte MSB first, 1 = literal; reference = 11-bit ring position and
// length - 2 in a little-endian half-word). No stack-buffer modelling.
bool reference_decode(const std::vector<uint8_t>& src, size_t total, std::vector<uint8_t>& out,
                      uint32_t& consumed) {
    out.clear();
    uint32_t flags = 0x100, i = 0;
    while (out.size() < total) {
        if (flags & 0x100u) {
            if (i >= src.size()) return false;
            flags = uint32_t(src[i++]) << 24 | 1u;
        }
        if (flags & 0x80000000u) {
            if (i >= src.size()) return false;
            out.push_back(src[i++]);
        } else {
            if (i + 2 > src.size()) return false;
            const uint32_t t = src[i] | uint32_t(src[i + 1]) << 8;
            i += 2;
            const uint32_t pos = uint32_t(out.size()) & 0x7FFu, from = t >> 5;
            const size_t distance = ((pos - from - 1u) & 0x7FFu) + 1u;
            if (distance > out.size()) return false;  // before the first byte
            const size_t base = out.size() - distance;
            for (uint32_t k = 0; k < (t & 31u) + 2u; ++k) out.push_back(out[base + k]);
        }
        flags <<= 1;
    }
    out.resize(total);
    consumed = i;
    return true;
}

// Random stream in the format above: literals and references to bytes
// already produced (distances 1..0x7FF), so the guest never reads its stack
// (distance 0x800 across the window end reads its stale overshoot bytes).
std::vector<uint8_t> encode_random(std::mt19937& rng, size_t total) {
    std::vector<uint8_t> out, plain;
    while (plain.size() < total) {
        const size_t flag_at = out.size();
        out.push_back(0);
        uint8_t flags = 0;
        for (int bit = 7; bit >= 0 && plain.size() < total; --bit) {
            if (plain.empty() || rng() % 3 == 0) {
                flags |= uint8_t(1u << bit);
                const uint8_t v = uint8_t(rng() % 7);  // few symbols: real repeats
                out.push_back(v);
                plain.push_back(v);
            } else {
                const size_t max_d = std::min<size_t>(plain.size(), 0x7FF);
                const size_t distance = 1 + rng() % max_d, length = 2 + rng() % 32;
                const uint32_t from = uint32_t((plain.size() - distance) & 0x7FFu);
                const uint32_t t = from << 5 | uint32_t(length - 2);
                out.push_back(uint8_t(t));
                out.push_back(uint8_t(t >> 8));
                const size_t base = plain.size() - distance;
                for (size_t k = 0; k < length; ++k) plain.push_back(plain[base + k]);
            }
        }
        out[flag_at] = flags;
    }
    for (int k = 0; k < 64; ++k) out.push_back(uint8_t(rng()));  // what follows in the chunk
    return out;
}

void put_be32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    for (int i = 0; i < 4; ++i) v[at + i] = uint8_t(x >> (24 - 8 * i));
}
void put_le32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    for (int i = 0; i < 4; ++i) v[at + i] = uint8_t(x >> (8 * i));
}
void put_le16(std::vector<uint8_t>& v, size_t at, uint16_t x) {
    v[at] = uint8_t(x);
    v[at + 1] = uint8_t(x >> 8);
}

// FORM TERR { FORM XOBF { BIN } }, the BIN carrying a texture table whose
// records are CLUT-then-compressed or compressed only (80020FF8's layout).
struct ExpTexture { int w, h; std::vector<uint8_t> stream; bool clut; };
std::vector<uint8_t> build_exp(const std::vector<ExpTexture>& tex, std::vector<uint32_t>& stream_at) {
    std::vector<uint8_t> bin(0x20, 0);
    const uint32_t table = 0x20;
    put_le32(bin, 16, uint32_t(tex.size()));
    put_le32(bin, 20, table);
    bin.resize(table + 4 * tex.size(), 0);
    std::vector<uint32_t> header_at;
    for (const auto& t : tex) {
        while (bin.size() & 3u) bin.push_back(0);
        header_at.push_back(uint32_t(bin.size()));
        if (t.clut) {
            const size_t c = bin.size();
            bin.resize(c + 0x14 + 32, 0);
            put_le32(bin, c + 4, 0x18u);
            put_le32(bin, c + 8, 0x14u + 32u);
            put_le16(bin, c + 0x10, 16);
            put_le16(bin, c + 0x12, 1);
        }
        const size_t h = bin.size();
        bin.resize(h + 0x14, 0);
        put_le32(bin, h + 4, t.clut ? 0x18u : 0x10u);
        put_le32(bin, h + 8, uint32_t(0xC + t.stream.size()));
        put_le16(bin, h + 0x10, uint16_t(t.w));
        put_le16(bin, h + 0x12, uint16_t(t.h));
        stream_at.push_back(uint32_t(h + 0x14));
        bin.insert(bin.end(), t.stream.begin(), t.stream.end());
    }
    for (size_t i = 0; i < tex.size(); ++i) put_le32(bin, table + 4 * i, header_at[i] - table);
    if (bin.size() & 1u) bin.push_back(0);
    std::vector<uint8_t> exp(32, 0);
    std::memcpy(&exp[0], "FORM", 4);
    std::memcpy(&exp[8], "TERR", 4);
    std::memcpy(&exp[12], "FORM", 4);
    std::memcpy(&exp[20], "XOBF", 4);
    std::memcpy(&exp[24], "BIN ", 4);
    put_be32(exp, 28, uint32_t(bin.size()));
    for (auto& a : stream_at) a += 32;
    exp.insert(exp.end(), bin.begin(), bin.end());
    put_be32(exp, 16, uint32_t(exp.size() - 20));
    put_be32(exp, 4, uint32_t(exp.size() - 8));
    while (exp.size() % 2048u) exp.push_back(0);
    return exp;
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

    // ---- Load floor ----
    const uint32_t Sp = 0x801FE000u;
    auto floor_call = [](uint32_t ra, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0) {
        CPUState c = call(ra, a0, a1, a2);
        c.gpr[29] = Sp;
        return c;
    };
    // A: the arena load's wipe wait finds a running wipe finished.
    psx_mod_write_word(Gp + GpWipeState, 1);
    { CPUState c = floor_call(ArenaWipeWaitReturn);
      require(!hook(WipeWait, c) && psx_mod_read_word(Gp + GpWipeState) == 2, "wipe ended, body runs"); }
    psx_mod_write_word(Gp + GpWipeState, 1);
    { CPUState c = floor_call(0x80021D20u);
      require(!hook(WipeWait, c), "other caller");
      c = floor_call(ArenaWipeWaitReturn, 1);
      require(!hook(WipeWait, c), "a0 != 0 does not wait");
      require(psx_mod_read_word(Gp + GpWipeState) == 1, "menu wipes keep their length"); }
    psx_mod_write_word(Gp + GpWipeState, 0);
    { CPUState c = floor_call(ArenaWipeWaitReturn);
      hook(WipeWait, c);
      require(psx_mod_read_word(Gp + GpWipeState) == 0, "no wipe, nothing to end"); }

    // B: the five VSync(0) calls of per-object setup under the EXP loader.
    psx_mod_write_word(Sp + ObjectSetupFrameRa, ExpChunkObjectSetupReturn);
    for (uint32_t ra : ObjectSetupVSyncReturns) {
        CPUState c = floor_call(ra);
        c.gpr[2] = 0x1234;
        require(hook(VSync, c) && c.gpr[2] == 0 && c.pc == ra, "object-setup VSync returns at once");
    }
    { CPUState c = floor_call(0x80013ACCu);
      require(!hook(VSync, c), "gameplay VSync waits");
      c = floor_call(ObjectSetupVSyncReturns[0], 1);
      require(!hook(VSync, c), "VSync(1) is not skipped");
      psx_mod_write_word(Sp + ObjectSetupFrameRa, 0x8002BA00u);
      c = floor_call(ObjectSetupVSyncReturns[0]);
      require(!hook(VSync, c), "setup outside an EXP load waits");
      psx_mod_write_word(Sp + ObjectSetupFrameRa, ExpChunkObjectSetupReturn); }

    // C: texture-loader allocations run the original allocator, uncharged.
    uncharged_result = 0x8010ABC0u;
    for (uint32_t ra : {TextureClutAllocReturn, TextureImageAllocReturn}) {
        uncharged_calls.clear();
        CPUState c = floor_call(ra, 36, 53, 0x40);
        c.gpr[7] = 0x100;
        require(hook(RectAlloc, c) && c.gpr[2] == 0x8010ABC0u, "allocation finished with the guest result");
        require(uncharged_calls.size() == 1 && uncharged_calls[0].function == RectAlloc &&
                    uncharged_calls[0].ra == ra && uncharged_calls[0].a[0] == 36 &&
                    uncharged_calls[0].a[1] == 53 && uncharged_calls[0].a[2] == 0x40 &&
                    uncharged_calls[0].a[3] == 0x100 && uncharged_calls[0].budget == AllocBudgetCycles,
                "uncharged call of the original");
        require(uncharged_calls[0].nested_ran_body, "the allocator's own entry runs its body");
    }
    uncharged_calls.clear();
    { CPUState c = floor_call(0x80021380u);
      require(!hook(RectAlloc, c) && uncharged_calls.empty(), "other allocations keep guest time"); }

    // D: the decoder model against the independent reference first.
    std::mt19937 tr(1999);
    for (int round = 0; round < 300; ++round) {
        const int w = 1 + int(tr() % 64), h = 1 + int(tr() % 96);
        const auto stream = encode_random(tr, size_t(w) * size_t(h) * 2u);
        std::vector<uint8_t> want, got;
        uint32_t c1 = 0, c2 = 0;
        require(reference_decode(stream, size_t(w) * size_t(h) * 2u, want, c1), "reference decodes");
        require(v82_lzss::decode(stream.data(), uint32_t(stream.size()), w, h, got, c2), "model decodes");
        // The guest's token loop is a do-while: when the previous window's
        // overshoot already holds the last rows it still reads one token
        // (at most a flag byte and a reference) the reference never needs.
        require(got == want && c2 >= c1 && c2 <= c1 + 3, "model output matches the reference");
    }
    { // A reference before the first byte reads the guest's stack: refused.
      const std::vector<uint8_t> bad = {0x00, 0x00, 0x7F, 0, 0, 0, 0, 0, 0, 0, 0};
      std::vector<uint8_t> got;
      uint32_t c = 0;
      require(!v82_lzss::decode(bad.data(), uint32_t(bad.size()), 4, 4, got, c), "stack read refused"); }

    std::vector<ExpTexture> tex = {{36, 53, encode_random(tr, 36 * 53 * 2), true},
                                   {15, 80, encode_random(tr, 15 * 80 * 2), false},
                                   {64, 64, encode_random(tr, 64 * 64 * 2), true}};
    std::vector<uint32_t> stream_at;
    const auto exp = build_exp(tex, stream_at);
    const uint32_t ExpFile = 3;
    fake.files[ExpFile] = exp;
    fake.lba[ExpFile] = 0x6000u;
    fake.derived.clear();
    { PSXResidentSink sink{&fake};
      for (uint32_t f = 0; f < 3; ++f)
          require(derive_textures(&sink, f, fake.files[f].data(), uint32_t(fake.files[f].size()), 1, nullptr),
                  "non-EXP files derive");
      require(fake.derived.empty(), "no textures outside EXP files");
      require(derive_textures(&sink, ExpFile, exp.data(), uint32_t(exp.size()), 1, nullptr), "derive");
      require(fake.derived.size() == 3, "every compressed texture pre-decoded"); }
    index_textures();
    const uint32_t Rect = 0x80110000u, Src = 0x80120000u;
    for (size_t t = 0; t < tex.size(); ++t) {
        std::vector<uint8_t> want;
        uint32_t consumed = 0;
        require(reference_decode(tex[t].stream, size_t(tex[t].w) * size_t(tex[t].h) * 2u, want, consumed), "ref");
        const auto& d = fake.derived[t];
        require(d.tag == TextureTag && d.meta[1] == stream_at[t] && d.meta[2] >= consumed &&
                    d.meta[2] <= consumed + 3 && d.bytes == want,
                "blob is the decoded image");
        consumed = d.meta[2];  // every byte the guest decoder reads
        const int x = 512 + int(t) * 70, y = 100 + int(t) * 3;
        psx_mod_write_half(Rect, uint16_t(x));
        psx_mod_write_half(Rect + 2, uint16_t(y));
        psx_mod_write_half(Rect + 4, uint16_t(tex[t].w));
        psx_mod_write_half(Rect + 6, uint16_t(tex[t].h));
        std::memcpy(at(Src), &exp[stream_at[t]], consumed);
        std::fill(vram.begin(), vram.end(), uint16_t(0xDEAD));
        guest_calls.clear();
        CPUState c = floor_call(TextureDecodeReturn, Rect, Src);
        c.gpr[2] = 7;
        require(hook(Lzss, c) && c.gpr[2] == 0, "texture served");
        require(guest_calls.size() == 1 && guest_calls[0].function == DrawSync && guest_calls[0].a0 == 0,
                "GPU queue drained first");
        for (int r = 0; r < tex[t].h; ++r)
            for (int i = 0; i < tex[t].w; ++i) {
                const size_t o = (size_t(r) * size_t(tex[t].w) + size_t(i)) * 2u;
                require(vram[size_t(y + r) * 1024u + size_t(x + i)] == uint16_t(want[o] | want[o + 1] << 8),
                        "VRAM holds the decoded texture");
            }
        require(vram[size_t(y) * 1024u + size_t(x - 1)] == 0xDEAD &&
                    vram[size_t(y + tex[t].h) * 1024u + size_t(x)] == 0xDEAD,
                "nothing outside the rectangle");
        // One compressed byte the guest would read differs: the game decodes.
        psx_mod_write_byte(Src + consumed - 1, uint8_t(psx_mod_read_byte(Src + consumed - 1) ^ 0x5A));
        c = floor_call(TextureDecodeReturn, Rect, Src);
        require(!hook(Lzss, c), "changed stream decoded by the game");
        psx_mod_write_byte(Src + consumed - 1, uint8_t(psx_mod_read_byte(Src + consumed - 1) ^ 0x5A));
    }
    { CPUState c = floor_call(0x80021420u, Rect, Src);
      require(!hook(Lzss, c), "table textures keep the guest decode");
      psx_mod_write_half(Rect + 4, 63);
      c = floor_call(TextureDecodeReturn, Rect, Src);
      require(!hook(Lzss, c), "size must match the blob"); }

    // E: a DMA-mode sound-bank chunk completes synchronously.
    auto spu_state = [](uint32_t callback, uint32_t io) {
        psx_mod_write_word(SpuIoMode, io);
        psx_mod_write_word(SpuCallback, callback);
        psx_mod_write_word(SpuAddrShift, 3);
        psx_mod_write_half(SpuTransferAddr, 0x0202);
        psx_mod_write_word(SpuDelayPtr, 0xBF801014u);
        psx_mod_write_word(0x1F801014u, 0x0E0F1234u);
        psx_mod_write_word(SpuDone, 0);
        psx_mod_write_word(SpuTMode, 7);
    };
    spu_state(0, 0);
    spu_uploads.clear();
    { CPUState c = floor_call(BankChunkWriteReturn, 0x80101000u, 0x2A0u);
      require(hook(SpuWrite, c) && c.gpr[2] == 0x2A0u, "chunk written");
      require(spu_uploads.size() == 1 && spu_uploads[0].address == 0x1010u &&
                  spu_uploads[0].source == 0x80101000u && spu_uploads[0].bytes == 11u * 64u &&
                  !spu_uploads[0].stop,
              "whole DMA blocks at the transfer address");
      require(psx_mod_read_word(SpuDone) == 1 && psx_mod_read_word(SpuTMode) == 0 &&
                  psx_mod_read_word(SpuDmaSource) == 0x80101000u && psx_mod_read_word(SpuDmaBlocks) == 11 &&
                  psx_mod_read_word(0x1F801014u) == 0x200F1234u,
              "libspu bookkeeping and SPU_DELAY"); }
    spu_state(0x80040000u, 0);
    spu_uploads.clear();
    { CPUState c = floor_call(BankChunkWriteReturn, 0x80101000u, 0x400u);
      require(!hook(SpuWrite, c) && spu_uploads.empty(), "a transfer callback keeps the DMA"); }
    spu_state(0, 1);
    { CPUState c = floor_call(BankChunkWriteReturn, 0x80101000u, 0x400u);
      require(!hook(SpuWrite, c) && spu_uploads.empty(), "I/O transfer mode keeps the original"); }
    spu_state(0, 0);
    { CPUState c = floor_call(0x80053900u, 0x80101000u, 0x400u);
      require(!hook(SpuWrite, c) && spu_uploads.empty(), "other SpuWrite callers keep the DMA"); }

    // The code contract and the debug switch gate the shims.
    contract_ok = false;
    psx_mod_write_word(Gp + GpWipeState, 1);
    { CPUState c = floor_call(ArenaWipeWaitReturn);
      hook(WipeWait, c);
      require(psx_mod_read_word(Gp + GpWipeState) == 1, "contract gates A");
      c = floor_call(ObjectSetupVSyncReturns[0]);
      require(!hook(VSync, c), "contract gates B");
      c = floor_call(TextureImageAllocReturn);
      require(!hook(RectAlloc, c), "contract gates C");
      psx_mod_write_half(Rect + 4, uint16_t(tex[0].w));
      std::memcpy(at(Src), &exp[stream_at[0]], 64);
      c = floor_call(TextureDecodeReturn, Rect, Src);
      require(!hook(Lzss, c), "contract gates D");
      c = floor_call(BankChunkWriteReturn, 0x80101000u, 0x400u);
      require(!hook(SpuWrite, c), "contract gates E"); }
    contract_ok = true;
    floor_on = false;
    { CPUState c = floor_call(ArenaWipeWaitReturn);
      hook(WipeWait, c);
      require(psx_mod_read_word(Gp + GpWipeState) == 1, "switch gates A");
      c = floor_call(ObjectSetupVSyncReturns[0]);
      require(!hook(VSync, c), "switch gates B");
      c = floor_call(BankChunkWriteReturn, 0x80101000u, 0x400u);
      require(!hook(SpuWrite, c), "switch gates E"); }
    floor_on = true;

    // A rejected second chunk leaves caller inputs and continuation intact.
    // Drive an independent decoder fallback from those preserved inputs and
    // verify the complete rectangle, including its idempotent prefix upload.
    { const auto& t = tex[0];
      psx_mod_write_half(Rect, 512); psx_mod_write_half(Rect + 2, 100);
      psx_mod_write_half(Rect + 4, uint16_t(t.w)); psx_mod_write_half(Rect + 6, uint16_t(t.h));
      std::memcpy(at(Src), &exp[stream_at[0]], fake.derived[0].meta[2]);
      std::fill(vram.begin(), vram.end(), uint16_t(0xDEAD));
      load_images = 0; fail_image_after = 1; guest_calls.clear();
      CPUState c = floor_call(TextureDecodeReturn, Rect, Src);
      c.gpr[2] = 0x12345678u;
      const CPUState before = c;
      require(!hook(Lzss, c) && load_images == 1 && c.gpr[2] == 0x12345678u,
              "partial texture upload keeps original continuation");
      require(!std::memcmp(c.gpr, before.gpr, sizeof c.gpr) && c.pc == before.pc &&
                  c.hi == before.hi && c.lo == before.lo,
              "native rejection preserves caller registers and continuation");
      require(!std::memcmp(at(Src), &exp[stream_at[0]], fake.derived[0].meta[2]) &&
                  psx_mod_read_half(Rect) == 512 && psx_mod_read_half(Rect + 2) == 100 &&
                  psx_mod_read_half(Rect + 4) == t.w && psx_mod_read_half(Rect + 6) == t.h,
              "native rejection preserves compressed source and rectangle");
      require(guest_calls.size() == 1 && guest_calls[0].function == DrawSync,
              "queued GPU work completes before the native prefix");
      fail_image_after = -1;
      std::vector<uint8_t> fallback;
      uint32_t consumed = 0;
      const std::vector<uint8_t> input(at(c.gpr[5]), at(c.gpr[5]) + fake.derived[0].meta[2]);
      require(reference_decode(input, size_t(t.w) * size_t(t.h) * 2u, fallback, consumed),
              "independent fallback decodes preserved input");
      for (int row = 0; row < t.h; ++row)
          for (int x = 0; x < t.w; ++x) {
              const size_t offset = (size_t(row) * t.w + x) * 2u;
              vram[size_t(100 + row) * 1024u + 512u + x] = uint16_t(fallback[offset] | fallback[offset + 1] << 8);
          }
      require(fallback == fake.derived[0].bytes, "fallback completes the intended texture");
      for (int row = 0; row < t.h; ++row)
          for (int x = 0; x < t.w; ++x) {
              const size_t offset = (size_t(row) * t.w + x) * 2u;
              require(vram[size_t(100 + row) * 1024u + 512u + x] ==
                          uint16_t(fake.derived[0].bytes[offset] | fake.derived[0].bytes[offset + 1] << 8),
                      "complete rectangle after native-prefix and fallback");
          }
      require(vram[100u * 1024u + 511u] == 0xDEAD &&
                  vram[size_t(100 + t.h) * 1024u + 512u] == 0xDEAD,
              "repeated upload leaves pixels outside the rectangle intact");
      const uint32_t alias = Sp - LzssFrame + 0x10u;
      std::memcpy(at(alias), &exp[stream_at[0]], fake.derived[0].meta[2]);
      load_images = 0; guest_calls.clear();
      c = floor_call(TextureDecodeReturn, Rect, alias);
      require(!hook(Lzss, c) && load_images == 0 && guest_calls.empty(),
              "staging alias declines before GPU or guest-stack work");
      std::memcpy(at(alias), at(Rect), 8u);
      c = floor_call(TextureDecodeReturn, alias, Src);
      require(!hook(Lzss, c) && load_images == 0 && guest_calls.empty(),
              "rectangle alias declines before GPU or guest-stack work"); }

    // Unavailable pack: every hook is inert.
    fail_prepare = true; activate();
    require(!open(Disc), "no pack, no service");
    std::puts("v82 resident contract tests passed");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL: %s\n", e.what());
    return 1;
}
