#include "memory.hpp"
#include <fstream>
#include <iostream>

namespace ooey_station::console {

ConsoleMemory::ConsoleMemory() {
    ram_.resize(64 * 1024, 0);
    vram_.resize(32 * 1024, 0);
    aram_.resize(16 * 1024, 0);
}

void ConsoleMemory::reset() {
    std::fill(ram_.begin(), ram_.end(), 0);
    std::fill(vram_.begin(), vram_.end(), 0);
    std::fill(aram_.begin(), aram_.end(), 0);
}

uint8_t ConsoleMemory::read_byte(uint32_t addr) const {
    if (addr < 0x10000) {
        return ram_[addr];
    } else if (addr < 0x18000) {
        return vram_[addr - 0x10000];
    } else if (addr < 0x1C000) {
        return aram_[addr - 0x18000];
    }
    return 0;
}

void ConsoleMemory::write_byte(uint32_t addr, uint8_t val) {
    if (addr < 0x10000) {
        ram_[addr] = val;
    } else if (addr < 0x18000) {
        vram_[addr - 0x10000] = val;
    } else if (addr < 0x1C000) {
        aram_[addr - 0x18000] = val;
    }
}

uint32_t ConsoleMemory::read_word(uint32_t addr) const {
    if (addr + 3 < 0x10000) {
        return *reinterpret_cast<const uint32_t*>(&ram_[addr]);
    }
    return 0;
}

void ConsoleMemory::write_word(uint32_t addr, uint32_t val) {
    if (addr + 3 < 0x10000) {
        *reinterpret_cast<uint32_t*>(&ram_[addr]) = val;
    }
}

bool ConsoleMemory::load_rom(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Memory Error: Could not load ROM file: " << path << std::endl;
        return false;
    }

    rom_data_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    in.close();
    return true;
}

} // namespace ooey_station::console
