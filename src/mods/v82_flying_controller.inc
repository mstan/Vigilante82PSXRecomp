/* Adapted from V82VehicleRegistry.ApplyOriginalV8FlyingPhysics and
 * V8VehicleMath.cs at reference revision 6fc8179.
 * Copyright (c) 2026 flaffy. MIT; tools/third_party/LICENSE_RECOMPONE. */
namespace flight {
constexpr uint32_t Physics = 0x80041B0Cu, Contact = 0x8002C590u;
struct Context { uint32_t object, prior_slope; bool applied; };
std::vector<Context> contexts;
int32_t add(int32_t a, int32_t b) { return int32_t(uint32_t(a) + uint32_t(b)); }
int32_t sub(int32_t a, int32_t b) { return int32_t(uint32_t(a) - uint32_t(b)); }
int32_t mul(int32_t a, int32_t b) { return int32_t(uint32_t(int64_t(a) * b)); }
int32_t rtz(int32_t v, unsigned n) { return (v < 0 ? add(v, (1 << n) - 1) : v) >> n; }
int32_t half(uint32_t p) { return int16_t(psx_mod_read_half(p)); }
bool flying(uint32_t object) {
    const int index = object_entry(object);
    return index >= 0 && package.vehicles[index].flying();
}

// Same three GTE commands as the reference's original V8 angular integrator.
// Read each result before the following command; matrix columns are stored
// only after the next vector has been submitted.
void rotate(CPUState* c, uint32_t matrix, int32_t pitch, int32_t yaw, int32_t roll) {
    for (unsigned i = 0; i < 5; ++i) gte_write_ctrl(c, uint8_t(i), rd(matrix + i * 4));
    const uint32_t vectors[3][3] = {
        {0x1000, uint32_t(roll), 0u - uint32_t(yaw)},
        {0u - uint32_t(roll), 0x1000, uint32_t(pitch)},
        {uint32_t(yaw), 0u - uint32_t(pitch), 0x1000},
    };
    uint32_t previous[3]{};
    for (unsigned column = 0; column < 3; ++column) {
        for (unsigned row = 0; row < 3; ++row) gte_write_data(c, uint8_t(9 + row), vectors[column][row]);
        gte_execute(c, 0x4A49E012);
        if (column) for (unsigned row = 0; row < 3; ++row)
            psx_mod_write_half(matrix + (column - 1) * 2 + row * 6, uint16_t(previous[row]));
        for (unsigned row = 0; row < 3; ++row) previous[row] = gte_read_data(c, uint8_t(9 + row));
    }
    for (unsigned row = 0; row < 3; ++row) psx_mod_write_half(matrix + 4 + row * 6, uint16_t(previous[row]));
}
void apply(CPUState* c, uint32_t object) {
    using original_specials::invoke;
    int32_t pos[3], velocity[3];
    for (unsigned i = 0; i < 3; ++i) { pos[i] = int32_t(rd(object + 0x34 + i * 4)); velocity[i] = int32_t(rd(object + 0x80 + i * 4)); }
    const int32_t surface = int32_t(invoke(c, 0x8002CFBC, object, object + 0x34));
    const int32_t depth = rtz(sub(surface, pos[1]), 8);
    const int32_t speed = int32_t(invoke(c, 0x80029E84, object + 0x80));
    wr(object + 0x8C, uint32_t(rtz(speed, 7)));
    const int32_t pitch = add(int32_t(rd(object + 0x90)), half(object + 0x2A) < 1 ? -0x200 : 0x200);
    const int32_t yaw = mul(half(object + 0xA8), 64);
    const int32_t roll = add(int32_t(rd(object + 0x98)), half(object + 0x26) < 0 ? 0x200 : -0x200);
    const int32_t thrust = half(object + 0xAA);
    velocity[0] = add(velocity[0], rtz(mul(half(object + 0x24), thrust), 4));
    const int32_t absolute_depth = depth < 0 ? sub(0, depth) : depth;
    const int32_t denominator = std::max(mul(depth, absolute_depth), 0x800);
    // V8's lift mass is controller-owned; sequel +E4 is an object pointer.
    velocity[1] = sub(add(velocity[1], 0x1C00), mul(0x5000, 0x1C00) / denominator);
    velocity[2] = add(velocity[2], rtz(mul(half(object + 0x30), thrust), 4));
    for (unsigned i = 0; i < 3; ++i) wr(object + 0x80 + i * 4, uint32_t(velocity[i]));
    wr(object + 0x90, uint32_t(pitch)); wr(object + 0x94, uint32_t(yaw)); wr(object + 0x98, uint32_t(roll));
    rotate(c, object + 0x20, rtz(pitch, 7), rtz(yaw, 7), rtz(roll, 7));
    for (unsigned i = 0; i < 3; ++i) {
        pos[i] = add(pos[i], rtz(velocity[i], 7));
        wr(object + 0x34 + i * 4, uint32_t(pos[i]));
    }
    invoke(c, 0x80059A0C, object + 0x20, object + 0x20);
    wr(object + 0x90, uint32_t(mul(pitch, 0xF80) >> 12));
    wr(object + 0x94, uint32_t(mul(yaw, 0xF80) >> 12));
    wr(object + 0x98, uint32_t(mul(roll, 0xF80) >> 12));
    for (unsigned i = 0; i < 3; ++i)
        wr(object + 0x80 + i * 4, uint32_t(sub(velocity[i], rtz(velocity[i], 6))));
    psx_mod_counter_add("v82.flight.ticks", 1);
}
bool ground_call(uint32_t pc, uint32_t ra) {
    switch (pc) {
    case 0x8003E774: return ra == 0x80041D74;
    case 0x8003E8C0: return ra == 0x80041D7C;
    case 0x8003EDC4: return ra == 0x80041DD0;
    case 0x8003FCC4: return ra == 0x80041DE0;
    case 0x80040598: return ra == 0x80041DF0;
    case 0x80040F10: return ra == 0x80041E00;
    default: return false;
    }
}
bool entry(CPUState* c, uint32_t address) {
    const auto object = c->gpr[4];
    if (address == Contact && c->gpr[31] == 0x8003C5A4 && constructing >= 0 &&
        package.vehicles[constructing].transform == 65535) {
        // The native six-contact loop already handles an absent socket. This
        // is the reference's no-contact-bank capability at its lookup seam.
        c->gpr[2] = 0; psx_mod_finish_function(c);
        psx_mod_counter_add("v82.flight.absent_contacts", 1); return true;
    }
    if (address == Physics && flying(object)) {
        contexts.push_back({object, rd(object + 0x9C), false});
        original(c, address);
        const bool applied = contexts.back().applied; contexts.pop_back();
        // 80041EF8..80041FC4 has only the follow-position calculation; the
        // epilogue follows immediately. Preserve its two preceding gates.
        if (applied && !(rd(object + 4) & 0x08000000) && !psx_mod_read_half(object + 0x132))
            for (unsigned i = 0; i < 12; i += 4) wr(object + 0x4C + i, rd(object + 0x34 + i));
        psx_mod_finish_function(c); return true;
    }
    if (!contexts.empty() && contexts.back().object == object && ground_call(address, c->gpr[31])) {
        if (!contexts.back().applied) {
            wr(object + 0x9C, contexts.back().prior_slope);
            contexts.back().applied = true;
            apply(c, object);
        }
        // These native calls are void; original flight replaces their ground
        // contribution, then retail weapon timers and post-physics run normally.
        psx_mod_finish_function(c); return true;
    }
    return false;
}
}
