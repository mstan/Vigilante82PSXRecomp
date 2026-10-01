/* Gameplay packet validation adapted from LibGpu.IsSafeGameplayOrderingTablePacket.
 * Copyright (c) 2026 flaffy. MIT; tools/third_party/LICENSE_RECOMPONE.
 * Keep this enhancement in the title: the hardware correctly interprets A0
 * as an upload. Imported draw records must not feed that command to it. */
namespace draw_packets {
enum class Result { Valid, Transfer, Truncated, Unterminated, Unknown };
Result validate(uint32_t address, unsigned words) {
    unsigned offset = 0;
    while (offset < words) {
        const unsigned op = rd(address + 4 + offset * 4) >> 24;
        if (op >= 0xA0 && op <= 0xDF) return Result::Transfer;
        unsigned length = 1;
        if (op == 2) length = 3;
        else if (op >= 0x20 && op <= 0x3F) {
            const unsigned vertices = (op & 8) ? 4 : 3;
            length = 1 + vertices + ((op & 16) ? vertices - 1 : 0) + ((op & 4) ? vertices : 0);
        } else if (op >= 0x40 && op <= 0x5F) {
            const bool shaded = (op & 16) != 0;
            length = shaded ? 4 : 3;
            if (op & 8) {
                // The first two vertices are mandatory. Only subsequent
                // vertex starts (colour words for shaded lines) can terminate.
                if (words - offset < length) return Result::Truncated;
                offset += length;
                bool terminated = false;
                while (offset < words) {
                    const auto word = rd(address + 4 + offset * 4);
                    if ((word & 0xF000F000u) == 0x50005000u) {
                        ++offset; terminated = true; break;
                    }
                    const unsigned vertex_words = shaded ? 2 : 1;
                    if (words - offset < vertex_words) return Result::Truncated;
                    offset += vertex_words;
                }
                if (!terminated) return Result::Unterminated;
                continue;
            }
        } else if (op >= 0x60 && op <= 0x7F)
            length = 2 + ((op & 4) ? 1 : 0) + ((op & 0x18) ? 0 : 1);
        else if (op >= 0x80 && op <= 0x9F) length = 4;
        else if (op != 0 && op != 1 && op != 0x1F && !(op >= 0xE1 && op <= 0xE6))
            return Result::Unknown;
        if (words - offset < length) return Result::Truncated;
        offset += length;
    }
    return Result::Valid;
}
void prepare(CPUState* cpu) {
    // Reservations exist only during a match containing imported vehicles.
    // Dreamland also needs this contract with an entirely retail roster.
    if (!rd(Meta + 8) && !(rd(Meta + 40) && psx_mod_read_byte(cpu->gpr[28] + 0xC28) == 18)) return;
    uint32_t address = cpu->gpr[4] & 0x007FFFFCu;
    for (unsigned nodes = 0; nodes < 0x100000; ++nodes) {
        const auto header = rd(address), next = header & 0x00FFFFFFu;
        const unsigned words = header >> 24;
        if (address > 0x800000u - 4u - words * 4u) fail("v82.draw.invalid_address");
        const auto result = validate(address, words);
        if (result != Result::Valid) {
            // Preserve the native list and DMA submission; omit only this
            // malformed packet, exactly as the reference's draw-list guard.
            wr(address, next);
            psx_mod_counter_add("v82.draw.rejected", 1);
            psx_mod_counter_add(result == Result::Transfer ? "v82.draw.rejected_transfer" :
                result == Result::Truncated ? "v82.draw.rejected_truncated" :
                result == Result::Unterminated ? "v82.draw.rejected_polyline" :
                "v82.draw.rejected_unknown", 1);
        }
        if (next == 0x00FFFFFFu) return;
        address = next & 0x007FFFFCu;
    }
    fail("v82.draw.cyclic_list");
}
}
