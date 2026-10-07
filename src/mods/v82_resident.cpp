/* SLUS-00868 Resident Loading adapter; contract in v82_resident.h. Guest CD
 * timing, movies, XA and CD audio are untouched: supported loader calls
 * complete from verified resident sectors, everything else runs the game's
 * original loader. The load floor shims (A-E in the header) take the guest
 * time out of the arena load's waits and CPU work. */
#include "v82_resident.h"
#include "v82_lzss.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include "mod_resident.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace v82_resident {
namespace {

struct V82ResidentEntry { const char* path; uint32_t lba, size; const char* sha256; };
#include "v82_resident_catalog.inc"

constexpr auto Plugin = "v82.resident";
constexpr uint32_t FileCount = sizeof kCatalog / sizeof kCatalog[0];
constexpr uint32_t NoFile = UINT32_MAX;

PSXResidentFile files[FileCount];
const PSXResidentPack* pack = nullptr;
uint32_t record = 0;      /* guest address of the stream record */
bool observe = false;     /* contract fixture: record calls without serving */
bool forcing_cd = false;  /* a fallback is starting the original CD stream */
bool floor_on = true;     /* contract fixture can isolate the five load shims */
bool in_uncharged = false; /* the allocator's own entry inside its uncharged call */
/* Pre-decoded textures by (size, first stream bytes): derived blob indexes.
 * Built from the pack alone, so both netplay peers hold the same index. */
std::unordered_multimap<uint64_t, uint32_t> texture_index;

uint32_t rd(uint32_t a) { return psx_mod_read_word(a); }
void wr(uint32_t a, uint32_t v) { psx_mod_write_word(a, v); }

bool ram_span(uint32_t address, uint32_t bytes) {
    const uint32_t phys = address & 0x1FFFFFFFu;
    const uint32_t limit = 0x00800000u; /* 8 MiB when a content mod expands RAM */
    return address >= 0x80000000u && address < 0xA0000000u && phys >= 0x10000u &&
           phys < limit && bytes <= limit - phys;
}

/* The loader text the adapter reproduces and every function the load floor
 * relies on, hashed from live guest RAM. */
bool contract() {
    if (psx_resident_guest_ranges_match(kContract, sizeof kContract / sizeof kContract[0],
                                        kContractSha256))
        return true;
    psx_mod_counter_add("v82.resident.contract_mismatch", 1);
    return false;
}

bool floor_ok() { return floor_on && !observe && contract(); }

/* `bytes` at file-relative byte `offset` of pack file `file`, or null. */
const uint8_t* file_bytes(uint32_t file, uint64_t offset, uint64_t bytes) {
    uint32_t padded = 0;
    const uint8_t* data = psx_resident_file(pack, file, nullptr, &padded);
    if (!data || offset > padded || bytes > padded - offset) return nullptr;
    return data + offset;
}

bool active(uint32_t gp) {
    return record && rd(record) == RecordMagic && rd(record + 12) == gp &&
           rd(gp + GpBase) == rd(record + 8) && rd(gp + GpNextLba) != 0 &&
           rd(record + 4) < FileCount;
}

void finish(CPUState* c, uint32_t v0) {
    c->gpr[2] = v0;
    psx_mod_finish_function(c);
}

/* Keep the game's sector-buffer invariant: when the position is inside a
 * sector, D58 points at a slot holding that sector, exactly what Wait would
 * have delivered. The slot fill is a completed CD DMA. */
bool hold_sector(uint32_t gp, uint32_t file, uint32_t base, uint32_t sector_index) {
    const uint32_t lba = base + sector_index;
    if (rd(record + 16) != lba) {
        const uint8_t* sector = file_bytes(file, uint64_t(sector_index) * 2048u, 2048u);
        if (!sector || !psx_mod_dma_write_ram(Ring, sector, 2048u, int(lba))) return false;
        wr(record + 16, lba);
    }
    wr(gp + GpSector, Ring);
    return true;
}

/* Settle position-derived stream state after a served call. */
bool settle(uint32_t gp, uint32_t file, uint32_t base, uint32_t position) {
    if ((position & 2047u) && !hold_sector(gp, file, base, position >> 11)) return false;
    wr(gp + GpPosition, position);
    wr(gp + GpNextLba, base + ((position + 2047u) >> 11));
    return true;
}

/* A request the resident file cannot satisfy (past its last sector): start
 * the original CD stream at the next unread sector and let the original body
 * run. The current partial sector is already in the slot D58 names. */
void fall_back(CPUState* c, uint32_t gp, const char* why) {
    const uint32_t base = rd(record + 8), position = rd(gp + GpPosition);
    const uint32_t file = rd(record + 4), bytes = rd(record + 20);
    wr(record, 0);
    forcing_cd = true;
    psx_mod_call_guest(c, Begin, c->gpr[31], base + ((position + 2047u) >> 11), 0, 0, 0);
    forcing_cd = false;
    psx_mod_counter_add(why, 1);
    psx_resident_record("v82.resident.stream_fallback", file, base + (position >> 11), bytes, 0);
}

void on_begin(CPUState* c, uint32_t gp) {
    const uint32_t lba = c->gpr[4], base = rd(gp + GpBase);
    if (forcing_cd) return;
    if (record) wr(record, 0);
    uint32_t file = NoFile;
    const bool found = psx_resident_find_lba(pack, lba, 2048u, &file) != nullptr;
    /* Only Open's call starts a served stream: Open stored the resolved
     * descriptor's first sector as the base and resets the position after
     * Begin returns. Seek's restart and the directory walker continue with
     * the original Wait loop, which needs the drive, so they keep the CD. */
    const bool serve = !observe && found && c->gpr[31] == OpenBeginReturn &&
                       psx_resident_file_stock(pack, file) &&
                       base == psx_resident_file_lba(pack, file) && lba == base &&
                       ram_span(gp, 0xD70u) && contract();
    if (!serve) {
        if (found && !observe) psx_mod_counter_add("v82.resident.begin_declined", 1);
        psx_resident_record("v82.resident.begin", found ? file : NoFile, lba, 0, 0);
        return;
    }
    wr(record + 4, file); wr(record + 8, base); wr(record + 12, gp);
    wr(record + 16, 0); wr(record + 20, 0);
    wr(record, RecordMagic);
    wr(gp + GpConsumed, Ring); wr(gp + GpFilled, Ring);
    wr(gp + GpNextLba, lba);
    psx_mod_write_byte(gp + GpCdState, 0xFF);
    finish(c, 0xFF);
    psx_mod_counter_add("v82.resident.streams", 1);
    psx_resident_record("v82.resident.begin", file, lba, 0, 1);
}

void on_end(CPUState* c, uint32_t gp) {
    if (!active(gp)) {
        if (observe && record) psx_resident_record("v82.resident.end", NoFile, rd(gp + GpNextLba), 0, 0);
        return;
    }
    const uint32_t file = rd(record + 4), bytes = rd(record + 20);
    wr(record, 0);
    wr(gp + GpNextLba, 0);
    finish(c, 0);
    psx_resident_record("v82.resident.end", file, psx_resident_file_lba(pack, file), bytes, 1);
}

void on_read(CPUState* c, uint32_t gp) {
    if (!active(gp)) return;
    const uint32_t dest = c->gpr[4], file = rd(record + 4), base = rd(record + 8);
    const int32_t length = int32_t(c->gpr[5]);
    const uint32_t position = rd(gp + GpPosition);
    const uint64_t offset = uint64_t(base - psx_resident_file_lba(pack, file)) * 2048u + position;
    const uint8_t* bytes = length >= 0 ? file_bytes(file, offset, uint32_t(length)) : nullptr;
    if (!bytes) { fall_back(c, gp, "v82.resident.read_fallback"); return; }
    if (length && (!ram_span(dest, uint32_t(length)) ||
                   !psx_mod_host_write_ram(dest, bytes, uint32_t(length)))) {
        psx_mod_counter_add("v82.resident.read_bad_destination", 1);
        fall_back(c, gp, "v82.resident.read_fallback");
        return;
    }
    if (!settle(gp, file, base, position + uint32_t(length))) {
        fall_back(c, gp, "v82.resident.read_fallback");
        return;
    }
    wr(record + 20, rd(record + 20) + uint32_t(length));
    finish(c, 1);
}

void on_read_byte(CPUState* c, uint32_t gp) {
    if (!active(gp)) return;
    const uint32_t file = rd(record + 4), base = rd(record + 8), position = rd(gp + GpPosition);
    const uint64_t offset = uint64_t(base - psx_resident_file_lba(pack, file)) * 2048u + position;
    const uint8_t* byte = file_bytes(file, offset, 1u);
    if (!byte || !hold_sector(gp, file, base, position >> 11)) {
        fall_back(c, gp, "v82.resident.read_byte_fallback");
        return;
    }
    wr(gp + GpPosition, position + 1u);
    wr(gp + GpNextLba, base + ((position + 1u + 2047u) >> 11));
    wr(record + 20, rd(record + 20) + 1u);
    finish(c, *byte);
}

void on_seek(CPUState* c, uint32_t gp) {
    if (!active(gp)) return;
    const uint32_t file = rd(record + 4), base = rd(record + 8), position = rd(gp + GpPosition);
    const int64_t target = c->gpr[5] ? int64_t(int32_t(position)) + int32_t(c->gpr[4])
                                     : int64_t(int32_t(c->gpr[4]));
    const uint64_t start = uint64_t(base - psx_resident_file_lba(pack, file)) * 2048u;
    if (target < 0 || target > INT32_MAX || !file_bytes(file, start + uint64_t(target), 0) ||
        !settle(gp, file, base, uint32_t(target))) {
        fall_back(c, gp, "v82.resident.seek_fallback");
        return;
    }
    finish(c, rd(gp + GpSector));
}

void on_whole_read(CPUState* c, uint32_t gp) {
    const uint32_t dest = c->gpr[4], lba = c->gpr[5], sectors = c->gpr[6];
    uint32_t file = NoFile;
    const uint8_t* data = sectors && sectors <= 0x1000u
        ? psx_resident_find_lba(pack, lba, sectors * 2048u, &file) : nullptr;
    const bool serve = !observe && data && psx_resident_file_stock(pack, file) && !(dest & 3u) &&
                       ram_span(dest, sectors * 2048u) && ram_span(gp, 0xD70u) && contract();
    if (!serve) {
        if (data && !observe) psx_mod_counter_add("v82.resident.whole_read_declined", 1);
        psx_resident_record("v82.resident.whole_read", data ? file : NoFile, lba, sectors * 2048u, 0);
        return;
    }
    if (!psx_mod_dma_write_ram(dest, data, sectors * 2048u, int(lba))) {
        psx_mod_counter_add("v82.resident.whole_read_declined", 1);
        return;
    }
    psx_mod_write_byte(gp + GpCdState, 0xFF);
    finish(c, dest);
    psx_mod_counter_add("v82.resident.whole_reads", 1);
    psx_resident_record("v82.resident.whole_read", file, lba, sectors * 2048u, 1);
}

/* ---- Load floor (header A-E) ---- */

/* A: the arena load waits for the loading-screen wipe after its loader is
 * done. Mark a running wipe finished; the original body then restores the
 * VSync callback and draw environment as it would after the last frame. */
void on_wipe_wait(CPUState* c, uint32_t gp) {
    if (c->gpr[31] != ArenaWipeWaitReturn || c->gpr[4] != 0 || !ram_span(gp, 0xEA0u) ||
        rd(gp + GpWipeState) != 1u || !floor_ok())
        return;
    wr(gp + GpWipeState, 2u);
    psx_mod_counter_add("v82.resident.wipe_ended", 1);
    psx_resident_record("v82.resident.wipe_end", NoFile, 0, 0, 1);
}

/* B: the five VSync(0) calls of per-object setup during an EXP load. */
void on_vsync(CPUState* c, uint32_t) {
    const uint32_t ra = c->gpr[31];
    bool site = false;
    for (uint32_t r : ObjectSetupVSyncReturns) site |= ra == r;
    if (!site || c->gpr[4] != 0) return;
    const uint32_t frame = c->gpr[29] + ObjectSetupFrameRa;
    if (!ram_span(frame, 4u) || rd(frame) != ExpChunkObjectSetupReturn || !floor_ok()) return;
    psx_mod_counter_add("v82.resident.vsync_skipped", 1);
    finish(c, 0);  /* the result is never read (8001E984 reloads v0) */
}

/* C: the EXP texture loader's VRAM allocations, in zero guest time. */
void on_rect_alloc(CPUState* c, uint32_t) {
    if (in_uncharged) return;  /* the uncharged call itself: run the body */
    const uint32_t ra = c->gpr[31];
    if ((ra != TextureClutAllocReturn && ra != TextureImageAllocReturn) || !floor_ok()) return;
    int charged = 0;
    in_uncharged = true;
    const uint32_t v0 = psx_mod_call_guest_uncharged(c, RectAlloc, ra, c->gpr[4], c->gpr[5],
                                                     c->gpr[6], c->gpr[7], AllocBudgetCycles,
                                                     &charged);
    in_uncharged = false;
    psx_mod_counter_add(charged ? "v82.resident.alloc_over_budget" : "v82.resident.alloc_uncharged", 1);
    finish(c, v0);
}

uint64_t texture_key(int w, int h, const uint8_t head[8]) {
    uint64_t k = 0xCBF29CE484222325ull;
    auto mix = [&](uint8_t b) { k = (k ^ b) * 0x100000001B3ull; };
    mix(uint8_t(w)); mix(uint8_t(w >> 8)); mix(uint8_t(h)); mix(uint8_t(h >> 8));
    for (int i = 0; i < 8; ++i) mix(head[i]);
    return k;
}

/* Pack derive callback: every compressed EXP texture, decoded exactly as the
 * guest would. Textures the model refuses are left to the game. */
int derive_textures(PSXResidentSink* sink, uint32_t file, const uint8_t* data, uint32_t size,
                    int, void*) {
    if (file >= FileCount || size < 12u || std::memcmp(data, "FORM", 4)) return 1;
    std::vector<uint8_t> pixels;
    for (const auto& t : v82_lzss::textures(data, size)) {
        uint32_t consumed = 0;
        if (t.avail < 8u ||
            !v82_lzss::decode(data + t.offset, t.avail, t.w, t.h, pixels, consumed))
            continue;
        const uint32_t meta[4] = {uint32_t(uint16_t(t.w)) | uint32_t(uint16_t(t.h)) << 16,
                                  t.offset, consumed, 0};
        if (!psx_resident_emit(sink, file, TextureTag, meta, pixels.data(), uint32_t(pixels.size())))
            return 0;
    }
    return 1;
}

void index_textures() {
    texture_index.clear();
    const uint32_t n = psx_resident_derived_count(pack);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t file = 0, tag = 0, meta[4] = {}, size = 0, padded = 0;
        if (!psx_resident_derived(pack, i, &file, &tag, meta, &size) || tag != TextureTag) continue;
        const uint8_t* data = psx_resident_file(pack, file, nullptr, &padded);
        if (!data || meta[1] > padded || padded - meta[1] < 8u) continue;
        texture_index.emplace(texture_key(int16_t(meta[0]), int16_t(meta[0] >> 16), data + meta[1]), i);
    }
}

/* The pre-decoded pixels for the stream at guest `src`, or null. The match
 * is exact: every compressed byte the guest decoder would read equals the
 * pack's, so its output is the blob. */
const uint8_t* find_texture(int w, int h, uint32_t src, uint32_t& file_out) {
    uint8_t head[8];
    if (!ram_span(src, 8u)) return nullptr;
    for (int i = 0; i < 8; ++i) head[i] = psx_mod_read_byte(src + uint32_t(i));
    const auto range = texture_index.equal_range(texture_key(w, h, head));
    for (auto it = range.first; it != range.second; ++it) {
        uint32_t file = 0, tag = 0, meta[4] = {}, size = 0, padded = 0;
        const uint8_t* pixels = psx_resident_derived(pack, it->second, &file, &tag, meta, &size);
        const uint8_t* data = psx_resident_file(pack, file, nullptr, &padded);
        if (!pixels || !data || int16_t(meta[0]) != w || int16_t(meta[0] >> 16) != h ||
            size != uint32_t(w) * uint32_t(h) * 2u || meta[1] > padded ||
            meta[2] > padded - meta[1] || !ram_span(src, meta[2]))
            continue;
        const uint8_t* want = data + meta[1];
        uint32_t i = 0;
        for (; i < meta[2]; ++i)
            if (psx_mod_read_byte(src + i) != want[i]) break;
        if (i != meta[2]) continue;
        file_out = file;
        return pixels;
    }
    return nullptr;
}

/* D: a texture the pack holds goes to VRAM without the guest decode. */
void on_lzss(CPUState* c, uint32_t) {
    if (c->gpr[31] != TextureDecodeReturn || texture_index.empty()) return;
    const uint32_t rect = c->gpr[4], src = c->gpr[5], sp = c->gpr[29];
    if (!ram_span(rect, 8u)) return;
    const int x = int16_t(psx_mod_read_half(rect)), y = int16_t(psx_mod_read_half(rect + 2));
    const int w = int16_t(psx_mod_read_half(rect + 4)), h = int16_t(psx_mod_read_half(rect + 6));
    /* Staging: the decoder's own window, below sp (the guest writes its
     * decoded rows there too). Chunks stay 0xC00 bytes so an odd-sized
     * upload's trailing half-word is still inside that frame. */
    const uint32_t staging = sp - LzssFrame + 0x10u, chunk_bytes = 0xC00u;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > 1024 || y + h > 512 ||
        uint32_t(w) * 2u > chunk_bytes || !ram_span(sp - LzssFrame, LzssFrame) || !floor_ok())
        return;
    uint32_t file = NoFile;
    const uint8_t* pixels = find_texture(w, h, src, file);
    if (!pixels) {
        psx_mod_counter_add("v82.resident.texture_unmatched", 1);
        psx_resident_record("v82.resident.texture", NoFile, src, uint32_t(w * h * 2), 0);
        return;
    }
    /* The game's queued GPU work (this texture's CLUT upload) comes first. */
    psx_mod_call_guest(c, DrawSync, c->gpr[31], 0, 0, 0, 0);
    const uint32_t row = uint32_t(w) * 2u, rows_per_chunk = chunk_bytes / row;
    for (int line = 0; line < h;) {
        const uint32_t rows = std::min<uint32_t>(rows_per_chunk, uint32_t(h - line));
        if (!psx_mod_host_write_ram(staging, pixels + size_t(line) * row, rows * row) ||
            !psx_mod_psyq_load_image(uint32_t(x), uint32_t(y + line), uint32_t(w), rows, staging)) {
            /* Do not publish completion for a partial upload. Let the
             * original decoder write the full rectangle from the unchanged
             * compressed source, replacing any already uploaded rows. */
            psx_mod_counter_add("v82.resident.texture_upload_failed", 1);
            return;
        }
        line += int(rows);
    }
    psx_mod_counter_add("v82.resident.textures_predecoded", 1);
    psx_resident_record("v82.resident.texture", file, src, uint32_t(w * h * 2), 1);
    finish(c, 0);  /* the decoder ends with v0 = 0 (800212AC) */
}

/* E: one sound-bank chunk by DMA, completed now. SpuWrite's own effects:
 * _spu_t(2) transfer address (RAM unchanged), _spu_t(1) DMA-write mode,
 * _spu_t(3) SPU_DELAY, DMA bookkeeping and whole 16-word blocks; then the
 * completion SpuIsTransferCompleted records once the DMA event arrives. */
void on_spu_write(CPUState* c, uint32_t) {
    if (c->gpr[31] != BankChunkWriteReturn) return;
    const uint32_t src = c->gpr[4];
    const uint32_t size = c->gpr[5] > SpuWriteMax ? SpuWriteMax : c->gpr[5];
    const uint32_t shift = rd(SpuAddrShift), delay = rd(SpuDelayPtr);
    const uint32_t blocks = (size >> 6) + ((size & 63u) != 0u);
    if (!size || (src & 3u) || rd(SpuIoMode) != 0u || rd(SpuCallback) != 0u || shift > 3u ||
        (delay & 0x1FFFFFFFu) != 0x1F801014u || !ram_span(src, blocks * 64u) || !floor_ok())
        return;
    const uint32_t address = uint32_t(psx_mod_read_half(SpuTransferAddr)) << shift;
    wr(SpuTMode, 0);
    psx_mod_write_word(delay, (psx_mod_read_word(delay) & 0xF0FFFFFFu) | 0x20000000u);
    wr(SpuDmaSource, src);
    wr(SpuDmaBlocks, blocks);
    if (!psx_mod_spu_upload(address, src, blocks * 64u, 0)) {
        /* Rejected before any SPU write; the original body redoes the
         * bookkeeping identically and runs the DMA. */
        psx_mod_counter_add("v82.resident.spu_declined", 1);
        return;
    }
    wr(SpuDone, 1u);
    psx_mod_counter_add("v82.resident.spu_chunks", 1);
    finish(c, size);
}

struct Hook { uint32_t address, word; void (*handler)(CPUState*, uint32_t); };
constexpr Hook Hooks[] = {
    {WholeRead, 0x27BDFFE8u, on_whole_read}, {Begin, 0x27BDFFE8u, on_begin},
    {End, 0x27BDFFF8u, on_end}, {Read, 0x8F820D64u, on_read},
    {ReadByte, 0x8F820D64u, on_read_byte}, {Seek, 0x27BDFFF0u, on_seek},
    {WipeWait, 0x8F820E98u, on_wipe_wait}, {VSync, 0x3C028006u, on_vsync},
    {RectAlloc, 0x27BDFFC8u, on_rect_alloc}, {Lzss, 0x27BDF390u, on_lzss},
    {SpuWrite, 0x27BDFFE8u, on_spu_write},
};

void entry(CPUState* c, uint32_t address) {
    if (!pack || !record) return;
    address |= 0x80000000u;
    for (const auto& h : Hooks) {
        if (h.address != address) continue;
        if (rd(address) != h.word) {
            psx_mod_counter_add("v82.resident.hook_word_mismatch", 1);
            return;
        }
        h.handler(c, c->gpr[28]);
        return;
    }
}

void activate() {
    pack = nullptr;
    record = 0;
    forcing_cd = false;
    observe = false;
    floor_on = true;
    in_uncharged = false;
    texture_index.clear();
    for (uint32_t i = 0; i < FileCount; ++i)
        files[i] = {kCatalog[i].path, kCatalog[i].size, kCatalog[i].sha256};
    PSXResidentSpec spec{};
    spec.struct_size = sizeof spec;
    spec.title = "Vigilante82PSXRecomp";
    spec.format = "slus00868-files-v2";  /* v2: pre-decoded textures */
    spec.files = files;
    spec.file_count = FileCount;
    /* Changed files stay in the pack but are never served (per-file stock
     * check at each request), so one modded asset keeps the rest resident. */
    spec.policy = PSX_RESIDENT_ALLOW_MODIFIED;
    spec.derive = derive_textures;
    pack = psx_resident_prepare(&spec);
    if (!pack) {
        psx_mod_counter_add("v82.resident.unavailable", 1);
        return;
    }
    record = psx_mod_alloc_guest_memory(RecordBytes, 4);
    if (!record) {
        pack = nullptr;
        psx_mod_counter_add("v82.resident.no_guest_memory", 1);
        return;
    }
    index_textures();
    psx_mod_counter_add("v82.resident.modified_files", psx_resident_modified_files(pack));
    psx_mod_counter_add("v82.resident.activated", 1);
}

}  // namespace
}  // namespace v82_resident

PSX_MOD_CONSTRUCTOR(register_v82_resident) {
    using namespace v82_resident;
    psx_mod_register_activation_plugin(Plugin, activate);
    for (const auto& h : Hooks) psx_mod_register_function_entry_plugin(Plugin, h.address, entry);
}
