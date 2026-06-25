#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <array>
#include <ooey/types.hpp>

namespace ooey_station::console {

class ConsoleDisplay {
public:
    static constexpr int WIDTH = 640;
    static constexpr int HEIGHT = 480;
    static constexpr int BPP = 4;
    static constexpr int SIZE = WIDTH * HEIGHT * BPP;

    ConsoleDisplay();
    ~ConsoleDisplay() = default;

    void clear(uint32_t color_idx);
    void set_pixel(int x, int y, uint32_t color_idx);
    uint32_t get_pixel(int x, int y) const;

    void draw_line(int x1, int y1, int x2, int y2, uint32_t color_idx);
    void draw_rect(int x, int y, int w, int h, uint32_t color_idx, bool fill);
    
    // Draw text using built-in 8x8 font
    void draw_text(int x, int y, const std::string& text, uint32_t color_idx);

    // Draw sprite from VRAM (vram contains: [width, height, pixel index data...])
    void draw_sprite(uint32_t sprite_vram_addr, int dx, int dy, uint32_t flags, const std::vector<uint8_t>& vram);

    // Color palette mapping
    void set_palette_color(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
    
    // Get raw RGBA buffer
    const uint8_t* get_framebuffer() const { return framebuffer_.data(); }
    uint8_t* get_framebuffer_mut() { return framebuffer_.data(); }

private:
    std::vector<uint8_t> framebuffer_;
    std::array<ooey::Color, 256> palette_;

    // Built-in 8x8 font bitmap data helper
    bool font_bit_set(char c, int x, int y) const;
};

} // namespace ooey_station::console
