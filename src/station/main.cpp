#include <gooey/application.hpp>
#include "console/display.hpp"
#include "console/input.hpp"
#include "vm/vm.hpp"
#include "station/browser.hpp"
#include "station/bootloader.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <GL/gl.h>

using namespace ooey_station::console;
using namespace ooey_station::vm;
using namespace ooey_station::station;

std::vector<uint8_t> read_binary_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

uint32_t find_sprite_address(uint32_t sprite_id, const std::vector<uint8_t>& vram) {
    uint32_t addr = 256 * 3; // Palette takes 768 bytes
    for (uint32_t i = 0; i < sprite_id; ++i) {
        if (addr + 1 >= vram.size()) return 0;
        uint8_t w = vram[addr];
        uint8_t h = vram[addr + 1];
        addr += 2 + (w * h);
    }
    return addr;
}

int main() {
    gooey::Application app;

    // Scan for games
    std::string games_dir = OOEY_STATION_GAMES_DIR;
    std::cout << "Scanning games in: " << games_dir << std::endl;
    auto games = GameScanner::scan_games(games_dir);

    // Initialize systems
    auto browser = std::make_shared<GameBrowser>();
    browser->set_games(games);

    BooeyVM vm;
    ConsoleDisplay display;
    InputController input_controller;

    bool game_running = false;
    GameInfo active_game;
    
    // Set up VM callbacks to display
    vm.set_clear_callback([&](uint32_t c) { display.clear(c); });
    vm.set_draw_pixel_callback([&](int x, int y, uint32_t c) { display.set_pixel(x, y, c); });
    vm.set_draw_line_callback([&](int x1, int y1, int x2, int y2, uint32_t c) { display.draw_line(x1, y1, x2, y2, c); });
    vm.set_draw_rect_callback([&](int x, int y, int w, int h, uint32_t c, bool fill) { display.draw_rect(x, y, w, h, c, fill); });
    vm.set_draw_sprite_callback([&](uint32_t id, int x, int y, uint32_t flags) {
        uint32_t addr = find_sprite_address(id, vm.get_vram());
        display.draw_sprite(addr, x, y, flags, vm.get_vram());
    });
    vm.set_draw_text_callback([&](int x, int y, uint32_t addr, uint32_t c) {
        std::string str;
        uint32_t curr = addr;
        while (true) {
            char ch = static_cast<char>(vm.read_memory_8(curr++));
            if (ch == '\0' || vm.is_halted()) break;
            str += ch;
        }
        display.draw_text(x, y, str, c);
    });

    browser->set_on_launch_game([&](const GameInfo& game) {
        auto binary = read_binary_file(game.binary_path);
        if (vm.load_program(binary)) {
            active_game = game;
            game_running = true;
            std::cout << "Launched game: " << game.title << std::endl;
        } else {
            std::cerr << "Failed to load game program: " << game.binary_path << std::endl;
        }
    });

    // Setup color palette inside display from loaded program VRAM (index 0 is transparent/ignored, indices 1-255 are loaded)
    auto sync_palette = [&]() {
        const auto& vram = vm.get_vram();
        if (vram.size() >= 256 * 3) {
            for (int i = 1; i < 256; ++i) {
                uint8_t r = vram[i * 3];
                uint8_t g = vram[i * 3 + 1];
                uint8_t b = vram[i * 3 + 2];
                display.set_palette_color(i, r, g, b);
            }
        }
    };

    app.set_root_view(std::move(browser));

    // Global timing
    auto start_time = std::chrono::steady_clock::now();
    uint32_t frame_counter = 0;

    // Tick the emulated console
    app.set_before_render_callback([&](ooey::IRenderTarget*) {
        if (game_running) {
            // Update inputs
            input_controller.update(&app.get_input_manager());
            
            // Map inputs to VM MMIO
            vm.write_memory_32(0x1C000, input_controller.get_held_mask());
            vm.write_memory_32(0x1C004, input_controller.get_pressed_mask());
            vm.write_memory_32(0x1C008, input_controller.get_released_mask());
            
            // Map frame counter and time
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            
            vm.write_memory_32(0x1C010, frame_counter++);
            vm.write_memory_32(0x1C014, static_cast<uint32_t>(ms));
            
            // Sync colors loaded from ROM
            sync_palette();
            
            // Run VM for the frame
            vm.run_frame();
            
            // Check exit
            if (vm.is_exited() || vm.is_halted()) {
                game_running = false;
                std::cout << "Game exited/halted." << std::endl;
            }

            // Quick quit back to browser (Select + Start)
            if (input_controller.is_held(ButtonId::Select) && input_controller.is_held(ButtonId::Start)) {
                game_running = false;
                std::cout << "Force quit game to browser." << std::endl;
            }
        } else {
            // Browser view update & key handling
            browser->handle_input(&app.get_input_manager());
        }
    });

    // Render scaled viewport of emulated display
    app.set_after_render_callback([&](ooey::IRenderTarget* target) {
        if (!game_running) return;

        // OpenGL rendering to scale display
        static GLuint console_tex = 0;
        if (console_tex == 0) {
            glGenTextures(1, &console_tex);
            glBindTexture(GL_TEXTURE_2D, console_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        }

        glBindTexture(GL_TEXTURE_2D, console_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, display.get_framebuffer());

        glEnable(GL_TEXTURE_2D);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        // Get host viewport dimensions
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int W = viewport[2];
        int H = viewport[3];

        // Maintain 4:3 Aspect Ratio
        float scale_x = static_cast<float>(W) / 640.0f;
        float scale_y = static_cast<float>(H) / 480.0f;
        float scale = std::min(scale_x, scale_y);

        float display_w = 640.0f * scale;
        float display_h = 480.0f * scale;

        float offset_x = (W - display_w) / 2.0f;
        float offset_y = (H - display_h) / 2.0f;

        // Draw quad
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(offset_x, offset_y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(offset_x + display_w, offset_y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(offset_x + display_w, offset_y + display_h);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(offset_x, offset_y + display_h);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    });

    app.run();
    return 0;
}
