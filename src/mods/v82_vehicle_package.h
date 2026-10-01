/* Adapted from NativeVehicleBankSource.cs and V82VehicleRegistry.cs.
 * Copyright (c) 2026 flaffy. MIT; tools/third_party/LICENSE_RECOMPONE. */
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace v82 {
struct Bytes {
    const uint8_t* data = nullptr;
    uint32_t size = 0;
    Bytes sub(uint32_t offset, uint32_t count) const;
    uint16_t u16(uint32_t offset) const;
    uint32_t u32(uint32_t offset) const;
    uint32_t be32(uint32_t offset) const;
};
struct Bank { Bytes bin, animation, sound; };
struct Vehicle {
    std::string id, name;
    uint16_t body, transform, preview, body_kind, preview_kind, selection_order;
    uint16_t rear_damping, original_impact_kind;
    uint32_t flags;
    std::array<uint8_t, 48> stats{}, transform_table{};
    std::array<uint8_t, 20> powerups{};
    bool transforms() const { return !(flags & 1u); }
    bool flying() const { return ((flags >> 8) & 15u) == 1u; }
};
struct VehiclePackage {
    // Slices borrow the verified mod resource snapshot for the launch.
    std::vector<Bank> banks;
    std::vector<Vehicle> vehicles;
    uint16_t original_effects_bank = 65535;
    std::array<uint16_t,13> special_effect_banks{{65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535}};
    std::array<std::array<uint16_t,50>,13> special_effect_kinds{};
};
VehiclePackage read_vehicle_package(Bytes registry, Bytes archive);
}
