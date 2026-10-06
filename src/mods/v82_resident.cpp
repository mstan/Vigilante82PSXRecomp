/* SLUS-00868 Resident Loading adapter; contract in v82_resident.h. Guest CD
 * timing, movies, XA and CD audio are untouched: supported loader calls
 * complete from verified resident sectors, everything else runs the game's
 * original loader. */
#include "v82_resident.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include "mod_resident.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
bool observe = false;     /* debug tools: record loader calls, never serve */
bool forcing_cd = false;  /* a fallback is starting the original CD stream */

uint32_t rd(uint32_t a) { return psx_mod_read_word(a); }
void wr(uint32_t a, uint32_t v) { psx_mod_write_word(a, v); }

bool ram_span(uint32_t address, uint32_t bytes) {
    const uint32_t phys = address & 0x1FFFFFFFu;
    const uint32_t limit = 0x00800000u; /* 8 MiB when a content mod expands RAM */
    return address >= 0x80000000u && address < 0xA0000000u && phys >= 0x10000u &&
           phys < limit && bytes <= limit - phys;
}

/* The loader text the adapter reproduces, hashed from live guest RAM. */
bool contract() {
    static const PSXResidentRange range{kLoaderLo, kLoaderHi};
    if (psx_resident_guest_ranges_match(&range, 1, kLoaderSha256)) return true;
    psx_mod_counter_add("v82.resident.contract_mismatch", 1);
    return false;
}

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

struct Hook { uint32_t address, word; void (*handler)(CPUState*, uint32_t); };
constexpr Hook Hooks[] = {
    {WholeRead, 0x27BDFFE8u, on_whole_read}, {Begin, 0x27BDFFE8u, on_begin},
    {End, 0x27BDFFF8u, on_end}, {Read, 0x8F820D64u, on_read},
    {ReadByte, 0x8F820D64u, on_read_byte}, {Seek, 0x27BDFFF0u, on_seek},
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
#ifndef PSX_NO_DEBUG_TOOLS
    const char* mode = std::getenv("PSX_V82_RESIDENT_OBSERVE");
    observe = mode && !std::strcmp(mode, "1");
#endif
    for (uint32_t i = 0; i < FileCount; ++i)
        files[i] = {kCatalog[i].path, kCatalog[i].size, kCatalog[i].sha256};
    PSXResidentSpec spec{};
    spec.struct_size = sizeof spec;
    spec.title = "Vigilante82PSXRecomp";
    spec.format = "slus00868-files-v1";
    spec.files = files;
    spec.file_count = FileCount;
    /* Changed files stay in the pack but are never served (per-file stock
     * check at each request), so one modded asset keeps the rest resident. */
    spec.policy = PSX_RESIDENT_ALLOW_MODIFIED;
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
