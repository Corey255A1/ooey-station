#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <array>
#include <functional>

namespace ooey_station::vm {

class BooeyVM {
public:
    BooeyVM();
    ~BooeyVM() = default;

    // Load compiled .booey binary
    bool load_program(const std::vector<uint8_t>& binary);
    
    // Execute until OP_VBLANK, OP_HALT, OP_EXIT, or error
    void run_frame();
    
    // Reset VM state
    void reset();

    // Memory access helpers
    uint32_t read_memory_32(uint32_t addr);
    void write_memory_32(uint32_t addr, uint32_t val);
    uint8_t read_memory_8(uint32_t addr);
    void write_memory_8(uint32_t addr, uint8_t val);

    // Callbacks for virtual hardware interface
    void set_vblank_callback(auto&& cb) { vblank_callback_ = cb; }
    void set_draw_pixel_callback(auto&& cb) { draw_pixel_callback_ = cb; }
    void set_draw_line_callback(auto&& cb) { draw_line_callback_ = cb; }
    void set_draw_rect_callback(auto&& cb) { draw_rect_callback_ = cb; }
    void set_draw_text_callback(auto&& cb) { draw_text_callback_ = cb; }
    void set_clear_callback(auto&& cb) { clear_callback_ = cb; }
    void set_draw_sprite_callback(auto&& cb) { draw_sprite_callback_ = cb; }

    // VM State queries
    bool is_halted() const { return halted_; }
    bool is_exited() const { return exited_; }
    uint32_t get_register(int reg) const { return registers_[reg & 0xF]; }
    void set_register(int reg, uint32_t val) { registers_[reg & 0xF] = val; }
    
    uint32_t get_pc() const { return pc_; }
    void set_pc(uint32_t pc) { pc_ = pc; }

    const std::vector<uint8_t>& get_ram() const { return ram_; }
    const std::vector<uint8_t>& get_vram() const { return vram_; }
    const std::vector<uint8_t>& get_aram() const { return aram_; }
    
private:
    // Memory arrays
    std::vector<uint8_t> ram_;   // 64 KB (0x00000 - 0x0FFFF)
    std::vector<uint8_t> vram_;  // 32 KB (0x10000 - 0x17FFF)
    std::vector<uint8_t> aram_;  // 16 KB (0x18000 - 0x1BFFF)
    std::vector<uint8_t> mmio_;  // 4 KB  (0x1C000 - 0x1CFFF)

    // Program space (Code and static data/assets)
    std::vector<uint8_t> code_;
    std::vector<uint8_t> static_data_;
    std::vector<uint8_t> assets_;
    uint32_t entry_point_{0};

    // Registers & CPU state
    std::array<uint32_t, 16> registers_{0};
    uint32_t pc_{0};
    
    // Flags
    bool flag_z_{false}; // Zero
    bool flag_n_{false}; // Negative
    bool flag_c_{false}; // Carry

    // Call stack (separate from RAM to prevent stack overflow exploits in sandboxed env)
    std::vector<uint32_t> call_stack_;

    // Execution state
    bool halted_{false};
    bool exited_{false};
    bool wait_for_vblank_{false};
    std::string error_msg_;

    // Hardware callbacks
    std::function<void()> vblank_callback_;
    std::function<void(int, int, uint32_t)> draw_pixel_callback_;
    std::function<void(int, int, int, int, uint32_t)> draw_line_callback_;
    std::function<void(int, int, int, int, uint32_t, bool)> draw_rect_callback_;
    std::function<void(int, int, uint32_t, uint32_t)> draw_text_callback_; // x, y, str_addr, color
    std::function<void(uint32_t)> clear_callback_;
    std::function<void(uint32_t, int, int, uint32_t)> draw_sprite_callback_; // id, x, y, flags

    void trigger_error(const std::string& msg);
    void execute_instruction();
};

} // namespace ooey_station::vm
