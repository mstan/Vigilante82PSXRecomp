/* Adapted from sdk/V8OriginalSpecials.cs at reference revision 6fc8179.
 * Copyright (c) 2026 flaffy. MIT; tools/third_party/LICENSE_RECOMPONE.
 * Original Houston/Y weapons, using the sequel's native engine services. */
namespace original_specials {
constexpr uint32_t Houston = 0x8FFF0020u, Alien = 0x8FFF0024u;
constexpr uint32_t Projectile = 0x8FFF0028u, Effect = 0x8FFF002Cu, Impact = 0x8FFF0030u;

uint32_t invoke(CPUState* c, uint32_t address, uint32_t a0 = 0, uint32_t a1 = 0,
                uint32_t a2 = 0, uint32_t a3 = 0, uint32_t a4 = 0) {
    const CPUState saved = *c;
    c->gpr[29] -= 0x40;
    c->gpr[4] = a0; c->gpr[5] = a1; c->gpr[6] = a2; c->gpr[7] = a3;
    wr(c->gpr[29] + 0x10, a4); c->pc = 0;
    psx_dispatch_call(c, address, saved.gpr[31]);
    const auto result = c->gpr[2]; *c = saved; return result;
}
int32_t signed_half(uint32_t p) { return int16_t(psx_mod_read_half(p)); }
int32_t shift(uint32_t value, unsigned left, unsigned right) { return int32_t(value << left) >> right; }
uint32_t sound(uint32_t object) { return rd(rd(object + 0x5C) + 12); }
uint32_t voice(CPUState* c) { return invoke(c, 0x8001DD9C); }

void aim(CPUState* c, uint32_t object, uint32_t owner, bool alien) {
    const auto target = rd(owner + 0xF0), child = rd(object + 0x10);
    if (!target || !child) return;
    const auto matrix = invoke(c, 0x8002CDF4, object), v = c->gpr[29] - 0x20;
    for (unsigned i = 0; i < 12; i += 4) wr(v + i, rd(target + 0x4C + i) - rd(matrix + 0x14 + i));
    invoke(c, 0x8002426C, matrix, v, v);
    const int32_t yaw = shift(invoke(c, 0x8005B9AC, rd(v), rd(v + 8)), 20, 20);
    int32_t pitch = shift(0u - invoke(c, 0x8005B9AC, rd(v + 4), rd(v + 8)), 20, 20);
    if (!alien) pitch = std::clamp(pitch, -128, 256);
    psx_mod_write_half(child + 0x46, uint16_t(yaw));
    psx_mod_write_half(child + 0x44, uint16_t(pitch));
    invoke(c, 0x8002CF74, child);
}
uint32_t fire(CPUState* c, uint32_t object, uint32_t owner, bool alien) {
    const auto effect = invoke(c, 0x8002C17C, rd(object + 0x5C), alien ? 0x2A : 0x3C, 0x80, 8);
    if (!effect) return 0;
    const auto shot = invoke(c, 0x80042408, owner, rd(object + 0x10), alien ? 0x28 : 0x3A, 0x98, effect);
    if (!shot) { invoke(c, 0x800309A0, effect); return 0; }
    psx_mod_write_half(shot + 0x1C, alien ? 100 : 50);
    wr(shot, Projectile); psx_mod_write_half(shot + 0x96, 2); psx_mod_write_half(shot + 0x94, 8);
    wr(shot + 4, 0x60000084);
    for (unsigned i = 0; i < 12; i += 4) wr(shot + 0x88 + i, uint32_t(shift(rd(owner + 0x80 + i), 5, 12)));
    invoke(c, 0x800305FC, shot);
    wr(effect + 4, 0x10); wr(effect, Effect);
    if (!(rd(owner + 4) & 4)) invoke(c, 0x80030BF0, effect);
    invoke(c, 0x8001E580, voice(c), sound(object), alien ? 2 : 0, shot + 0x34);
    invoke(c, 0x80030CB0, object, 25);
    psx_mod_write_half(object + 0x1C, uint16_t(psx_mod_read_half(object + 0x1C) - 1));
    psx_mod_counter_add(alien ? "v82.special.y.fired" : "v82.special.houston.fired", 1);
    return signed_half(owner + 0xA) >= 0 ? 180 : 30;
}
void special(CPUState* c, uint32_t address) {
    const bool alien = (address & 0x1FFFFFFFu) == (Alien & 0x1FFFFFFFu);
    const auto object = c->gpr[4], owner = c->gpr[6];
    uint32_t result = 0;
    switch (c->gpr[5]) {
    case 0:
        if (owner && invoke(c, 0x80042330, object, owner)) aim(c, object, owner, alien);
        break;
    case 1:
        psx_mod_write_half(object + 0x1C, 12); psx_mod_write_byte(object + 8, 3);
        wr(object + 4, rd(object + 4) | 0x4000); break;
    case 2:
        if (!(rd(object + 4) & 0x20000)) {
            wr(object + 4, rd(object + 4) | 0x20000);
            invoke(c, 0x8001E14C, voice(c), sound(object), alien ? 0 : 1);
        }
        if (!psx_mod_read_half(object + 0x1C)) invoke(c, 0x8003A368, object);
        break;
    case 11: {
        wr(object + 4, rd(object + 4) & ~0x20000u);
        const auto matrix = invoke(c, 0x8002CDF4, object);
        invoke(c, alien ? 0x8001E628 : 0x8001E580, voice(c), sound(object), 4, matrix + 0x14);
        break;
    }
    case 12: if (owner) result = fire(c, object, owner, alien); break;
    case 13:
        if (owner && !rd(0x8006B874) && int32_t(rd(owner + 0x8C)) < 0x11E1) {
            const auto target = rd(owner + 0xF0);
            if (target) result = invoke(c, 0x80029F6C, owner + 0x4C, target + 0x4C) <= 0x12BFFF;
        }
        break;
    case 14: result = 6; break;
    case 15: result = 0x801F; break;
    }
    c->gpr[2] = result;
    psx_mod_counter_add("v82.special.original.events", 1);
}
void impact(CPUState* c, uint32_t object) {
    int kind = -1;
    const auto bank = rd(object + 0x5C);
    for (unsigned i = 0; i < package.vehicles.size(); ++i)
        if ((package.vehicles[i].flags & (1u << 23)) && bank && rd(state(i)) == bank)
            kind = package.vehicles[i].original_impact_kind;
    if (kind < 0) fail("v82.special.missing_impact_metadata");
    const auto fx = invoke(c, 0x8002C17C, bank, uint32_t(kind), 0x80, 8);
    if (!fx) return;
    psx_mod_write_byte(fx + 8, 7); wr(fx + 4, 0xB4); wr(fx, Impact);
    for (unsigned i = 0; i < 12; i += 4) wr(fx + 0x4C + i, rd(object + 0x4C + i));
    invoke(c, 0x8003066C, fx);
}
void projectile(CPUState* c, uint32_t) {
    const auto object = c->gpr[4], mode = c->gpr[5], collision = c->gpr[6];
    c->gpr[2] = 0;
    if (mode == 0) {
        for (unsigned i = 0; i < 12; i += 4) {
            wr(object + 0x34 + i, rd(object + 0x34 + i) + rd(object + 0x88 + i));
            wr(object + 0x4C + i, rd(object + 0x34 + i));
        }
        const auto delay = uint16_t(psx_mod_read_half(object + 0x96) - 1);
        psx_mod_write_half(object + 0x96, delay);
        if (delay || !signed_half(object + 0x94)) return;
        const auto next = invoke(c, 0x8002C17C, rd(object + 0x5C), psx_mod_read_half(object + 0x1A), 0x98, 8);
        if (!next) return;
        const auto socket = invoke(c, 0x8002C5F4, object, 0x8000);
        if (!socket) fail("v82.special.missing_propagation_socket");
        psx_mod_write_byte(next + 8, 8); wr(next + 4, 0x60000084);
        psx_mod_write_half(next + 0xA, psx_mod_read_half(object + 0xA));
        psx_mod_write_half(next + 0x1C, psx_mod_read_half(object + 0x1C)); wr(next, Projectile);
        invoke(c, 0x8003066C, next);
        psx_mod_write_half(next + 0x96, 3);
        psx_mod_write_half(next + 0x94, uint16_t(psx_mod_read_half(object + 0x94) - 1));
        for (unsigned i = 0; i < 12; i += 4) wr(next + 0x88 + i, rd(object + 0x88 + i));
        wr(next + 0x80, rd(object + 0x80));
        for (unsigned i = 0; i < 32; i += 4) wr(next + 0x20 + i, rd(object + 0x20 + i));
        invoke(c, 0x80024148, object + 0x20, socket + 4, next + 0x34);
        psx_mod_counter_add("v82.special.propagated", 1);
    } else if (mode == 3) {
        impact(c, object); wr(object + 4, rd(object + 4) | 0x20);
        invoke(c, 0x8001E580, voice(c), sound(object), 3, object + 0x34);
        if (invoke(c, 0x8002E22C, object + 0x34, 0)) invoke(c, 0x8004E338, 0x08C0C000);
        const auto hit = collision ? rd(collision) : 0;
        if (hit && psx_mod_read_byte(hit + 8) == 2) {
            if (signed_half(hit + 0xA) < 0) invoke(c, 0x80015ADC, uint32_t(~signed_half(hit + 0xA)), 20);
            if ((invoke(c, 0x8002AC5C) * 10 >> 15) == 0) {
                const auto v = c->gpr[29] - 0x20;
                for (unsigned i = 0; i < 3; ++i) {
                    const auto timer = hit + 0x12E + i * 2;
                    if (!psx_mod_read_half(timer)) continue;
                    wr(v, (invoke(c, 0x8002AC5C) * 3051 >> 15) - 1525);
                    wr(v + 4, uint32_t(-4577)); wr(v + 8, (invoke(c, 0x8002AC5C) * 3051 >> 15) - 1525);
                    while ((invoke(c, 0x8002AC5C) * 12 >> 15) != i) {}
                    invoke(c, 0x8004AA24, psx_mod_read_half(0x80063FA4 + i * 2), hit + 0x34, v);
                    psx_mod_write_half(timer, 0);
                }
            }
        }
        psx_mod_counter_add("v82.special.collisions", 1);
    } else if (mode == 5) {
        invoke(c, 0x800309A0, object); c->gpr[2] = UINT32_MAX;
        psx_mod_counter_add("v82.special.retired", 1);
    }
}
void impact_effect(CPUState* c, uint32_t) {
    const auto object = c->gpr[4], mode = c->gpr[5]; c->gpr[2] = 0;
    if (mode == 1) {
        for (unsigned n = 0; n < 8; ++n) {
            const auto spark = invoke(c, 0x8002C17C, rd(object + 0x5C), uint32_t(psx_mod_read_half(object + 0x1A)) - 1, 0x94, 8);
            if (!spark) continue;
            for (unsigned i = 0; i < 12; i += 4) wr(spark + 0x34 + i, 0);
            wr(spark + 0x88, uint32_t(shift(invoke(c, 0x8002AC5C), 11, 15)) - 1024);
            wr(spark + 0x8C, 0u - uint32_t(shift(invoke(c, 0x8002AC5C), 12, 15)));
            wr(spark + 0x90, uint32_t(shift(invoke(c, 0x8002AC5C), 11, 15)) - 1024);
            wr(spark + 4, 0x10);
            psx_mod_write_byte(spark + 0x86, uint8_t((invoke(c, 0x8002AC5C) << 3 >> 15) + 24));
            invoke(c, 0x8002CC48, object, spark);
        }
    } else if (mode == 0) {
        for (auto child = rd(object + 0x10); child;) {
            const auto next = rd(child + 0xC);
            const auto life = uint8_t(psx_mod_read_byte(child + 0x86) - 1);
            psx_mod_write_byte(child + 0x86, life);
            if (!life) { invoke(c, 0x8002CCBC, child); invoke(c, 0x8002C4B4, child); }
            else {
                for (unsigned i = 0; i < 12; i += 4) wr(child + 0x34 + i, rd(child + 0x34 + i) + rd(child + 0x88 + i));
                wr(child + 0x8C, rd(child + 0x8C) + 56);
            }
            child = next;
        }
    } else if (mode == 5) { invoke(c, 0x800309A0, object); c->gpr[2] = UINT32_MAX; }
}
void effect(CPUState* c, uint32_t) {
    const auto object = c->gpr[4]; c->gpr[2] = 0;
    if (c->gpr[5] != 5) return;
    if (rd(object + 0x14)) invoke(c, 0x800308C4, invoke(c, 0x8002CCBC, object));
    else invoke(c, 0x800309A0, object);
    c->gpr[2] = UINT32_MAX;
}
void register_callbacks() {
    psx_mod_register_guest_function_plugin(Plugin, Houston, special);
    psx_mod_register_guest_function_plugin(Plugin, Alien, special);
    psx_mod_register_guest_function_plugin(Plugin, Projectile, projectile);
    psx_mod_register_guest_function_plugin(Plugin, Effect, effect);
    psx_mod_register_guest_function_plugin(Plugin, Impact, impact_effect);
}
}
