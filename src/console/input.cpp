#include "input.hpp"
#include <iostream>

namespace ooey_station::console {

InputController::InputController() {
    setup_default_bindings();
}

void InputController::setup_default_bindings() {
    // WASD / Upper-case WASD -> D-Pad
    key_bindings_[119] = ButtonId::Up;    // 'w'
    key_bindings_[87]  = ButtonId::Up;    // 'W'
    key_bindings_[115] = ButtonId::Down;  // 's'
    key_bindings_[83]  = ButtonId::Down;  // 'S'
    key_bindings_[97]  = ButtonId::Left;  // 'a'
    key_bindings_[65]  = ButtonId::Left;  // 'A'
    key_bindings_[100] = ButtonId::Right; // 'd'
    key_bindings_[68]  = ButtonId::Right; // 'D'
 
    // Arrow keys -> D-Pad (standard X11 KeySyms, Win32 VKs, Linux Keycodes)
    key_bindings_[65362] = ButtonId::Up;    // X11 Up
    key_bindings_[38]    = ButtonId::Up;    // Win32 Up
    key_bindings_[103]   = ButtonId::Up;    // Linux Up
    
    key_bindings_[65364] = ButtonId::Down;  // X11 Down
    key_bindings_[40]    = ButtonId::Down;  // Win32 Down
    key_bindings_[108]   = ButtonId::Down;  // Linux Down
    
    key_bindings_[65361] = ButtonId::Left;  // X11 Left
    key_bindings_[37]    = ButtonId::Left;  // Win32 Left
    key_bindings_[105]   = ButtonId::Left;  // Linux Left
    
    key_bindings_[65363] = ButtonId::Right; // X11 Right
    key_bindings_[39]    = ButtonId::Right; // Win32 Right
    key_bindings_[106]   = ButtonId::Right; // Linux Right
 
    // Face buttons: JKL / jkl
    key_bindings_[106] = ButtonId::A; // 'j'
    key_bindings_[74]  = ButtonId::A; // 'J'
    key_bindings_[107] = ButtonId::B; // 'k'
    key_bindings_[75]  = ButtonId::B; // 'K'
    key_bindings_[108] = ButtonId::C; // 'l'
    key_bindings_[76]  = ButtonId::C; // 'L'
 
    // Face buttons: UIO / uio
    key_bindings_[117] = ButtonId::X; // 'u'
    key_bindings_[85]  = ButtonId::X; // 'U'
    key_bindings_[105] = ButtonId::Y; // 'i'
    key_bindings_[73]  = ButtonId::Y; // 'I'
    key_bindings_[111] = ButtonId::Z; // 'o'
    key_bindings_[79]  = ButtonId::Z; // 'O'
 
    // Alternate face buttons: ZXC / zxc
    key_bindings_[122] = ButtonId::A; // 'z'
    key_bindings_[90]  = ButtonId::A; // 'Z'
    key_bindings_[120] = ButtonId::B; // 'x'
    key_bindings_[88]  = ButtonId::B; // 'X'
    key_bindings_[99]  = ButtonId::C; // 'c'
    key_bindings_[67]  = ButtonId::C; // 'C'
 
    // System buttons: Enter / Return
    key_bindings_[65293] = ButtonId::Start;  // X11 Return
    key_bindings_[13]    = ButtonId::Start;  // Win32/ASCII Carriage Return
    key_bindings_[10]    = ButtonId::Start;  // Line Feed
    key_bindings_[36]    = ButtonId::Start;  // Wayland Enter (key code)
    key_bindings_[28]    = ButtonId::Start;  // Linux Enter (key code)
    
    // Select: Backspace
    key_bindings_[65288] = ButtonId::Select; // X11 Backspace
    key_bindings_[8]     = ButtonId::Select; // ASCII Backspace / Win32 Backspace
    key_bindings_[14]    = ButtonId::Select; // Linux Backspace (key code)
    
    // Select: Shift keys
    key_bindings_[65505] = ButtonId::Select; // X11 Left Shift
    key_bindings_[65506] = ButtonId::Select; // X11 Right Shift
    key_bindings_[16]    = ButtonId::Select; // Win32 Shift
    key_bindings_[42]    = ButtonId::Select; // Linux Left Shift (key code)
    key_bindings_[54]    = ButtonId::Select; // Linux Right Shift (key code)
}

void InputController::update(ooey::InputManager* input_manager) {
    if (!input_manager) return;

    previous_state_ = current_state_;
    
    // Reset current raw state
    for (int i = 0; i < 12; ++i) {
        current_state_.buttons[i] = false;
    }

    // Map active keys in input_manager to buttons
    for (const auto& [keycode, btn] : key_bindings_) {
        if (input_manager->is_key_pressed(keycode)) {
            current_state_.buttons[static_cast<int>(btn)] = true;
        }
    }

    // Process frame transitions
    for (int i = 0; i < 12; ++i) {
        bool was_pressed = previous_state_.buttons[i];
        bool is_pressed = current_state_.buttons[i];

        if (was_pressed && is_pressed) {
            frame_state_.buttons[i] = ButtonState::Held;
        } else if (!was_pressed && is_pressed) {
            frame_state_.buttons[i] = ButtonState::JustPressed;
        } else if (was_pressed && !is_pressed) {
            frame_state_.buttons[i] = ButtonState::JustReleased;
        } else {
            frame_state_.buttons[i] = ButtonState::Released;
        }
    }
}

bool InputController::is_pressed(ButtonId btn) const {
    return current_state_.buttons[static_cast<int>(btn)];
}

bool InputController::is_held(ButtonId btn) const {
    return frame_state_.buttons[static_cast<int>(btn)] == ButtonState::Held || 
           frame_state_.buttons[static_cast<int>(btn)] == ButtonState::JustPressed;
}

bool InputController::is_just_released(ButtonId btn) const {
    return frame_state_.buttons[static_cast<int>(btn)] == ButtonState::JustReleased;
}

uint32_t InputController::get_held_mask() const {
    uint32_t mask = 0;
    for (int i = 0; i < 12; ++i) {
        if (current_state_.buttons[i]) {
            mask |= (1 << i);
        }
    }
    return mask;
}

uint32_t InputController::get_pressed_mask() const {
    uint32_t mask = 0;
    for (int i = 0; i < 12; ++i) {
        if (frame_state_.buttons[i] == ButtonState::JustPressed) {
            mask |= (1 << i);
        }
    }
    return mask;
}

uint32_t InputController::get_released_mask() const {
    uint32_t mask = 0;
    for (int i = 0; i < 12; ++i) {
        if (frame_state_.buttons[i] == ButtonState::JustReleased) {
            mask |= (1 << i);
        }
    }
    return mask;
}

} // namespace ooey_station::console
