#include "input.hpp"
#include <iostream>

namespace ooey_station::console {

InputController::InputController() {
    setup_default_bindings();
}

void InputController::setup_default_bindings() {
    // WASD -> D-Pad
    key_bindings_[119] = ButtonId::Up;    // 'w'
    key_bindings_[115] = ButtonId::Down;  // 's'
    key_bindings_[97]  = ButtonId::Left;  // 'a'
    key_bindings_[100] = ButtonId::Right; // 'd'

    // Arrow keys -> D-Pad (standard X11 KeySyms)
    key_bindings_[65362] = ButtonId::Up;
    key_bindings_[65364] = ButtonId::Down;
    key_bindings_[65361] = ButtonId::Left;
    key_bindings_[65363] = ButtonId::Right;

    // Face buttons: JKL
    key_bindings_[106] = ButtonId::A; // 'j'
    key_bindings_[107] = ButtonId::B; // 'k'
    key_bindings_[108] = ButtonId::C; // 'l'

    // Face buttons: UIO
    key_bindings_[117] = ButtonId::X; // 'u'
    key_bindings_[105] = ButtonId::Y; // 'i'
    key_bindings_[111] = ButtonId::Z; // 'o'

    // Alternate face buttons: ZXC
    key_bindings_[122] = ButtonId::A; // 'z'
    key_bindings_[120] = ButtonId::B; // 'x'
    key_bindings_[99]  = ButtonId::C; // 'c'

    // System buttons
    key_bindings_[65293] = ButtonId::Start;  // Enter
    key_bindings_[65288] = ButtonId::Select; // Backspace
    key_bindings_[65505] = ButtonId::Select; // Left Shift
    key_bindings_[65506] = ButtonId::Select; // Right Shift
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
