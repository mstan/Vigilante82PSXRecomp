/* Original cars in the sequel engine. Adapted from the authorized reference's
 * V82VehicleRegistry, NativeVehicleBankSource and V82Compat.
 * Copyright (c) 2026 flaffy. MIT; tools/third_party/LICENSE_RECOMPONE.
 * Mutable ownership lives in expanded guest RAM and is saved with the game. */
#include "v82_vehicle_package.h"
#include "mod_plugins.h"
#include "cpu_state.h"
#include "spu.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {
constexpr auto Plugin = "v82.original-cars";
constexpr auto Package = "v82.content.original-cars";
constexpr uint32_t Meta = 0x80300000u, Entries = Meta + 0x1000, Banks = Meta + 0x3000;
constexpr uint32_t Objects = Meta + 0x5000, Reservations = Meta + 0x6000;
constexpr uint32_t AllocationMap = 0x80320000u;
constexpr uint32_t HeapLo = 0x80400000u, HeapHi = 0x80800000u, Magic = 0x56383248u;
constexpr uint32_t Dispatch = 0x8FFF0000u, WheelSlot = 0x800C61C0u;
constexpr uint32_t Alloc = 0x80052060u, Free = 0x800520D8u, Realloc = 0x80052188u;
constexpr uint32_t Build = 0x8001E914u, Destroy = 0x8001EC18u, Clone = 0x80031DDCu;
constexpr uint32_t Create = 0x80036C2Cu, Resolve = 0x80036B64u, Generic = 0x800367A4u;
constexpr uint32_t Common = 0x80033234u, VramReset = 0x8002091Cu, VramAlloc = 0x80020A80u;
constexpr uint32_t VramFree = 0x80020DF0u, Teardown = 0x800333D0u;
constexpr uint32_t DrawOTag = 0x8005C57Cu;
constexpr uint32_t LoadCompressedTexture = 0x80021064u;
constexpr uint32_t VehicleEvent = 0x8003C118u;
constexpr uint32_t Transform = 0x8003E32Cu, Pickup = 0x80049D54u;
v82::VehiclePackage package;
int selected = -1, constructing = -1;
bool enabled = false;
#ifndef PSX_NO_DEBUG_TOOLS
bool test_original_special = false;
bool test_defeat = false;
bool test_transform = false;
#endif
std::vector<uint32_t> bypass;
struct Request { std::array<uint32_t, 6> args; v82::Bytes pixels; bool compressed; };
std::vector<Request> requests;
struct Hook { uint32_t pc, word; };
constexpr Hook Hooks[] = {
    {Alloc, 0x1080001Bu}, {Free, 0x10800029u}, {Realloc, 0x1080FFB5u},
    {Resolve, 0x27BDFFF8u}, {Create, 0x27BDFFE8u}, {Common, 0x27BDFFF0u},
    {VramReset, 0x27BDFFF0u}, {VramAlloc, 0x27BDFFC8u}, {VramFree, 0x27BDFFF0u},
    {Teardown, 0x8F841124u}, {VehicleEvent, 0x27BDFFE8u},
    {Transform, 0x27BDFFE0u}, {Pickup, 0x27BDFF20u},
    {0x8002C590u, 0x27BDFFF8u}, {0x80041B0Cu, 0x27BDFFE0u},
    {0x8003E774u, 0x27BDFFE0u}, {0x8003E8C0u, 0x27BDFF90u},
    {0x8003EDC4u, 0x27BDFEF8u}, {0x8003FCC4u, 0x27BDFF68u},
    {0x80040598u, 0x27BDFF40u}, {0x80040F10u, 0x27BDFEF0u},
    {LoadCompressedTexture, 0x27BDF390u},
    {DrawOTag, 0x3C028007u}, {0x80038398u, 0x8482000Au},
    {0x8005FA9Cu, 0xAFA50004u}, {0x8001DA14u, 0x27BDFFE0u}, {0x80018210u, 0x27BDFFD8u},
};
// Before the consumer instruction, after the MIPS load delay where relevant.
// Mappings come from apply_vehicle_engine_patches.py at the pinned reference.
constexpr Hook InstructionHooks[] = {
    {0x8003CA3Cu, 0x3402FFFFu}, // independent upgrade status
    {0x8003CBB8u, 0x90A30014u}, // authored stats
    {0x8003E5B4u, 0x8632000Au}, // normal wheel bank
    {0x8003E5D4u, 0x00161840u}, // transformation table
    {0x8003E5E0u, 0x00541021u}, // transformed wheel bank
    {0x8003E69Cu, 0x90820007u}, // wheel upgrade status
    {0x8004AEE0u, 0x10600008u}, // declared native special dependency
    {0x8004AF0Cu, 0x16000003u}, // original Houston/Y special callback
};

uint32_t rd(uint32_t p) { return psx_mod_read_word(p); }
void wr(uint32_t p, uint32_t v) { psx_mod_write_word(p, v); }
uint32_t state(unsigned i) { return Entries + i * 128; }
uint32_t bank_state(unsigned i) { return Banks + i * 32; }
[[noreturn]] void fail(const char* counter) { psx_mod_counter_add(counter, 1); std::abort(); }
void copy(uint32_t to, v82::Bytes from) {
    for (uint32_t i = 0; i < from.size; ++i) psx_mod_write_byte(to + i, from.data[i]);
}
void init() {
    if (rd(Meta) == Magic) return;
    for (uint32_t p = Meta; p < Meta + 0x10000; p += 4) wr(p, 0);
    for(uint32_t p=AllocationMap;p<AllocationMap+0x10000;p+=4)wr(p,0);
    wr(Meta, Magic); wr(Meta + 4, HeapLo);
    wr(HeapLo, 0); wr(HeapLo + 4, (HeapHi - HeapLo) / 8);
    for (unsigned i = 0; i < package.vehicles.size(); ++i) {
        const auto& v = package.vehicles[i];
        copy(state(i) + 8, {v.stats.data(), 48});
        copy(state(i) + 56, {v.transform_table.data(), 48});
        copy(state(i) + 104, {v.powerups.data(), 20});
    }
}
void allocation_bit(uint32_t header,bool allocated) {
    const auto bit=(header-HeapLo)/8, address=AllocationMap+bit/8;
    const auto mask=uint8_t(1u<<(bit&7));auto value=psx_mod_read_byte(address);
    psx_mod_write_byte(address,allocated?uint8_t(value|mask):uint8_t(value&~mask));
}
uint32_t allocate(uint32_t bytes) {
    init();
    if (!bytes || bytes > HeapHi - HeapLo - 8) return 0;
    uint32_t units = (bytes + 15) / 8, link = Meta + 4, node = rd(link);
    while (node) {
        const uint32_t size = rd(node + 4);
        if (size >= units) {
            const uint32_t header = node + (size - units) * 8;
            if (size == units) wr(link, rd(node)); else wr(node + 4, size - units);
            wr(header, Magic); wr(header + 4, units); allocation_bit(header,true);
            return header + 8;
        }
        link = node; node = rd(node);
    }
    psx_mod_counter_add("v82.heap.exhausted", 1);
    return 0;
}
bool owned(uint32_t p) {
    if(p<HeapLo+8||p>=HeapHi||(p&7))return false;
    const auto bit=(p-8-HeapLo)/8;
    return (psx_mod_read_byte(AllocationMap+bit/8)&(1u<<(bit&7)))!=0;
}
void release(uint32_t p) {
    if (!p) return;
    if (!owned(p)) fail("v82.heap.invalid_free");
    // Cloning replaces the factory callback with the native vehicle handler.
    // Retire identity by allocation address, including transformed clones.
    for (unsigned i = 0; i < 128; ++i)
        if (rd(Objects + i * 32) == p) wr(Objects + i * 32, 0);
    const uint32_t node = p - 8;
    allocation_bit(node,false);
    uint32_t link = Meta + 4, next = rd(link), previous = 0;
    while (next && next < node) { previous = next; link = next; next = rd(next); }
    wr(node, next); wr(link, node);
    if (next && node + rd(node + 4) * 8 == next) {
        wr(node + 4, rd(node + 4) + rd(next + 4)); wr(node, rd(next));
    }
    if (previous && previous + rd(previous + 4) * 8 == node) {
        wr(previous + 4, rd(previous + 4) + rd(node + 4)); wr(previous, rd(node));
    }
}
uint32_t load_bytes(v82::Bytes bytes) {
    if (!bytes.data) return 0;
    const auto p = allocate(bytes.size);
    if (!p) fail("v82.bank.out_of_memory");
    copy(p, bytes); return p;
}
uint32_t call(CPUState* cpu, uint32_t fn, uint32_t a0 = 0, uint32_t a1 = 0,
              uint32_t a2 = 0, uint32_t a3 = 0) {
    const CPUState saved = *cpu;
    cpu->pc = 0; cpu->gpr[4] = a0; cpu->gpr[5] = a1; cpu->gpr[6] = a2; cpu->gpr[7] = a3;
    psx_dispatch_call(cpu, fn, saved.gpr[31]);
    const uint32_t result = cpu->gpr[2]; *cpu = saved; return result;
}
void original(CPUState* cpu, uint32_t fn) {
    bypass.push_back(fn);
    const uint32_t ra = cpu->gpr[31]; cpu->pc = 0;
    psx_dispatch_call(cpu, fn, ra);
    cpu->gpr[31] = ra; bypass.pop_back();
}
struct WordScope {
    uint32_t address, previous;
    WordScope(uint32_t a, uint32_t value) : address(a), previous(rd(a)) { wr(a, value); }
    ~WordScope() { wr(address, previous); }
};
void remember(uint32_t object, unsigned index) {
    uint32_t empty = 0;
    for (unsigned i = 0; i < 128; ++i) {
        auto p = Objects + i * 32;
        if (rd(p) == object) { wr(p + 4, index); return; }
        if (!rd(p) && !empty) empty = p;
    }
    if (!empty) fail("v82.objects.full");
    for (unsigned off = 8; off < 32; off += 4) wr(empty + off, 0);
    wr(empty, object); wr(empty + 4, index);
}
int object_entry(uint32_t object) {
    for (unsigned i = 0; i < 128; ++i)
        if (rd(Objects + i * 32) == object && object) return int(rd(Objects + i * 32 + 4));
    const unsigned type = psx_mod_read_byte(object + 0xDC);
    return type >= 64 && type - 64 < package.vehicles.size() ? int(type - 64) : -1;
}
uint32_t upgrade_status(uint32_t object, unsigned index) {
    remember(object, index);
    for (unsigned i = 0; i < 128; ++i)
        if (rd(Objects + i * 32) == object) return Objects + i * 32 + 8;
    fail("v82.upgrade.missing_identity");
}
uint32_t build_bank(CPUState* cpu, unsigned index);
#include "v82_original_specials.inc"
#include "v82_lightning.inc"
#include "v82_flying_controller.inc"
#include "v82_special_roster.inc"
void instruction(CPUState* cpu, uint32_t address) {
    if (!enabled) return;
    const uint32_t pc = (address & 0x1FFFFFFFu) | 0x80000000u;
    const uint32_t object = cpu->gpr[pc == 0x8004AEE0u || pc == 0x8004AF0Cu ? 18 : 21];
    const int index = object_entry(object);
    if (index < 0) return;
    const auto& v = package.vehicles[index];
    switch (pc) {
    case 0x8003CA3Cu: case 0x8003E69Cu:
        cpu->gpr[4] = upgrade_status(object, unsigned(index));
        psx_mod_counter_add("v82.vehicle.upgrade_status", 1); break;
    case 0x8003CBB8u:
        cpu->gpr[5] = state(index) + 8;
        psx_mod_counter_add("v82.vehicle.stats", 1); break;
    case 0x8003E5D4u:
        cpu->gpr[4] = state(index) + 56;
        psx_mod_counter_add("v82.vehicle.transform_table", 1); break;
    case 0x8003E5B4u: case 0x8003E5E0u:
        if (rd(state(index) + 4)) cpu->gpr[19] = rd(state(index) + 4);
        psx_mod_counter_add("v82.vehicle.wheel_bank", 1); break;
    case 0x8004AEE0u:
        cpu->gpr[3] = v.flags & (1u << 16) ? rd(0x800C6130u + ((v.flags >> 17) & 31) * 4) : 0;
        psx_mod_counter_add("v82.vehicle.special_behavior", 1); break;
    case 0x8004AF0Cu:
        if (v.flags & (1u << 23)) {
            const auto kind = (v.flags >> 17) & 31;
            if (kind == 1) cpu->gpr[17] = lightning::Weapon;
            else if (kind == 7) cpu->gpr[17] = original_specials::Houston;
            else if (kind == 12) cpu->gpr[17] = original_specials::Alien;
            else if (kind < 13) cpu->gpr[17] = special_roster::Weapon + kind * 4;
            psx_mod_counter_add("v82.vehicle.original_special", 1);
        }
        break;
    }
}
uint32_t build_bank(CPUState* cpu, unsigned index) {
    auto slot = bank_state(index);
    if (rd(slot)) return rd(slot);
    const auto& source = package.banks.at(index);
    const auto bin = load_bytes(source.bin), anm = load_bytes(source.animation);
    wr(slot + 4, bin); wr(slot + 8, source.bin.size); wr(slot + 12, anm);
    const auto runtime = call(cpu, Build, bin, anm);
    if (!runtime) fail("v82.bank.build_failed");
    if (source.sound.data && index != package.original_effects_bank) {
        const uint32_t count = source.sound.u16(0), bytes = source.sound.u16(2) * 8u;
        const auto address = call(cpu, 0x80052F9Cu, (bytes + 63) & ~63u);
        if (!address || address >= 0x80000u || bytes > 0x80000u - address) fail("v82.sound.allocation_failed");
        const uint32_t handle = allocate(4 + count * 4);
        if (!handle) fail("v82.sound.handle_failed");
        psx_mod_write_half(handle, count); psx_mod_write_half(handle + 2, uint16_t(address >> 3));
        for (unsigned i = 0; i < count; ++i) {
            psx_mod_write_half(handle + 4 + i * 4, uint16_t((address >> 3) + source.sound.u16(4 + i * 4)));
            psx_mod_write_half(handle + 6 + i * 4, source.sound.u16(6 + i * 4));
        }
        const auto transfer = spu_read(0x1F801DA6u);
        spu_write(0x1F801DA6u, address >> 3);
        for (uint32_t i = 0; i < bytes; i += 4) spu_dma_write(source.sound.u32(4 + count * 4 + i));
        spu_write(0x1F801DA6u, transfer);
        wr(runtime + 12, handle);
        psx_mod_counter_add("v82.sound.bytes", bytes);
    }
    wr(slot, runtime);
    psx_mod_counter_add("v82.bank.built", 1);
    return runtime;
}
void ensure(CPUState* cpu, unsigned index) {
    auto p = state(index);
    if (rd(p)) return;
    const auto& v = package.vehicles.at(index);
    wr(p, build_bank(cpu, v.body));
    if (v.transform != 65535) wr(p + 4, build_bank(cpu, v.transform));
}
void dispatch(CPUState* cpu, uint32_t) {
    int index = constructing >= 0 ? constructing : object_entry(cpu->gpr[4]);
    if (index < 0) fail("v82.dispatch.missing_identity");
    ensure(cpu, unsigned(index));
    const uint32_t ra = cpu->gpr[31];
    // Scoped native table selection is equivalent to the reference's two wheel
    // pointer seams while this object's generic event handler is executing.
    WordScope wheel(WheelSlot, rd(state(index) + 4) ? rd(state(index) + 4) : rd(WheelSlot));
    cpu->gpr[7] = state(index) + 8; cpu->pc = 0;
    psx_dispatch_call(cpu, Generic, ra);
    cpu->gpr[31] = ra;
    psx_mod_counter_add("v82.vehicle.events", 1);
}
void create(CPUState* cpu, unsigned index) {
    const auto source = cpu->gpr[4]; const int32_t variant = int32_t(cpu->gpr[6]);
    if (!source) { cpu->gpr[2] = 0; psx_mod_finish_function(cpu); return; }
    ensure(cpu, index);
    const auto& v = package.vehicles.at(index);
    remember(source, index); wr(source, Dispatch);
    psx_mod_write_half(source + 0x1A, v.body_kind); wr(source + 0x5C, rd(state(index)));
    const int previous = constructing; constructing = int(index);
    const auto vehicle = call(cpu, Clone, source); constructing = previous;
    if (!vehicle) fail("v82.vehicle.clone_failed");
    if (!v.flying()) {
        unsigned wheels = 0;
        for (unsigned i = 0; i < 6; ++i) {
            auto wheel = rd(vehicle + 0x104 + i * 4);
            if (!wheel) continue;
            const unsigned axle = i / 2;
            const auto spring = uint16_t(v.stats[4 + axle * 2] | (v.stats[5 + axle * 2] << 8));
            const auto damping = axle == 2 ? v.rear_damping : uint16_t(v.stats[8 + axle * 2] | (v.stats[9 + axle * 2] << 8));
            psx_mod_write_half(wheel + 0x8C, spring); psx_mod_write_half(wheel + 0x8E, damping); ++wheels;
        }
        if (!wheels) fail("v82.vehicle.missing_wheels");
        psx_mod_counter_add("v82.vehicle.wheels", wheels);
    }
    for (unsigned off : {0xF8u, 0xFCu}) if (auto child = rd(vehicle + off))
        psx_mod_write_half(child + 10, psx_mod_read_half(vehicle + 10));
    if (variant > 0) {
        wr(vehicle + 0x80, uint32_t((int16_t(psx_mod_read_half(vehicle + 0x24)) * 4577) >> 5));
        wr(vehicle + 0x84, 0);
        wr(vehicle + 0x88, uint32_t((int16_t(psx_mod_read_half(vehicle + 0x30)) * 4577) >> 5));
    }
    remember(vehicle, index); psx_mod_write_byte(vehicle + 0xDC, uint8_t(64 + index));
    cpu->gpr[2] = vehicle; psx_mod_finish_function(cpu);
    psx_mod_counter_add("v82.vehicle.created", 1);
    wr(Meta + 16, vehicle);
    wr(Meta + 20, uint32_t(variant));
}

bool same_request(const Request& a, const Request& b) {
    return a.args == b.args && a.compressed == b.compressed && a.pixels.size == b.pixels.size &&
        !std::memcmp(a.pixels.data, b.pixels.data, a.pixels.size);
}
void add_request(Request request) {
    if (std::none_of(requests.begin(), requests.end(), [&](const auto& r) { return same_request(r, request); }))
        requests.push_back(request);
}
void scan_bank(const v82::Bank& bank) {
    const auto b = bank.bin;
    const auto count = b.u32(16), table = b.u32(20);
    if (count > 256) throw std::runtime_error("too many bank textures");
    b.sub(table, count * 4);
    for (unsigned i = 0; i < count; ++i) {
        const uint64_t d = uint64_t(table) + b.u32(table + i * 4);
        if (d > UINT32_MAX) throw std::runtime_error("texture offset overflow");
        const auto descriptor = uint32_t(d), flags = b.u32(descriptor + 4), depth = flags & 3u;
        if (depth > 2) throw std::runtime_error("invalid texture depth");
        if (depth < 2) {
            const uint32_t pw = b.u16(descriptor + 16), ph = b.u16(descriptor + 18);
            if (!pw || !ph || pw * ph > 256) throw std::runtime_error("invalid CLUT");
            add_request({{pw, ph, 16, 1, pw, 1}, b.sub(descriptor + 20, pw * ph * 2), false});
        }
        const uint64_t image64 = depth == 2 ? d : d + b.u32(descriptor + 8);
        if (image64 > UINT32_MAX) throw std::runtime_error("image offset overflow");
        const uint32_t image = uint32_t(image64), width = b.u16(image + 16), height = b.u16(image + 18);
        const uint32_t size = b.u32(image + 8);
        if (!width || !height || size < 12) throw std::runtime_error("invalid image");
        add_request({{width, height, 64, 256, 64u << depth, 256}, b.sub(image + 20, size - 12), (flags & 16) != 0});
    }
}
#include "v82_presentation.inc"
#include "v82_native_selector.inc"
void prepare_transform_test_roster(CPUState* cpu) {
#ifndef PSX_NO_DEBUG_TOOLS
    // Keep the isolated pickup fixture within the native VRAM budget by
    // sharing one retail opponent bank. Participant counts remain native.
    if (test_transform) for (unsigned i = 1; i < 6; ++i) {
        const auto p = cpu->gpr[28] + 0x1104 + i;
        if (int8_t(psx_mod_read_byte(p)) >= 0) psx_mod_write_byte(p, 0);
    }
#endif
}
void prepare_match_textures(CPUState* cpu) {
    requests.clear();selector::apply_npcs(cpu);
    prepare_transform_test_roster(cpu);
    if(selected>=0)psx_mod_write_byte(cpu->gpr[28]+0x1104,uint8_t(64+selected));
    for(unsigned i=0;i<6;++i) {
        const unsigned type=psx_mod_read_byte(cpu->gpr[28]+0x1104+i);
        if(type<64||type-64>=package.vehicles.size())continue;
        const auto& v=package.vehicles[type-64];scan_bank(package.banks[v.body]);
        if(v.transform!=65535)scan_bank(package.banks[v.transform]);
        const unsigned kind=(v.flags>>17)&31;
        if((v.flags&(1u<<23))&&kind<13&&package.special_effect_banks[kind]!=65535)scan_bank(package.banks[package.special_effect_banks[kind]]);
    }
    std::stable_sort(requests.begin(),requests.end(),[](const auto& a,const auto& b) {
        if(a.args[2]!=b.args[2])return a.args[2]<b.args[2];
        if(a.args[0]!=b.args[0])return a.args[0]>b.args[0];
        return a.args[1]>b.args[1];
    });
    if(requests.size()>512)fail("v82.vram.too_many_requests");
}
void reserve(CPUState* cpu) {
    prepare_match_textures(cpu);
    if (rd(Meta + 8)) fail("v82.vram.stale_reservations");
    const uint32_t sp = cpu->gpr[29], stack10 = rd(sp + 16), stack14 = rd(sp + 20);
    for (unsigned i = 0; i < requests.size(); ++i) {
        const auto& r = requests[i];
        wr(sp + 16, r.args[4]); wr(sp + 20, r.args[5]);
        const auto node = call(cpu, VramAlloc, r.args[0], r.args[1], r.args[2], r.args[3]);
        if (!node) fail("v82.vram.reservation_failed");
        auto descriptor = Reservations + i * 32;
        for (unsigned j = 0; j < 24; j += 4) wr(descriptor + j, 0);
        wr(descriptor, rd(node));
        psx_mod_write_half(descriptor + 4, uint16_t(r.args[0]));
        psx_mod_write_half(descriptor + 6, uint16_t(r.args[1]));
        wr(descriptor + 8, 1); wr(descriptor + 24, node); wr(descriptor + 28, 0);
    }
    wr(sp + 16, stack10); wr(sp + 20, stack14);
    wr(Meta + 8, uint32_t(requests.size()));
    wr(Meta + 40, 1); // match VRAM has been initialized, including retail rosters
    psx_mod_counter_add("v82.vram.reserved", uint32_t(requests.size()));
}
bool texture_owned(uint32_t pointer) {
    for (unsigned i = 0; i < package.banks.size(); ++i) {
        const auto p = bank_state(i), base = rd(p + 4), size = rd(p + 8);
        if (base && pointer >= base && pointer - base < size) return true;
    }
    return false;
}
// Imported wheel textures are compressed. The native transform animation
// loads them while using the 1 KiB scratchpad stack, but 80021064 reserves
// 0xC70 bytes before calling GPU upload helpers. Give that synchronous call
// a guest RAM stack; keep its native decoder, uploads and return values.
// Normal main-RAM calls and textures outside our package retain their path.
bool load_compressed_texture(CPUState* cpu) {
    const auto sp = cpu->gpr[29];
    if (sp < 0x1F800000u || sp > 0x1F800400u || !texture_owned(cpu->gpr[5])) return false;
    constexpr uint32_t StackBytes = 0x2000;
    const auto stack = allocate(StackBytes);
    if (!stack) fail("v82.texture.stack_allocation_failed");
    cpu->gpr[29] = stack + StackBytes - 16;
    original(cpu, LoadCompressedTexture);
    cpu->gpr[29] = sp;
    release(stack);
    psx_mod_counter_add("v82.texture.scratchpad_stack_redirected", 1);
    psx_mod_finish_function(cpu);
    return true;
}
bool claim(CPUState* cpu) {
    if (!rd(Meta + 8)) return false;
    const bool palette = cpu->gpr[6] == 16 && cpu->gpr[7] == 1;
    const auto gp = cpu->gpr[28];
    if (!texture_owned(rd(gp + (palette ? 0xE6C : 0xE74)))) return false;
    const std::array<uint32_t, 6> args{cpu->gpr[4], cpu->gpr[5], cpu->gpr[6], cpu->gpr[7],
                                     rd(cpu->gpr[29] + 16), rd(cpu->gpr[29] + 20)};
    const uint32_t data = rd(gp + (palette ? 0xE70 : 0xE78));
    const bool compressed = !palette && (rd(gp + 0xE68) & 16);
    for (unsigned i = 0; i < requests.size(); ++i) {
        const auto& r = requests[i];
        if (r.args != args || r.compressed != compressed) continue;
        bool matches = true;
        for (uint32_t j = 0; j < r.pixels.size && matches; ++j)
            matches = psx_mod_read_byte(data + j) == r.pixels.data[j];
        if (!matches) continue;
        cpu->gpr[2] = Reservations + i * 32;
        wr(cpu->gpr[2] + 28, rd(cpu->gpr[2] + 28) + 1);
        psx_mod_counter_add("v82.vram.claimed", 1); return true;
    }
    fail("v82.vram.unreserved_texture");
}
void common(CPUState* cpu) {
    const uint32_t gp = cpu->gpr[28];
    selector::apply_npcs(cpu);
    prepare_transform_test_roster(cpu);
    if(selected>=0)psx_mod_write_byte(gp+0x1104,uint8_t(64+selected));
    uint32_t mask = 0x001C0000u, alternate = 0;
    const unsigned players = int8_t(psx_mod_read_byte(gp + 0x31)) < 9 ? 2 : 4;
    for (unsigned i = 0; i < 6; ++i) {
        const int type = int8_t(psx_mod_read_byte(gp + 0x1104 + i));
        if(type>=64&&unsigned(type-64)<package.vehicles.size()) {
            const auto flags=package.vehicles[type-64].flags;
            if(flags&(1u<<16))mask|=1u<<((flags>>17)&31);
            continue;
        }
        if(type<0)continue;
        if (type >= 21) { if (type - 21 < 32) alternate |= 1u << (type - 21); continue; }
        if (i < players) {
            const auto p = 0x8006B9E8u + i * 188 + unsigned(type) * 10;
            bool upgraded = true;
            for (unsigned j = 6; j <= 9; ++j) upgraded &= psx_mod_read_byte(p + j) >= 100;
            (upgraded ? alternate : mask) |= 1u << type;
        } else if (psx_mod_read_byte(0x8006B802u + i) || int8_t(psx_mod_read_byte(gp + 0x31)) == 3)
            mask |= 1u << type;
    }
    cpu->gpr[4] = mask; cpu->gpr[19] = mask; cpu->gpr[18] = alternate;
    psx_mod_counter_add("v82.match.selected", 1);
}
void teardown(CPUState* cpu) {
    original(cpu, Teardown);
    special_roster::release_sounds(cpu);
    for (unsigned i = 0; i < package.banks.size(); ++i) {
        auto p = bank_state(i);
        if (auto runtime = rd(p)) { call(cpu, Destroy, runtime); wr(p, 0); wr(p + 4, 0); wr(p + 8, 0); }
    }
    for (unsigned i = 0; i < package.vehicles.size(); ++i) { wr(state(i), 0); wr(state(i) + 4, 0); }
    for (unsigned i = 0; i < 128; ++i) wr(Objects + i * 32, 0);
    // Backing rectangles belong to the native allocator tree; synthetic
    // handles remain valid until all imported bank destructors have run.
    wr(Meta + 8, 0);
    wr(Meta + 40, 0);
    psx_mod_counter_add("v82.match.released", 1);
    psx_mod_finish_function(cpu);
}

#include "v82_draw_packets.inc"
void entry(CPUState* cpu, uint32_t address) {
    address = (address & 0x1FFFFFFFu) | 0x80000000u;
    if (!enabled || std::find(bypass.begin(), bypass.end(), address) != bypass.end()) return;
    const auto guard = std::find_if(std::begin(Hooks), std::end(Hooks),
                                   [=](const auto& hook) { return hook.pc == address; });
    if (guard == std::end(Hooks) || rd(address) != guard->word) {
        psx_mod_counter_add("v82.hook.word_mismatch", 1); return;
    }
    init();
    if (address == 0x80038398u && presentation::object_name(cpu)) return;
    if (presentation::result_entry(cpu,address)) return;
    if (address == LoadCompressedTexture && load_compressed_texture(cpu)) return;
    if (address == DrawOTag) { draw_packets::prepare(cpu); return; }
    if (flight::entry(cpu, address)) return;
    if (address == Alloc) { cpu->gpr[2] = allocate(cpu->gpr[4]); psx_mod_finish_function(cpu); }
    else if (address == Free) {
        if (!cpu->gpr[4] || owned(cpu->gpr[4])) { release(cpu->gpr[4]); psx_mod_finish_function(cpu); }
        else if(cpu->gpr[4]>=HeapLo&&cpu->gpr[4]<HeapHi)fail("v82.heap.invalid_guest_free");
    } else if (address == Realloc) {
        const auto p = cpu->gpr[4], size = cpu->gpr[5];
        if (p && !owned(p)) return;
        // Match V82Compat.PcRealloc: shrinking retains the allocation and all
        // pointers into it. Native loaders retain aliases to these blocks.
        if (p && size && size <= rd(p - 4) * 8 - 8) {
            cpu->gpr[2] = p; psx_mod_finish_function(cpu);
            psx_mod_counter_add("v82.heap.realloc_retained", 1); return;
        }
        const auto result = allocate(size);
        if (result && p) for (uint32_t i = 0; i < std::min(size, rd(p - 4) * 8 - 8); ++i)
            psx_mod_write_byte(result + i, psx_mod_read_byte(p + i));
        if (p && (result || !size)) release(p);
        cpu->gpr[2] = result; psx_mod_finish_function(cpu);
    } else if (address == Resolve) {
        if (cpu->gpr[4] >= 64 && cpu->gpr[4] - 64 < package.vehicles.size()) {
            cpu->gpr[2] = Dispatch; psx_mod_finish_function(cpu);
        }
    } else if (address == Create) {
        if (cpu->gpr[5] >= 64 && cpu->gpr[5] - 64 < package.vehicles.size()) create(cpu, cpu->gpr[5] - 64);
    } else if (address == VehicleEvent) {
        if (object_entry(cpu->gpr[4]) >= 0) {
            psx_mod_counter_add("v82.vehicle.native_events", 1);
            wr(Meta + 24, cpu->gpr[4]); wr(Meta + 28, cpu->gpr[5]);
#ifndef PSX_NO_DEBUG_TOOLS
            // Reference V82Compat.UpdateSoakWeapon: exercise the actual native
            // attachment path in an explicitly requested isolated playtest.
            const auto object = cpu->gpr[4];
            // Reference V82Compat.UpdateDefeatRegression: let native lethal
            // damage own the wreck, notification and result transition.
            // Explicit isolated-test opt-in; never enabled by a mod setting.
            if (test_defeat && cpu->gpr[5] == 0 && rd(Meta + 40) &&
                object == rd(0x8006B7E8u) && rd(Meta + 44) < 120) {
                const auto ticks = rd(Meta + 44) + 1;
                wr(Meta + 44, ticks);
                if (ticks == 120) {
                    original_specials::invoke(cpu, 0x80039DCCu, object,
                                             uint32_t(-0x7FFF), object + 0x34, 1);
                    psx_mod_counter_add("v82.test.native_defeat", 1);
                    psx_mod_finish_function(cpu); return;
                }
            }
            if (test_transform && cpu->gpr[5] == 0 && rd(Meta + 40) &&
                object == rd(0x8006B7E8u)) {
                // Reference MaintainSoakVehicle: keep native durability intact
                // so opponents do not end the isolated pickup lifecycle test.
                const auto health = psx_mod_read_half(object + 0x1E);
                psx_mod_write_half(object + 0x1C, health);
                for (unsigned offset : {0xF8u, 0xFCu, 0x100u}) {
                    const auto zone = rd(object + offset);
                    if (zone >= HeapLo && zone < HeapHi - 0x20)
                        psx_mod_write_half(zone + 0x1C, health);
                }
            }
            if (test_transform && cpu->gpr[5] == 0 && rd(Meta + 40) &&
                object == rd(0x8006B7E8u) && rd(Meta + 48)) {
                const auto command = rd(Meta + 48); wr(Meta + 48, 0);
                if (command >= 0x81 && command <= 0x83) {
                    // Native dropped pickup + its real collision callback.
                    const auto hit = allocate(64);
                    for (unsigned i = 0; i < 64; i += 4) wr(hit + i, 0);
                    wr(hit, object);
                    const auto pickup = original_specials::invoke(cpu, 0x8004AA24u,
                        command - 0x80 + 6, object + 0x34, hit + 16);
                    wr(Meta + 60, pickup);
                    const auto result = original_specials::invoke(cpu, Pickup, pickup, 3, hit);
                    wr(Meta + 56, result);
                    if (!result) original_specials::invoke(cpu, 0x800309A0u, pickup);
                    release(hit);
                    psx_mod_counter_add("v82.test.pickup_collision", 1);
                } else original_specials::invoke(cpu, Transform, object, command - 1, 600);
                wr(Meta + 52, rd(Meta + 52) + 1);
                psx_mod_counter_add("v82.test.transform_requested", 1);
            }
            if (test_original_special && cpu->gpr[5] == 0 && rd(Meta + 32) != object &&
                int16_t(psx_mod_read_half(object + 10)) < 0 &&
                (package.vehicles[object_entry(object)].flags & (1u << 23))) {
                wr(Meta + 32, object);
                original_specials::invoke(cpu, 0x8003A500, object, 1u << 31);
                const auto weapon = rd(object + 0x120);
                wr(Meta + 36, weapon);
                if (!weapon) fail("v82.test.special_attach_failed");
                psx_mod_counter_add("v82.test.special_attached", 1);
            }
#endif
        }
    } else if (address == Transform) {
        const int index = object_entry(cpu->gpr[4]);
        if (cpu->gpr[5] && index >= 0 && !package.vehicles[index].transforms()) {
            cpu->gpr[2] = 0; psx_mod_finish_function(cpu);
            psx_mod_counter_add("v82.vehicle.transform_rejected", 1);
        }
    } else if (address == Pickup) {
        if (cpu->gpr[5] == 3 && cpu->gpr[4] && cpu->gpr[6]) {
            const auto kind = psx_mod_read_half(cpu->gpr[4] + 0x1A);
            const auto index = object_entry(rd(cpu->gpr[6]));
            if (kind >= 7 && kind <= 9 && index >= 0 && !package.vehicles[index].transforms()) {
                cpu->gpr[2] = 0; psx_mod_finish_function(cpu);
                psx_mod_counter_add("v82.vehicle.pickup_rejected", 1);
            }
        }
    } else if (address == Common) common(cpu);
    else if (address == VramReset && cpu->gpr[31] == 0x800137A0u) {
        original(cpu, address); reserve(cpu); psx_mod_finish_function(cpu);
    } else if (address == VramAlloc) { if (claim(cpu)) psx_mod_finish_function(cpu); }
    else if (address == VramFree && cpu->gpr[4] >= Reservations &&
             cpu->gpr[4] < Reservations + requests.size() * 32 && !((cpu->gpr[4] - Reservations) & 31))
        psx_mod_finish_function(cpu);
    else if (address == Teardown) teardown(cpu);
}
void activate() {
    enabled = false; selected = -1; package = {}; requests.clear(); bypass.clear();
    presentation::ready=false;
#ifndef PSX_NO_DEBUG_TOOLS
    const char* test_special = std::getenv("PSX_V82_TEST_ORIGINAL_SPECIAL");
    test_original_special = test_special && !std::strcmp(test_special, "1");
    const char* transform_test = std::getenv("PSX_V82_TEST_TRANSFORM");
    test_transform = transform_test && !std::strcmp(transform_test, "1");
    const char* defeat = std::getenv("PSX_V82_TEST_DEFEAT");
    test_defeat = defeat && !std::strcmp(defeat, "1");
#endif
    const uint8_t *r = nullptr, *a = nullptr; uint64_t rs = 0, as = 0;
    if (!psx_mod_current_resource_bytes("registry", &r, &rs) ||
        !psx_mod_current_resource_bytes("archive", &a, &as) || rs > UINT32_MAX || as > UINT32_MAX) {
        psx_mod_counter_add("v82.activate.missing_package", 1); return;
    }
    try {
        package = v82::read_vehicle_package({r, uint32_t(rs)}, {a, uint32_t(as)});
        const uint8_t* identity=nullptr;uint64_t size=0;
        if(!psx_mod_current_resource_bytes("presentation",&identity,&size)||size>UINT32_MAX)
            throw std::runtime_error("missing original presentation");
        presentation::load({identity,uint32_t(size)});
        if(!psx_mod_current_resource_bytes("outcomes",&identity,&size)||size>UINT32_MAX)
            throw std::runtime_error("missing original outcome speech");
        presentation::load_outcomes({identity,uint32_t(size)});
    }
    catch (...) { psx_mod_counter_add("v82.activate.invalid_package", 1); return; }
#ifndef PSX_NO_DEBUG_TOOLS
    const char* choice=std::getenv("PSX_V82_TEST_VEHICLE");
    for(unsigned i=0;choice&&i<package.vehicles.size();++i)
        if(package.vehicles[i].id==choice)selected=int(i);
#endif
    if (selected >= 0) try {
        const auto& v = package.vehicles[selected];
        scan_bank(package.banks[v.body]);
        if (v.transform != 65535) scan_bank(package.banks[v.transform]);
        const unsigned kind=(v.flags>>17)&31;
        if((v.flags&(1u<<23))&&kind<13&&package.special_effect_banks[kind]!=65535)scan_bank(package.banks[package.special_effect_banks[kind]]);
        std::stable_sort(requests.begin(), requests.end(), [](const auto& a, const auto& b) {
            if (a.args[2] != b.args[2]) return a.args[2] < b.args[2];
            if (a.args[0] != b.args[0]) return a.args[0] > b.args[0];
            return a.args[1] > b.args[1];
        });
        if (requests.size() > 512) throw std::runtime_error("reservation table full");
    } catch (...) { psx_mod_counter_add("v82.activate.invalid_textures", 1); return; }
    if (!psx_mod_set_main_ram_8mb(1)) { psx_mod_counter_add("v82.activate.ram_failed", 1); return; }
    if(!psx_mod_append_disc_extent("outcomes",216,(presentation::outcomes.size-216)/2336,&presentation::outcome_lba)) {
        psx_mod_counter_add("v82.activate.outcome_mount_failed",1);return;
    }
    enabled = true;
    psx_mod_counter_add("v82.activate.vehicles", uint32_t(package.vehicles.size()));
}
PSX_MOD_CONSTRUCTOR(register_combined) {
    psx_mod_register_activation_plugin(Plugin, activate);
    original_specials::register_callbacks();
    lightning::register_callbacks();
    special_roster::register_callbacks();
    // SHELL is supplied by the shared menu feature, so even a retail-only roster
    // needs these callbacks to forward its native menu calls unchanged.
    for(uint32_t i=0;i<26;++i)
        psx_mod_register_guest_function_plugin("v82.content-files",selector::Host+i*4,selector::callback);
    psx_mod_register_guest_function_plugin(Plugin, Dispatch, dispatch);
    for (const auto& hook : Hooks)
        psx_mod_register_function_entry_plugin(Plugin, hook.pc, entry);
    for (const auto& hook : InstructionHooks)
        psx_mod_register_instruction_plugin(Plugin, hook.pc, hook.word, instruction);
}
}
extern "C" bool v82_presentation_file(const uint8_t** data,uint64_t* size) {
    if(!enabled||!presentation::ready)return false;
    *data=presentation::sections[15].data;*size=presentation::sections[15].size;return true;
}

extern "C" void v82_content_reset() { enabled=false; }
