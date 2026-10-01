#include "mod_test_runtime.h"

namespace {
constexpr uint32_t Weapon = 0x80100000, Owner = 0x80101000, Bank = 0x80102000;
constexpr uint32_t Muzzle = 0x80103000, Shot = 0x80104000, Fx = 0x80105000;
constexpr uint32_t Next = 0x80106000, Socket = 0x80107000, Stack = 0x801F0000;
struct Invocation { uint32_t pc; std::array<uint32_t, 5> args; };
std::vector<Invocation> calls;
uint32_t allocation = Fx;
bool fail_shot = false;
void require(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
void engine(CPUState* c, uint32_t pc, uint32_t) {
    require(c->gpr[29] == Stack - 0x40, "native service lacks its own ABI frame");
    calls.push_back({pc, {c->gpr[4], c->gpr[5], c->gpr[6], c->gpr[7], rd(c->gpr[29] + 16)}});
    uint32_t result = 0;
    switch (pc) {
    case 0x8002C17C: result = allocation; break;
    case 0x80042408: result = fail_shot ? 0 : Shot; break;
    case 0x8002C5F4: result = Socket; break;
    case 0x8001DD9C: result = 9; break;
    case 0x800305FC: case 0x8003066C: case 0x80030BF0: case 0x8001E580:
    case 0x80030CB0: case 0x80024148: case 0x800309A0: case 0x8002E22C: break;
    default: throw std::runtime_error("unexpected engine call");
    }
    c->gpr[2] = result; c->gpr[4] = 0xBAD; c->gpr[31] = 0xBAD; c->pc = 0xBAD;
}
CPUState cpu_for(uint32_t object, uint32_t mode, uint32_t other = 0) {
    CPUState c{}; c.gpr[4] = object; c.gpr[5] = mode; c.gpr[6] = other;
    c.gpr[29] = Stack; c.gpr[31] = 0x80020000; return c;
}
const Invocation& service(uint32_t pc) {
    for (const auto& call : calls) if (call.pc == pc) return call;
    throw std::runtime_error("expected engine service not reached");
}
}
int main() {
    test_dispatch = engine; enabled = true;
    package.vehicles.resize(2);
    for (unsigned i = 0; i < 2; ++i) {
        package.vehicles[i].flags = (1u << 23) | ((i ? 12u : 7u) << 17);
        package.vehicles[i].original_impact_kind = 0x60;
    }
    remember(Owner, 0); wr(state(0), Bank);
    auto c = cpu_for(Owner, 0); c.gpr[18] = Owner; c.gpr[17] = 0x80059880;
    instruction(&c, 0x8004AF0C);
    require(c.gpr[17] == original_specials::Houston, "Houston callback was not selected");
    remember(Owner, 1); instruction(&c, 0x8004AF0C);
    require(c.gpr[17] == original_specials::Alien, "Y callback was not selected");
    c.gpr[18] = Owner + 0x400; c.gpr[17] = 0x80059880;
    instruction(&c, 0x8004AF0C);
    require(c.gpr[17] == 0x80059880, "retail callback changed");

    wr(Weapon + 0x5C, Bank); wr(Bank + 12, 0x80108000); wr(Weapon + 0x10, Muzzle);
    wr(Owner + 0x80, uint32_t(-256)); wr(Owner + 0x84, 256); wr(Owner + 0x88, 0x08000000);
    psx_mod_write_half(Owner + 0xA, 0xFFFF);
    for (bool alien : {false, true}) {
        const auto callback = alien ? original_specials::Alien : original_specials::Houston;
        c = cpu_for(Weapon, 1); original_specials::special(&c, callback);
        require(psx_mod_read_half(Weapon + 0x1C) == 12 && psx_mod_read_byte(Weapon + 8) == 3,
                "original weapon initialization changed");
        c = cpu_for(Weapon, 12, Owner); const auto saved = c; calls.clear();
        original_specials::special(&c, callback);
        require(c.gpr[2] == 30 && c.gpr[29] == saved.gpr[29] && c.gpr[31] == saved.gpr[31] &&
                c.gpr[4] == saved.gpr[4] && c.pc == saved.pc, "service calls leaked CPU state");
        require(service(0x80042408).args == std::array<uint32_t,5>{Owner, Muzzle, alien ? 0x28u : 0x3Au, 0x98, Fx},
                "muzzle/effect linkage or fifth stack argument changed");
        require(rd(Shot) == original_specials::Projectile && rd(Shot + 4) == 0x60000084 &&
                psx_mod_read_half(Shot + 0x1C) == (alien ? 100 : 50), "projectile damage/identity changed");
        require(psx_mod_read_half(Shot + 0x94) == 8 && psx_mod_read_half(Shot + 0x96) == 2 &&
                psx_mod_read_half(Weapon + 0x1C) == 11, "propagation or ammo initialization changed");
        require(rd(Shot + 0x88) == uint32_t(-2) && rd(Shot + 0x8C) == 2 && rd(Shot + 0x90) == 0,
                "MIPS wrapping velocity conversion changed");
    }
    fail_shot = true; calls.clear(); c = cpu_for(Weapon, 12, Owner);
    original_specials::special(&c, original_specials::Houston);
    require(c.gpr[2] == 0 && psx_mod_read_half(Weapon + 0x1C) == 11 && service(0x800309A0).args[0] == Fx,
            "failed shot leaked effect or consumed ammo");
    fail_shot = false; allocation = Next; calls.clear();
    wr(Shot + 0x5C, Bank); wr(Shot + 0x34, 0x80000000); wr(Shot + 0x88, 0xFFFFFFFF);
    c = cpu_for(Shot, 0); original_specials::projectile(&c, original_specials::Projectile);
    require(calls.empty() && rd(Shot + 0x34) == 0x7FFFFFFF, "first propagation tick or wrap changed");
    original_specials::projectile(&c, original_specials::Projectile);
    require(rd(Next) == original_specials::Projectile && psx_mod_read_half(Next + 0x94) == 7 &&
            psx_mod_read_half(Next + 0x96) == 3 && psx_mod_read_half(Next + 0x1C) == 100,
            "child segment lost generations, countdown or damage");
    require(service(0x80024148).args[1] == Socket + 4, "propagation ignored authored socket");
    c = cpu_for(Shot, 5); calls.clear(); original_specials::projectile(&c, original_specials::Projectile);
    require(c.gpr[2] == UINT32_MAX && service(0x800309A0).args[0] == Shot, "animation completion did not retire shot");
    c = cpu_for(Shot, 3); calls.clear(); original_specials::projectile(&c, original_specials::Projectile);
    require(service(0x8002C17C).args[1] == 0x60 && rd(Next) == original_specials::Impact,
            "collision lost bank-owned impact metadata");
    std::cout << "original specials: callback selection, damage, ABI, propagation, collision and retirement passed\n";
}
