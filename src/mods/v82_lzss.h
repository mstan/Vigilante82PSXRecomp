#pragma once
/* SLUS-00868 texture LZSS (decoder 80021064) and the EXP texture tables that
 * feed it, for Resident Loading's pre-decoded textures. Host-only and
 * dependency-free so the pack derive callback and the tests share it.
 *
 * decode() is an instruction-level model of the guest decoder, not just of
 * its format: the guest decodes into a 2 KiB window on its stack, uploads
 * whole rows with LoadImage, then moves the unsent tail down 0x800 bytes.
 * Copy references index that physical buffer, so a stream could in principle
 * read stack bytes it never wrote (the first lap of the window, or the stale
 * overshoot area past 0x800). The model tracks which bytes were written and
 * refuses such a stream; the game then decodes it itself. The result is the
 * exact row data the guest uploads to VRAM and the exact number of
 * compressed bytes it reads. */
#include <cstdint>
#include <cstring>
#include <vector>

namespace v82_lzss {

/* Pixels (w*h 16-bit values, row-major) and compressed bytes consumed for
 * the stream at src[0..avail) decoded to a w x h rectangle (w in 16-bit
 * units). Returns false where the guest would read unwritten stack, run past
 * `avail`, or leave the frame the model covers. */
inline bool decode(const uint8_t* src, uint32_t avail, int w, int h,
                   std::vector<uint8_t>& pixels, uint32_t& consumed) {
    /* Buffer indices are relative to the guest's s4 = sp + 0x410; its frame
     * spans s4 - 0x400 .. s4 + 0x840. The tail move after the last upload
     * can land up to 0x800 below s4 (under the frame, never read back).
     * Rows wider than 256 are not modelled. */
    constexpr int Below = 0x800, Above = 0x840;
    if (w <= 0 || h <= 0 || w > 256 || h > 512) return false;
    uint8_t buf[Below + Above];
    bool written[Below + Above] = {};
    const int32_t row = 2 * w;
    int32_t s0 = 0, s2 = 0, line = 0;
    uint32_t flags = 0x100, in = 0;
    pixels.clear();
    pixels.reserve(size_t(w) * size_t(h) * 2u);
    auto at = [&](int32_t i) -> int32_t { return i >= -Below && i < Above ? i + Below : -1; };
    auto put = [&](int32_t i, uint8_t v) {
        const int32_t p = at(i);
        if (p < 0) return false;
        buf[p] = v;
        written[p] = true;
        return true;
    };
    auto get = [&](int32_t i, uint8_t& v) {
        const int32_t p = at(i);
        if (p < 0 || !written[p]) return false;
        v = buf[p];
        return true;
    };
    for (;;) {
        const int32_t fill = s2 + (h - line) * row;
        const int32_t a3 = fill < 0x800 ? fill : 0x800;
        do {
            if (flags & 0x100u) {
                if (in >= avail) return false;
                flags = uint32_t(src[in++]) << 24 | 1u;
            }
            if (flags & 0x80000000u) {
                if (in >= avail || !put(s0++, src[in++])) return false;
            } else {
                if (avail - in < 2u) return false;
                const uint32_t token = src[in] | uint32_t(src[in + 1]) << 8;
                in += 2;
                int32_t from = int32_t(token >> 5);
                const int32_t count = int32_t(token & 31u) + 2;
                const bool wraps = s0 < from && from + count >= 0x801;
                for (int32_t k = 0; k < count; ++k) {
                    uint8_t v;
                    if (!get(wraps ? (from & 0x7FF) : from, v) || !put(s0, v)) return false;
                    ++from;
                    ++s0;
                }
            }
            flags <<= 1;
        } while (s0 < a3);
        int32_t rows = (a3 - s2) / row;
        if ((rows & 1) && (row & 2) && line + rows < h) --rows;
        if (rows <= 0) return false;  // LoadImage with h = 0 is not modelled
        for (int32_t i = s2; i < s2 + rows * row; ++i) {
            uint8_t v;
            if (!get(i, v)) return false;
            pixels.push_back(v);
        }
        line += rows;
        s2 += rows * row;
        for (int32_t i = 0; i < s0 - s2; ++i) {  // memcpy 80020330, forward
            uint8_t v;
            if (!get(s2 + i, v) || !put(s2 - 0x800 + i, v)) return false;
        }
        s2 -= 0x800;
        s0 -= 0x800;
        if (line >= h) break;
    }
    consumed = in;
    return true;
}

/* A compressed texture image inside an EXP file: its 16-bit-unit size and
 * the file offset of its LZSS stream (texture header + 0x14). */
struct Texture { uint32_t offset, avail; int w, h; };

inline uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
inline uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
inline int16_t le16(const uint8_t* p) { return int16_t(p[0] | p[1] << 8); }

/* Every LZSS-compressed image in the BIN chunks of an IFF EXP file, as the
 * texture loader 8001F3C0 enumerates them: BIN +16 count, +20 table offset;
 * table entries are offsets from the table; a header (80020FF8) has flags at
 * +4 (8 = CLUT first, 0x10 = compressed image), record size at +8, rect at
 * +0xC and data at +0x14. Malformed structures end the walk quietly. */
inline std::vector<Texture> textures(const uint8_t* d, uint32_t size) {
    std::vector<Texture> out;
    auto walk = [&](auto&& self, uint32_t p, uint32_t end, int depth) -> void {
        while (depth < 8 && end <= size && p + 8u <= end) {
            const uint32_t length = be32(d + p + 4);
            if (length > end - p - 8u) return;
            if (!std::memcmp(d + p, "FORM", 4) && length >= 4u) {
                self(self, p + 12u, p + 8u + length, depth + 1);
            } else if (!std::memcmp(d + p, "BIN ", 4) && length >= 24u) {
                const uint32_t bin = p + 8u, limit = bin + length;
                const uint32_t count = le32(d + bin + 16), table = bin + le32(d + bin + 20);
                if (table < bin || table > limit || count > (limit - table) / 4u) return;
                for (uint32_t i = 0; i < count; ++i) {
                    uint32_t h = table + le32(d + table + 4u * i);
                    if (h < bin || h > limit || limit - h < 0x14u) continue;
                    const uint32_t flag = le32(d + h + 4);
                    if (flag & 8u) {
                        const uint32_t clut = le32(d + h + 8);
                        if (clut > limit - h || limit - (h + clut) < 0x14u) continue;
                        h += clut;
                    }
                    if (!(flag & 0x10u)) continue;
                    out.push_back({h + 0x14u, limit - (h + 0x14u), le16(d + h + 0x10), le16(d + h + 0x12)});
                }
            }
            p += 8u + ((length + 1u) & ~1u);
        }
    };
    walk(walk, 0, size, 0);
    return out;
}

}  // namespace v82_lzss
