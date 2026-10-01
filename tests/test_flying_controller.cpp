#include "mod_test_runtime.h"

namespace {
constexpr uint32_t Object = 0x80100000, Stack = 0x801F0000;
int physics_calls = 0, ground_mode = 0;
void require(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
void matrix_command(CPUState* c, uint32_t cmd) {
    require(cmd == 0x4A49E012, "angular integrator changed GTE command");
    int16_t matrix[9];
    for (unsigned i = 0; i < 9; ++i) matrix[i] = int16_t(c->gte_ctrl[i / 2] >> (16 * (i % 2)));
    int16_t input[3];
    for (unsigned i = 0; i < 3; ++i) input[i] = int16_t(c->gte_data[9 + i]);
    for (unsigned row = 0; row < 3; ++row) {
        int64_t sum = 0;
        for (unsigned col = 0; col < 3; ++col) sum += int64_t(matrix[row * 3 + col]) * input[col];
        c->gte_data[9 + row] = uint32_t(std::clamp(sum >> 12, int64_t(-32768), int64_t(32767)));
    }
}
void engine(CPUState* c, uint32_t pc, uint32_t) {
    if (pc == flight::Physics) {
        ++physics_calls;
        // Retail writes the ground-only slope immediately before dispatch.
        wr(Object + 0x9C, 0xBAD);
        const auto ra = c->gpr[31];
        c->gpr[31] = ground_mode ? 0x80041D74 : 0x80041DD0;
        require(flight::entry(c, ground_mode ? 0x8003E774 : 0x8003EDC4), "flight did not replace ground dispatch");
        if (ground_mode) {
            c->gpr[31] = 0x80041D7C;
            require(flight::entry(c, 0x8003E8C0), "rolling tail ran ground physics");
        }
        c->gpr[31] = ra;
        // Timers and unrelated post-physics must still execute.
        psx_mod_write_half(Object + 0x12E, uint16_t(psx_mod_read_half(Object + 0x12E) - 1));
        if (!(rd(Object + 4) & 0x08000000))
            for (unsigned i = 0; i < 12; i += 4) wr(Object + 0x4C + i, 0x55);
        return;
    }
    require(c->gpr[29] == Stack - 0x40, "flight service has no outgoing stack frame");
    switch (pc) {
    case 0x8002CFBC: c->gpr[2] = 64 * 256; break;
    case 0x80029E84: c->gpr[2] = 1024; break;
    case 0x80059A0C: require(c->gpr[4] == Object + 0x20 && c->gpr[5] == Object + 0x20, "normalization pointer changed"); break;
    default: throw std::runtime_error("unexpected flight service");
    }
}
void reset() {
    for (unsigned i = 0; i < 0x200; i += 4) wr(Object + i, 0);
    for (unsigned i : {0u, 8u, 16u}) psx_mod_write_half(Object + 0x20 + i, 0x1000);
    wr(Object + 0x9C, 0x12345678); wr(Object + 0xE4, 0x80123456);
    psx_mod_write_half(Object + 0xAA, 16); psx_mod_write_half(Object + 0xA8, 4);
    psx_mod_write_half(Object + 0x12E, 9);
    remember(Object, 0);
}
}
int main() {
    test_dispatch = engine; test_gte_execute = matrix_command;
    package.vehicles.resize(2); package.vehicles[0].flags = 0x101;
    package.vehicles[0].transform = 65535; package.vehicles[1].transform = 1;
    init(); reset();
    CPUState c{}; c.gpr[4] = Object; c.gpr[29] = Stack; c.gpr[31] = 0x80020000;
    require(flight::entry(&c, flight::Physics), "flying object was ignored");
    require(int32_t(rd(Object + 0x38)) == -224 && rd(Object + 0x3C) == 32,
            "lift or forward thrust integration changed");
    require(int32_t(rd(Object + 0x84)) == -28224 && rd(Object + 0x88) == 4032 && rd(Object + 0x8C) == 8,
            "flight drag or speed conversion changed");
    require(rd(Object + 0x9C) == 0x12345678 && rd(Object + 0xE4) == 0x80123456,
            "flight overwrote ground slope or native pointer");
    require(rd(Object + 0x50) == rd(Object + 0x38) && psx_mod_read_half(Object + 0x12E) == 8,
            "native timers or unsmoothed follow position changed");
    require(flight::half(Object + 0x22) == 4 && flight::half(Object + 0x24) == 2 && flight::half(Object + 0x26) == -4 &&
            flight::half(Object + 0x2A) == 4 && flight::half(Object + 0x2C) == -2 && flight::half(Object + 0x2E) == -4,
            "angular matrix columns changed");
    reset(); ground_mode = 1; c.gpr[4] = Object;
    flight::entry(&c, flight::Physics);
    require(int32_t(rd(Object + 0x38)) == -224, "rolling branch applied flight twice");
    reset(); ground_mode = 0; psx_mod_write_half(Object + 0x132, 2);
    flight::entry(&c, flight::Physics);
    require(rd(Object + 0x50) == 0x55, "radar-jammer smoothing gate was overridden");
    reset(); wr(Object + 4, 0x08000000); wr(Object + 0x50, 0x77);
    flight::entry(&c, flight::Physics);
    require(rd(Object + 0x50) == 0x77, "native no-follow flag was overridden");
    remember(Object, 1); const int before = physics_calls;
    require(!flight::entry(&c, flight::Physics) && physics_calls == before, "ground car entered flight");
    constructing = 0; c.gpr[31] = 0x8003C5A4; c.gpr[2] = 123;
    require(flight::entry(&c, flight::Contact) && c.gpr[2] == 0, "wheel-free topology was ignored");
    c.gpr[31] = 0x80020000;
    require(!flight::entry(&c, flight::Contact), "unrelated authored socket was suppressed");
    constructing = 1; c.gpr[31] = 0x8003C5A4;
    require(!flight::entry(&c, flight::Contact), "ground car contact was suppressed");
    require(flight::rtz(-127, 7) == 0 && flight::rtz(-129, 7) == -1 &&
            flight::add(INT32_MAX, 1) == INT32_MIN, "MIPS arithmetic boundary changed");
    std::cout << "flight: movement, rotation, native timers, follow gates, topology and ground isolation passed\n";
}
