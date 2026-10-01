#include "v82_vehicle_package.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <cstdlib>

static std::vector<uint8_t> read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("missing owner package: " + path);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
static v82::Bytes bytes(const std::vector<uint8_t>& b) { return {b.data(), uint32_t(b.size())}; }
int main(int argc, char** argv) {
    const char* location = argc == 2 ? argv[1] : std::getenv("V82_ORIGINAL_CARS_PACKAGE");
    if (!location) return 77;
    try {
        const std::string root(location);
        auto registry = read(root + "/VEHICLES.V8R"), archive = read(root + "/CUSTOM.EXP");
        const auto package = v82::read_vehicle_package(bytes(registry), bytes(archive));
        if (package.vehicles.size() != 13 || package.vehicles.front().id != "guest.v8.chassey_blue" ||
            package.vehicles.back().id != "guest.v8.y_the_alien") throw std::runtime_error("original roster identity");
        unsigned transformations = 0, flying = 0;
        for (unsigned i = 0; i < package.vehicles.size(); ++i) {
            const auto& v = package.vehicles[i];
            transformations += v.transforms(); flying += v.flying();
            if (v.stats[13] != 64 + i) throw std::runtime_error("independent runtime type");
            if (!(v.flags & (1u << 23)) || ((v.flags >> 17) & 31) != i)
                throw std::runtime_error("original special capability missing");
        }
        if (transformations != 12 || flying != 1) throw std::runtime_error("original capabilities lost");
        auto rejects = [](v82::Bytes r, v82::Bytes a) {
            try { (void)v82::read_vehicle_package(r, a); }
            catch (const std::exception&) { return true; }
            return false;
        };
        if (!rejects(bytes(registry), {archive.data(), uint32_t(archive.size() - 1)}))
            throw std::runtime_error("truncated archive accepted");
        auto broken = registry;
        const uint32_t table = bytes(broken).u32(12);
        broken[table + 24] = broken[table + 25] = broken[table + 26] = broken[table + 27] = 255;
        if (!rejects(bytes(broken), bytes(archive))) throw std::runtime_error("invalid stats accepted");
        broken = registry;
        broken[table + 44 + 8] = broken[table + 8];
        broken[table + 44 + 9] = broken[table + 9];
        if (!rejects(bytes(broken), bytes(archive))) throw std::runtime_error("shared ownership accepted");
        constexpr unsigned extensionSize=8+13*102;
        if(package.original_effects_bank==65535||registry.size()<extensionSize)
            throw std::runtime_error("original SFX bank missing");
        const auto extension=registry.size()-extensionSize;
        broken=registry;broken[extension+4]=255;
        if(!rejects(bytes(broken),bytes(archive)))throw std::runtime_error("missing SFX bank accepted");
        broken=registry;broken[extension+8+2*102]=broken[extension+8];broken[extension+9+2*102]=broken[extension+9];
        if(!rejects(bytes(broken),bytes(archive)))throw std::runtime_error("aliased effect ownership accepted");
        broken=registry;broken[extension+10+2*102+0xD*2]=254;broken[extension+11+2*102+0xD*2]=255;
        if(!rejects(bytes(broken),bytes(archive)))throw std::runtime_error("nonexistent effect root accepted");
        std::cout << "Loaded 13 original vehicles, " << package.banks.size()
                  << " independent native banks; malformed inputs rejected.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
