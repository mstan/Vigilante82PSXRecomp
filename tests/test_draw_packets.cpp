#include "mod_test_runtime.h"
#include <initializer_list>

int main() {
    using draw_packets::Result;
    constexpr uint32_t p = 0x80090000, next = p + 0x100;
    auto packet = [&](std::initializer_list<uint32_t> words) {
        wr(p, uint32_t(words.size()) << 24 | (next & 0xFFFFFF));
        unsigned i = 0; for (auto word : words) wr(p + 4 + 4 * i++, word);
        return draw_packets::validate(p, unsigned(words.size()));
    };
    auto expect = [](bool ok) { if (!ok) throw std::runtime_error("draw packet regression"); };
    // Captured first corrupting packet: a draw-list A0 consumes subsequent
    // polygons as a 311x84 upload, overwriting the UFO and HUD texture pages.
    expect(packet({0xA0673640,0x00530136,0x00540137}) == Result::Transfer);
    enabled = true; init(); wr(DrawOTag, 0x3C028007); wr(Meta + 8, 1);
    wr(next,0x04FFFFFF); wr(next+4,0x80000000); wr(next+8,0);
    wr(next+12,0); wr(next+16,0x00010002);
    CPUState cpu{}; cpu.gpr[4]=p;cpu.gpr[28]=0x8006A7F0;cpu.gpr[31]=0x80010000;
    entry(&cpu,DrawOTag);
    expect(rd(p)==(next&0xFFFFFF) && rd(next)==0x04FFFFFF && cpu.pc==0 && cpu.gpr[4]==p);
    // Payload colour/UV words may look like transfer opcodes; only inspect
    // actual command boundaries. Preserve a valid GT3 and chained state word.
    expect(packet({0x34808080,0,0,0xA0673640,0,0,0,0,0,0xE1000000})==Result::Valid);
    auto header=rd(p);entry(&cpu,DrawOTag);expect(rd(p)==header);
    expect(packet({0x34808080,0,0})==Result::Truncated);
    expect(packet({0xC0000000,0,0})==Result::Transfer);
    expect(packet({0x48000000,0,0,0x50005000,0xE1000000})==Result::Valid);
    // Mandatory vertices/second colour, and shaded vertex words, are never
    // terminators even if their bits match the GPU's termination mask.
    expect(packet({0x58000000,0,0x50005000,0,0,0x50005000,0x50005000})==Result::Valid);
    expect(packet({0x48000000,0,0})==Result::Unterminated);
    expect(packet({0x58000000,0,0,0,0})==Result::Truncated);
    expect(packet({0xFE000000})==Result::Unknown);
    // Retail-only matches and a disabled plugin must retain the original list.
    packet({0xA0673640,0x00530136,0x00540137});header=rd(p);wr(Meta+8,0);
    entry(&cpu,DrawOTag);expect(rd(p)==header);
    psx_mod_write_byte(cpu.gpr[28]+0xC28,18);entry(&cpu,DrawOTag);expect(rd(p)==header);
    wr(Meta+40,1);entry(&cpu,DrawOTag);expect(rd(p)==(next&0xFFFFFF));
    packet({0xA0673640,0x00530136,0x00540137});enabled=false;
    entry(&cpu,DrawOTag);expect(rd(p)==header);
    std::cout << "captured upload corruption, packet boundaries, polylines and activation passed\n";
}
