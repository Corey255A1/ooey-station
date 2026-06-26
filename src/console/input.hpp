#pragma once

#include <ooey/input.hpp>
#include <map>

namespace ooey_station::console {

enum class ButtonId {
    Up = 0, Down = 1, Left = 2, Right = 3,
    A = 4, B = 5, C = 6,
    X = 7, Y = 8, Z = 9,
    Start = 10, Select = 11
};

struct GamepadState {
    bool buttons[12] = {false};
};

enum class ButtonState {
    Released,
    JustPressed,
    Held,
    JustReleased
};

struct GamepadFrame {
    ButtonState buttons[12] = {ButtonState::Released};
};

class InputController {
public:
    InputController();
    ~InputController() = default;

    void update(ooey::InputManager* input_manager, int window_w, int window_h);

    bool is_pressed(ButtonId btn) const;
    bool is_held(ButtonId btn) const;
    bool is_just_released(ButtonId btn) const;

    // Get current combined bitmasks
    uint32_t get_held_mask() const;
    uint32_t get_pressed_mask() const;
    uint32_t get_released_mask() const;

private:
    GamepadState previous_state_;
    GamepadState current_state_;
    GamepadFrame frame_state_;

    // Keyboard bindings
    std::map<int, ButtonId> key_bindings_;
    void setup_default_bindings();
};

} // namespace ooey_station::console
