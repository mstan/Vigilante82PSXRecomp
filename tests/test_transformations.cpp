#include "mod_test_runtime.h"

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static unsigned uploads = 0;
static void native_upload(CPUState* cpu, uint32_t pc, uint32_t ra) {
    require(pc == LoadCompressedTexture, "wrong decoder");
    require(cpu->gpr[4] == 0x1F8002E0u && cpu->gpr[5] == 0x80401020u,
            "texture/rectangle pointers changed");
    const auto sp = cpu->gpr[29];
    require(sp >= HeapLo + 0x2000 && sp < HeapHi, "decoder still on scratchpad");
    entry(cpu, pc); // Bypass must prevent recursive redirection.
    require(cpu->gpr[29] == sp && cpu->pc == 0, "decoder hook recursed");
    // Actual decoder frame size plus nested native GPU upload calls. These
    // writes fail the bounded test memory if the scratchpad SP is retained.
    for (uint32_t i = 0; i < 0xE00; i += 4) wr(sp - i, 0xA5000000u + i);
    cpu->gpr[2] = 0x1234;
    cpu->pc = ra;
    ++uploads;
}

int main() {
    enabled = true;
    package.vehicles.resize(2);
    package.vehicles[1].flags = 0x101; // Y: flying, no transformation contacts.
    package.banks.resize(1);
    init();
    for (const auto& hook : Hooks) wr(hook.pc, hook.word);
    wr(bank_state(0) + 4, 0x80401000);
    wr(bank_state(0) + 8, 0x100);
    const auto sentinel = allocate(32); wr(sentinel, 0xCAFEBABE);
    CPUState cpu{};
    cpu.gpr[29] = 0x1F800300; cpu.gpr[31] = 0x80021420;
    cpu.gpr[4] = 0x1F8002E0; cpu.gpr[5] = 0x80401020;
    test_dispatch = native_upload;
    for (unsigned n = 0; n < 30; ++n) {
        cpu.pc = 0;
        entry(&cpu, LoadCompressedTexture);
        require(cpu.gpr[29] == 0x1F800300 && cpu.pc == cpu.gpr[31], "caller stack/return lost");
        require(cpu.gpr[2] == 0x1234 && rd(sentinel) == 0xCAFEBABE, "native result or live heap corrupted");
    }
    require(uploads == 30, "native texture decoder skipped");
    release(sentinel);
    const auto whole = allocate(HeapHi - HeapLo - 8);
    require(whole == HeapLo + 8, "temporary decoder stacks leaked");
    release(whole);
    cpu.pc = 0; cpu.gpr[29] = 0x801FF000;
    entry(&cpu, LoadCompressedTexture);
    require(cpu.pc == 0 && uploads == 30, "main RAM upload intercepted");
    cpu.gpr[29] = 0x1F800300; cpu.gpr[5] = 0x80070000;
    entry(&cpu, LoadCompressedTexture);
    require(cpu.pc == 0 && uploads == 30, "retail texture intercepted");

    constexpr uint32_t ground = 0x80402000, alien = 0x80402200;
    constexpr uint32_t pickup = 0x80402400, collision = 0x80402500;
    remember(ground, 0); remember(alien, 1);
    for (auto vehicle : {ground, alien}) {
        for (unsigned mode = 1; mode <= 3; ++mode) {
            cpu.pc = 0; cpu.gpr[4] = vehicle; cpu.gpr[5] = mode;
            entry(&cpu, Transform);
            require((cpu.pc == cpu.gpr[31]) == (vehicle == alien), "wrong transformation capability");
            wr(collision, vehicle); psx_mod_write_half(pickup + 0x1A, uint16_t(mode + 6));
            cpu.pc = 0; cpu.gpr[4] = pickup; cpu.gpr[5] = 3; cpu.gpr[6] = collision;
            entry(&cpu, Pickup);
            require((cpu.pc == cpu.gpr[31]) == (vehicle == alien), "wrong pickup capability");
            require(rd(collision) == vehicle && psx_mod_read_half(pickup + 0x1A) == mode + 6,
                    "rejected pickup mutated its participants");
        }
    }
    psx_mod_write_half(pickup + 0x1A, 1); cpu.pc = 0;
    entry(&cpu, Pickup);
    require(cpu.pc == 0, "alien ordinary pickup rejected");
    std::cout << "native decoder stack lifecycle and ground/alien pickup capabilities passed\n";
}
