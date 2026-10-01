/* Direct format adaptation of the reference's NativeVehicleBankSource.Open,
 * V82VehicleRegistry.ParseRegistry and cross-reference checks. We consume the
 * same V8VR v4/CUSTOM.EXP produced by its original-car builder.
 * Copyright (c) 2026 flaffy. MIT; tools/third_party/LICENSE_RECOMPONE. */
#include "v82_vehicle_package.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>

namespace v82 {
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Bytes Bytes::sub(uint32_t offset, uint32_t count) const {
    require(data && offset <= size && count <= size - offset, "vehicle package range");
    return {data + offset, count};
}
uint16_t Bytes::u16(uint32_t offset) const {
    auto p = sub(offset, 2).data;
    return uint16_t(p[0] | (uint16_t(p[1]) << 8));
}
uint32_t Bytes::u32(uint32_t offset) const {
    auto p = sub(offset, 4).data;
    return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint32_t Bytes::be32(uint32_t offset) const {
    auto p = sub(offset, 4).data;
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
static bool tag(Bytes b, uint32_t off, const char* text) {
    return !std::memcmp(b.sub(off, 4).data, text, 4);
}
static std::string string_at(Bytes data, uint32_t offset, uint32_t floor) {
    require(offset >= floor, "vehicle string overlaps metadata");
    auto bytes = data.sub(offset, data.size - std::min(offset, data.size));
    const auto end = static_cast<const uint8_t*>(std::memchr(bytes.data, 0, bytes.size));
    require(end && end != bytes.data, "vehicle string missing terminator or empty");
    return {reinterpret_cast<const char*>(bytes.data), reinterpret_cast<const char*>(end)};
}
static void validate_sound(Bytes sound) {
    if (!sound.data) return;
    const uint32_t count = sound.u16(0), end = sound.u16(2);
    require(count && end && sound.size == 4u + count * 4u + end * 8u,
            "native SND size mismatch");
    uint32_t previous = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t offset = sound.u16(4u + i * 4u);
        const uint32_t next = i + 1u == count ? end : sound.u16(8u + i * 4u);
        require((i ? offset > previous : offset == 0) && next > offset && next <= end &&
                    ((next - offset) * 8u) % 16u == 0, "native SND sample offsets");
        previous = offset;
    }
}
static std::vector<Bank> read_archive(Bytes archive) {
    std::vector<Bank> banks;
    uint32_t pos = 0;
    while (pos < archive.size) {
        require(tag(archive, pos, "FORM"), "CUSTOM.EXP requires FORM");
        const uint32_t size = archive.be32(pos + 4);
        Bytes form = archive.sub(pos + 8, size);
        require(tag(form, 0, "XOBF"), "CUSTOM.EXP requires XOBF");
        Bank bank;
        uint32_t child = 4;
        while (child < form.size) {
            const uint32_t count = form.be32(child + 4);
            auto payload = form.sub(child + 8, count);
            Bytes* target = nullptr;
            if (tag(form, child, "BIN ")) target = &bank.bin;
            else if (tag(form, child, "ANM ")) target = &bank.animation;
            else if (tag(form, child, "SND ")) target = &bank.sound;
            require(target && !target->data, "unknown or duplicate XOBF child");
            *target = payload;
            child += 8u + count;
            if (count & 1u) { form.sub(child, 1); ++child; }
        }
        require(child == form.size && bank.bin.size >= 28 &&
                    uint64_t(bank.bin.size) + bank.animation.size + bank.sound.size <= 256u * 1024u,
                "invalid native bank size");
        validate_sound(bank.sound);
        banks.push_back(bank);
        require(banks.size() <= 65535, "too many native banks");
        pos += 8u + size;
        if (size & 1u) { archive.sub(pos, 1); ++pos; }
    }
    require(pos == archive.size && !banks.empty(), "empty or truncated native archive");
    return banks;
}
static uint32_t slot_count(Bytes bin) {
    const uint32_t count = bin.u32(24);
    require(count <= (bin.size - 28) / 28, "native slot table truncated");
    return count;
}
static bool top_level(Bytes bin, uint32_t target) {
    const uint32_t count = slot_count(bin);
    require(target < count, "native slot outside table");
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t child = bin.u16(28 + i * 28 + 26), guard = 0;
        while (child != 65535) {
            require(child < count && ++guard <= count, "invalid native child hierarchy");
            if (child == target) return false;
            child = bin.u16(28 + child * 28 + 24);
        }
    }
    return true;
}
static bool instance_marker(Bytes bin, uint32_t root) {
    const uint32_t count = slot_count(bin), groups = bin.u32(0);
    uint32_t child = bin.u16(28 + root * 28 + 26), guard = 0;
    while (child != 65535) {
        require(child < count && ++guard <= count, "invalid transformation hierarchy");
        const auto key = bin.u16(28 + child * 28);
        if ((key & 0xC000) == 0xC000 && (key & 0x7FF) < groups) return true;
        child = bin.u16(28 + child * 28 + 24);
    }
    return false;
}
VehiclePackage read_vehicle_package(Bytes registry, Bytes archive) {
    require(tag(registry, 0, "V8VR") && registry.u16(4) == 4 &&
                registry.u16(6) == 2 && registry.u16(10) == 44,
            "requires V8:2 V8VR version 4");
    const uint32_t count = registry.u16(8), table = registry.u32(12), strings = registry.u32(16);
    require(count && count <= 192 && table >= 20 && strings <= registry.size,
            "invalid native roster table");
    registry.sub(table, count * 44);
    VehiclePackage package;
    package.banks = read_archive(archive);
    std::set<std::string> ids;
    std::set<uint16_t> referenced;
    // Title extension after the v4 strings: one sparse SFX source and explicit
    // per-special effect banks/root maps. Reject unowned or aliased banks.
    constexpr unsigned EffectsSize=8+13*102;
    if (registry.size >= EffectsSize && tag(registry, registry.size - EffectsSize, "V8FX")) {
        const auto ext=registry.size-EffectsSize;
        const auto index = registry.u32(ext+4);
        require(index < package.banks.size() && index < 65535, "invalid original effects bank");
        package.original_effects_bank = uint16_t(index);
        for(unsigned kind=0;kind<13;++kind){
            const auto bank=registry.u16(ext+8+kind*102);package.special_effect_banks[kind]=bank;
            if(bank!=65535){require(bank<package.banks.size()&&referenced.insert(bank).second,"invalid shared effects ownership");}
            for(unsigned i=0;i<50;++i){
                const auto slot=registry.u16(ext+10+kind*102+i*2);package.special_effect_kinds[kind][i]=slot;
                if(slot!=65535)require(bank!=65535&&top_level(package.banks[bank].bin,slot),"invalid original effects mapping");
            }
        }
        require(referenced.count(uint16_t(index))&&package.banks[index].sound.data,"original effects sound bank missing");
    }
    for (uint32_t i = 0; i < count; ++i) {
        auto record = registry.sub(table + i * 44, 44);
        Vehicle v{};
        v.id = string_at(registry, record.u32(0), strings);
        v.name = string_at(registry, record.u32(4), strings);
        require(ids.insert(v.id).second && std::all_of(v.id.begin(), v.id.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        }), "invalid or duplicate stable vehicle id");
        v.body = record.u16(8); v.transform = record.u16(10); v.flags = record.u32(12);
        v.body_kind = record.u16(18); v.selection_order = record.u16(20);
        v.rear_damping = record.u16(36); v.original_impact_kind = record.u16(38);
        v.preview = record.u16(40); v.preview_kind = record.u16(42);
        constexpr uint32_t supported = 1u | 0xF00u | (1u << 16) | (31u << 17) | (1u << 23);
        const uint32_t controller = (v.flags >> 8) & 15, special = (v.flags >> 17) & 31;
        const bool original = (v.flags & (1u << 23)) != 0, sequel = (v.flags & (1u << 16)) != 0;
        require(!original || special == 1 || special == 7 || special == 12 ||
                    package.original_effects_bank != 65535, "original special lacks shared effects");
        require(!(v.flags & ~supported) && controller <= 1 && (!sequel || special < 18) &&
                    (!original || (!sequel && special < 13)) &&
                    record.u16(16) == 48 && !record.u16(22) && v.rear_damping <= 32767 &&
                    (!v.original_impact_kind || original), "unsupported vehicle capability");
        auto stats = registry.sub(record.u32(24), 48);
        require(stats.data[13] == i, "vehicle local type mismatch");
        std::copy_n(stats.data, 48, v.stats.begin());
        v.stats[13] = uint8_t(64 + i);
        const uint32_t transform = record.u32(28);
        require(v.transforms() ? (transform && v.transform != 65535) : !transform,
                "contradictory transformation metadata");
        if (v.transforms()) {
            std::copy_n(registry.sub(transform, 48).data, 48, v.transform_table.begin());
            require(std::all_of(v.transform_table.begin(), v.transform_table.begin() + 12,
                                [](uint8_t b) { return b == 0; }), "normal-mode wheel table is not zero");
        }
        std::copy_n(registry.sub(record.u32(32), 20).data, 20, v.powerups.begin());
        require(v.body < package.banks.size() &&
                    (v.transform == 65535 || v.transform < package.banks.size()) &&
                    (v.preview == 65535 || v.preview < package.banks.size()), "vehicle references missing bank");
        for (auto index : {v.body, v.transform, v.preview})
            if (index != 65535) require(referenced.insert(index).second, "shared native bank ownership");
        require(top_level(package.banks[v.body].bin, v.body_kind), "vehicle body is not top-level");
        if (v.preview != 65535)
            require(top_level(package.banks[v.preview].bin, v.preview_kind), "preview body is not top-level");
        if (v.transform != 65535) {
            const auto bin = package.banks[v.transform].bin;
            for (uint32_t j : {0u, 2u})
                require(top_level(bin, stats.u16(j)), "wheel/contact is not top-level");
            if (v.transforms()) for (uint32_t j = 12; j < 48; j += 2) {
                const uint32_t kind = registry.u16(transform + j);
                require(top_level(bin, kind) && bin.u16(28 + kind * 28 + 2) != 65535 &&
                            bin.u16(28 + kind * 28 + 22) == 0xAAAA && instance_marker(bin, kind),
                        "transformation lacks native state or instance marker");
            }
        }
        package.vehicles.push_back(std::move(v));
    }
    require(referenced.size() == package.banks.size(), "unreferenced native bank");
    return package;
}
}
