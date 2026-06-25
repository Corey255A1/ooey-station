#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace ooey_station::console {

class ConsoleMemory {
public:
    ConsoleMemory();
    ~ConsoleMemory() = default;

    void reset();
    
    // RAM, VRAM, ARAM access helpers
    uint8_t read_byte(uint32_t addr) const;
    void write_byte(uint32_t addr, uint8_t val);
    
    uint32_t read_word(uint32_t addr) const;
    void write_word(uint32_t addr, uint32_t val);

    // ROM helper
    bool load_rom(const std::string& path);
    const std::vector<uint8_t>& get_rom_data() const { return rom_data_; }

private:
    std::vector<uint8_t> ram_;
    std::vector<uint8_t> vram_;
    std::vector<uint8_t> aram_;
    std::vector<uint8_t> rom_data_;
};

} // namespace ooey_station::console
