#include <gooey/application.hpp>
#include <gooey/mvvmc/controller.hpp>
#include <ooey/platform.hpp>
#include "console/display.hpp"
#include "console/input.hpp"
#include "console/audio.hpp"
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

#include <cmath>

static void draw_gl_circle(float cx, float cy, float r, bool fill, int segments = 32) {
    if (fill) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= segments; ++i) {
            float theta = 2.0f * 3.1415926f * float(i) / float(segments);
            float x = r * std::cos(theta);
            float y = r * std::sin(theta);
            glVertex2f(x + cx, y + cy);
        }
        glEnd();
    } else {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float theta = 2.0f * 3.1415926f * float(i) / float(segments);
            float x = r * std::cos(theta);
            float y = r * std::sin(theta);
            glVertex2f(x + cx, y + cy);
        }
        glEnd();
    }
}

static void draw_gl_rect(float cx, float cy, float w, float h, bool fill) {
    float x1 = cx - w/2.0f;
    float x2 = cx + w/2.0f;
    float y1 = cy - h/2.0f;
    float y2 = cy + h/2.0f;
    if (fill) {
        glBegin(GL_QUADS);
        glVertex2f(x1, y1);
        glVertex2f(x2, y1);
        glVertex2f(x2, y2);
        glVertex2f(x1, y2);
        glEnd();
    } else {
        glBegin(GL_LINE_LOOP);
        glVertex2f(x1, y1);
        glVertex2f(x2, y1);
        glVertex2f(x2, y2);
        glVertex2f(x1, y2);
        glEnd();
    }
}

static void draw_gl_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bool fill) {
    if (fill) {
        glBegin(GL_TRIANGLES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glVertex2f(x3, y3);
        glEnd();
    } else {
        glBegin(GL_LINE_LOOP);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glVertex2f(x3, y3);
        glEnd();
    }
}

struct VirtualButtonRenderInfo {
    ButtonId id;
    float cx, cy;
    float r;
    bool is_rect = false;
    float w = 0, h = 0;
    const char* label = "";
};

static std::vector<VirtualButtonRenderInfo> get_virtual_buttons_render(int W, int H) {
    float base_size = std::min(W, H) / 6.0f;
    if (base_size < 60.0f) base_size = 60.0f;
    if (base_size > 120.0f) base_size = 120.0f;

    std::vector<VirtualButtonRenderInfo> buttons;

    // A, B, C, X, Y, Z buttons on the right
    float btn_r = base_size * 0.25f;
    buttons.push_back({ButtonId::A, static_cast<float>(W) - 180.0f, static_cast<float>(H) - 70.0f, btn_r, false, 0, 0, "A"});
    buttons.push_back({ButtonId::B, static_cast<float>(W) - 120.0f, static_cast<float>(H) - 90.0f, btn_r, false, 0, 0, "B"});
    buttons.push_back({ButtonId::C, static_cast<float>(W) - 60.0f, static_cast<float>(H) - 110.0f, btn_r, false, 0, 0, "C"});
    
    buttons.push_back({ButtonId::X, static_cast<float>(W) - 200.0f, static_cast<float>(H) - 130.0f, btn_r, false, 0, 0, "X"});
    buttons.push_back({ButtonId::Y, static_cast<float>(W) - 140.0f, static_cast<float>(H) - 150.0f, btn_r, false, 0, 0, "Y"});
    buttons.push_back({ButtonId::Z, static_cast<float>(W) - 80.0f, static_cast<float>(H) - 170.0f, btn_r, false, 0, 0, "Z"});

    // Select and Start in the middle
    float sys_w = 60.0f;
    float sys_h = 24.0f;
    buttons.push_back({ButtonId::Select, static_cast<float>(W)/2.0f - 70.0f, static_cast<float>(H) - 30.0f, 0, true, sys_w, sys_h, "SELECT"});
    buttons.push_back({ButtonId::Start, static_cast<float>(W)/2.0f + 70.0f, static_cast<float>(H) - 30.0f, 0, true, sys_w, sys_h, "START"});

    return buttons;
}

int main() {
    gooey::Application app;

    auto backend = ooey::create_default_window_backend();
    if (!backend || !backend->create({800, 600}, "Ooey-Station")) {
        std::cerr << "Failed to create window\n";
        return 1;
    }
    app.set_window_backend(std::move(backend));

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
    AudioEngine audio_engine;

    bool game_running = false;
    GameInfo active_game;
    
    // Set up VM callbacks to display and audio
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
    vm.set_sfx_callback([&](uint32_t id) {
        audio_engine.play_sfx(id);
    });
    vm.set_play_callback([&](uint32_t ch, uint32_t freq, uint32_t dur, uint32_t wave) {
        audio_engine.play_tone(ch, static_cast<float>(freq), dur, wave);
    });

    browser->set_on_launch_game([&](const GameInfo& game) {
        auto binary = read_binary_file(game.binary_path);
        if (vm.load_program(binary)) {
            active_game = game;
            game_running = true;
            std::cout << "Launched game: " << game.title << std::endl;
            
            // Clear focus so browser doesn't intercept keys while game is running
            auto controller = dynamic_cast<gooey::mvvmc::Controller*>(app.get_controller());
            if (controller) {
                controller->set_focused_element(nullptr);
            }
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

    app.set_root_view(browser);
    
    // Set initial focus to the browser so it receives input events
    auto controller = dynamic_cast<gooey::mvvmc::Controller*>(app.get_controller());
    if (controller) {
        controller->set_focused_element(browser);
    }

    // Global timing
    auto start_time = std::chrono::steady_clock::now();
    auto last_frame_time = std::chrono::steady_clock::now();
    uint32_t frame_counter = 0;

    // Tick the emulated console
    app.set_before_render_callback([&](ooey::IRenderTarget*) {
        if (game_running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time).count();
            
            if (elapsed >= 16) { // ~60 FPS (16.67ms)
                last_frame_time = now;

                // Update inputs
                GLint viewport[4];
                glGetIntegerv(GL_VIEWPORT, viewport);
                int W = viewport[2];
                int H = viewport[3];
                if (W <= 0 || H <= 0) { W = 800; H = 600; }
                input_controller.update(&app.get_input_manager(), W, H);
                
                uint32_t held = input_controller.get_held_mask();
                if (held != 0) {
                    std::cout << "Host: Writing held mask " << held << " to VM MMIO 0x1C000" << std::endl;
                }
                vm.write_memory_32(0x1C000, held);
                vm.write_memory_32(0x1C004, input_controller.get_pressed_mask());
                vm.write_memory_32(0x1C008, input_controller.get_released_mask());
                
                // Map frame counter and time
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                
                vm.write_memory_32(0x1C010, frame_counter++);
                vm.write_memory_32(0x1C014, static_cast<uint32_t>(ms));
                
                // Sync colors loaded from ROM
                sync_palette();
                
                // Run VM for the frame
                vm.run_frame();
                
                // Request a redraw from the framework
                app.request_render();
                
                // Check exit
                if (vm.is_exited() || vm.is_halted()) {
                    game_running = false;
                    std::cout << "Game exited/halted." << std::endl;
                    
                    // Restore focus to browser
                    auto controller = dynamic_cast<gooey::mvvmc::Controller*>(app.get_controller());
                    if (controller) {
                        controller->set_focused_element(browser);
                    }
                }

                // Quick quit back to browser (Select + Start)
                if (input_controller.is_held(ButtonId::Select) && input_controller.is_held(ButtonId::Start)) {
                    game_running = false;
                    std::cout << "Force quit game to browser." << std::endl;
                    
                    // Restore focus to browser
                    auto controller = dynamic_cast<gooey::mvvmc::Controller*>(app.get_controller());
                    if (controller) {
                        controller->set_focused_element(browser);
                    }
                }
            }
        } else {
            // Browser view update
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

        // Get host viewport dimensions
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int W = viewport[2];
        int H = viewport[3];

        // Save current OpenGL state and matrices
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);

        glViewport(0, 0, W, H);
        
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, W, H, 0.0, -1.0, 1.0);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, console_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, display.get_framebuffer());

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

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

        // --- Render On-Screen Keypad/Gamepad Overlay ---
        float base_size = std::min(W, H) / 6.0f;
        if (base_size < 60.0f) base_size = 60.0f;
        if (base_size > 120.0f) base_size = 120.0f;

        float dpad_cx = 30.0f + base_size;
        float dpad_cy = H - 30.0f - base_size;
        float dpad_r = base_size;

        // 1. Draw D-pad Background
        glColor4f(0.2f, 0.2f, 0.2f, 0.35f);
        draw_gl_circle(dpad_cx, dpad_cy, dpad_r, true);
        glColor4f(0.8f, 0.8f, 0.8f, 0.4f);
        draw_gl_circle(dpad_cx, dpad_cy, dpad_r, false);

        // 2. Draw D-pad Direction Arrows (Triangles)
        // Up
        if (input_controller.is_held(ButtonId::Up)) glColor4f(1.0f, 0.9f, 0.0f, 0.8f);
        else glColor4f(0.5f, 0.5f, 0.5f, 0.4f);
        draw_gl_triangle(dpad_cx, dpad_cy - dpad_r * 0.8f,
                         dpad_cx - dpad_r * 0.25f, dpad_cy - dpad_r * 0.35f,
                         dpad_cx + dpad_r * 0.25f, dpad_cy - dpad_r * 0.35f, true);

        // Down
        if (input_controller.is_held(ButtonId::Down)) glColor4f(1.0f, 0.9f, 0.0f, 0.8f);
        else glColor4f(0.5f, 0.5f, 0.5f, 0.4f);
        draw_gl_triangle(dpad_cx, dpad_cy + dpad_r * 0.8f,
                         dpad_cx - dpad_r * 0.25f, dpad_cy + dpad_r * 0.35f,
                         dpad_cx + dpad_r * 0.25f, dpad_cy + dpad_r * 0.35f, true);

        // Left
        if (input_controller.is_held(ButtonId::Left)) glColor4f(1.0f, 0.9f, 0.0f, 0.8f);
        else glColor4f(0.5f, 0.5f, 0.5f, 0.4f);
        draw_gl_triangle(dpad_cx - dpad_r * 0.8f, dpad_cy,
                         dpad_cx - dpad_r * 0.35f, dpad_cy - dpad_r * 0.25f,
                         dpad_cx - dpad_r * 0.35f, dpad_cy + dpad_r * 0.25f, true);

        // Right
        if (input_controller.is_held(ButtonId::Right)) glColor4f(1.0f, 0.9f, 0.0f, 0.8f);
        else glColor4f(0.5f, 0.5f, 0.5f, 0.4f);
        draw_gl_triangle(dpad_cx + dpad_r * 0.8f, dpad_cy,
                         dpad_cx + dpad_r * 0.35f, dpad_cy - dpad_r * 0.25f,
                         dpad_cx + dpad_r * 0.35f, dpad_cy + dpad_r * 0.25f, true);

        // 3. Draw face/system buttons
        auto v_btns = get_virtual_buttons_render(W, H);
        for (const auto& btn : v_btns) {
            bool held = input_controller.is_held(btn.id);
            if (held) {
                if (btn.id == ButtonId::Select || btn.id == ButtonId::Start) {
                    glColor4f(0.9f, 0.9f, 0.9f, 0.7f);
                } else if (btn.id == ButtonId::A) {
                    glColor4f(1.0f, 0.3f, 0.3f, 0.7f); // Red
                } else if (btn.id == ButtonId::B) {
                    glColor4f(0.3f, 1.0f, 0.3f, 0.7f); // Green
                } else if (btn.id == ButtonId::C) {
                    glColor4f(0.3f, 0.3f, 1.0f, 0.7f); // Blue
                } else {
                    glColor4f(1.0f, 0.9f, 0.0f, 0.7f); // Yellow for X/Y/Z
                }
            } else {
                glColor4f(0.2f, 0.2f, 0.2f, 0.35f);
            }

            if (btn.is_rect) {
                draw_gl_rect(btn.cx, btn.cy, btn.w, btn.h, true);
                glColor4f(0.8f, 0.8f, 0.8f, 0.4f);
                draw_gl_rect(btn.cx, btn.cy, btn.w, btn.h, false);
            } else {
                draw_gl_circle(btn.cx, btn.cy, btn.r, true);
                glColor4f(0.8f, 0.8f, 0.8f, 0.4f);
                draw_gl_circle(btn.cx, btn.cy, btn.r, false);
            }
        }

        // Restore OpenGL matrices and attributes first so target->draw_text works in UI coords
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();

        // 4. Draw button text labels on top of the circles
        for (const auto& btn : v_btns) {
            bool held = input_controller.is_held(btn.id);
            ooey::Color color = held ? ooey::Color{255, 255, 255, 255} : ooey::Color{255, 255, 255, 150};
            int fontSize = (btn.id == ButtonId::Select || btn.id == ButtonId::Start) ? 10 : 14;
            int ox = (btn.id == ButtonId::Select) ? -20 : (btn.id == ButtonId::Start) ? -18 : -6;
            int oy = -fontSize / 2 - 2;

            target->draw_text(btn.label, ooey::Font{"sans-serif", fontSize, ooey::FontWeight::Bold}, 
                             ooey::Point{static_cast<int>(btn.cx + ox), static_cast<int>(btn.cy + oy)}, color);
        }
    });

    app.run();
    return 0;
}
