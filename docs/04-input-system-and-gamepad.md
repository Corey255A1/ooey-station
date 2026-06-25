# Ooey-Station: Input System & Gamepad

## 1. Game Console Button Layout

The Ooey-Station uses a classic retro-style digital controller layout, inspired by consoles like the Sega Genesis and Sega Saturn. It does not use analog sticks, keeping the input model simple and true to the 16-bit era.

```text
     [L] (Shoulder, reserved)              [R] (Shoulder, reserved)

        D-Pad                                 Face Buttons
        
          [UP]                                   [X]   [Y]   [Z]
           │
  [LEFT] ──┼── [RIGHT]                           [A]   [B]   [C]
           │
         [DOWN]
         
                   [SELECT]   [START]
```

- **D-Pad**: Up, Down, Left, Right (4-way digital directional pad)
- **Face Buttons**: A, B, C (primary row), X, Y, Z (secondary row) — 6 action buttons total
- **System Buttons**: Start, Select
- Total: 12 buttons

## 2. GamepadState Struct

The state of the gamepad is represented by simple boolean flags. We also define a frame-based state to allow the game to detect edge transitions (just pressed, just released).

```cpp
#pragma once

namespace ooey_station {
namespace input {

// Raw state of all buttons at a given moment
struct GamepadState {
    // D-Pad
    bool up{false};
    bool down{false};
    bool left{false};
    bool right{false};
    
    // Face buttons
    bool a{false};
    bool b{false};
    bool c{false};
    bool x{false};
    bool y{false};
    bool z{false};
    
    // System
    bool start{false};
    bool select{false};
};

// Represents the transition state of a single button during a frame
enum class ButtonState {
    Released,
    JustPressed,
    Held,
    JustReleased
};

// State of all buttons processed for the current frame
struct GamepadFrame {
    ButtonState up;
    ButtonState down;
    ButtonState left;
    ButtonState right;
    ButtonState a;
    ButtonState b;
    ButtonState c;
    ButtonState x;
    ButtonState y;
    ButtonState z;
    ButtonState start;
    ButtonState select;
};

// Button IDs used by the VM and InputController
enum class ButtonId {
    Up = 0, Down = 1, Left = 2, Right = 3,
    A = 4, B = 5, C = 6,
    X = 7, Y = 8, Z = 9,
    Start = 10, Select = 11
};

} // namespace input
} // namespace ooey_station
```

## 3. Keyboard Mapping

To play games without a physical gamepad, keyboard keys are mapped to gamepad buttons. The `GamepadMapper` handles this translation.

### Default Mapping Table

The default mapping uses WASD for the D-Pad and JKL/UIO for face buttons. Alternative bindings use Arrow keys for the D-Pad and ZXCASD for face buttons.

| Gamepad Button | Primary Key (QWERTY) | Alternate Key |
|----------------|----------------------|---------------|
| D-Pad Up       | W                    | Arrow Up      |
| D-Pad Down     | S                    | Arrow Down    |
| D-Pad Left     | A                    | Arrow Left    |
| D-Pad Right    | D                    | Arrow Right   |
| A              | J                    | Z             |
| B              | K                    | X             |
| C              | L                    | C             |
| X              | U                    | A (if not D-pad)|
| Y              | I                    | S (if not D-pad)|
| Z              | O                    | D (if not D-pad)|
| Start          | Enter                | —             |
| Select         | Backspace            | Shift         |

*Note: The mapping should be configurable via the Settings menu.*

## 4. InputController Class

The `InputController` sits between the OOEY framework's `InputManager` and the Booey VM.

```cpp
#pragma once

#include "input_types.hpp"
#include <ooey/input.hpp>
#include <map>

namespace ooey_station {
namespace input {

class InputController {
public:
    InputController(ooey::InputManager* input_manager);

    // Called once per frame (e.g., at the start of the 16.67ms tick)
    void update();

    // Query API for the VM and Shell
    bool is_pressed(ButtonId button) const;
    bool is_held(ButtonId button) const;
    bool is_just_pressed(ButtonId button) const;
    bool is_just_released(ButtonId button) const;
    
    // Get the full frame state
    const GamepadFrame& get_frame_state() const { return current_frame_; }

    // Configuration
    void set_key_mapping(ButtonId button, int primary_keycode, int secondary_keycode = -1);

    // Overlay Gamepad integration (injects state from touch/mouse)
    void inject_overlay_state(const GamepadState& state);

private:
    ooey::InputManager* input_manager_;
    
    GamepadState previous_state_;
    GamepadState current_raw_state_;
    GamepadState overlay_state_; // State from touch UI
    
    GamepadFrame current_frame_;
    
    struct KeyBinding {
        int primary;
        int secondary;
    };
    std::map<ButtonId, KeyBinding> bindings_;

    // Helper to evaluate frame transition
    ButtonState calculate_transition(bool was_pressed, bool is_pressed);
};

} // namespace input
} // namespace ooey_station
```

### Update Logic

In the `update()` method:
1. Poll `input_manager_` for currently pressed keys using `is_key_pressed()`.
2. Map these physical keys to logical buttons based on `bindings_`.
3. Combine (OR) the keyboard mapped state with the `overlay_state_` (from the on-screen gamepad).
4. For each button, compare `current_raw_state_` with `previous_state_` to calculate the `GamepadFrame` transitions (JustPressed, Held, JustReleased).
5. Save `current_raw_state_` to `previous_state_`.

## 5. On-Screen Gamepad

For devices with touch screens (like WebAssembly builds on mobile) or simply for mouse interaction, an on-screen virtual gamepad overlay is provided.

It is rendered using OOEY's geometry primitives and is placed on top of the virtual display.

### Layout Geometry
- **Left Side (D-Pad)**: A cross shape made of overlapping rectangles, or 4 directional arrows.
- **Right Side (Action Buttons)**: 
  - `A`, `B`, `C` arranged in a slight upward arc.
  - `X`, `Y`, `Z` arranged in a parallel arc above them.
- **Center Bottom**: Small rectangular `Start` and `Select` buttons.

### Hit Testing
The overlay maintains a list of touch zones (circles for face buttons, rects for D-pad/System).
When an `ooey::PointerEvent` is received:
- If `PointerState::Pressed` or `Moved`, check intersection with touch zones. If intersected, set corresponding flag in `overlay_state_`.
- If `PointerState::Released`, clear flags associated with that pointer ID.

## 6. Input Mapping for the Booey VM

The Booey VM accesses input via specific opcodes or memory-mapped I/O.

### Memory-Mapped I/O Region
The gamepad state is mapped into a read-only memory segment in the VM:
`0x1C000`: Bitmask of currently held buttons.
`0x1C004`: Bitmask of buttons just pressed this frame.
`0x1C008`: Bitmask of buttons just released this frame.

### VM Opcodes
Instead of reading memory directly, scripts usually use input opcodes:
- `BTNP R_dest, button_id` (Button Pressed?)
- `BTNH R_dest, button_id` (Button Held?)
- `BTNR R_dest, button_id` (Button Released?)

*Example Booey Script snippet:*
```booey
if btn_pressed(UP):
    jump()
```

## 7. Multi-Input Considerations

- **Keyboard + Overlay**: Handled seamlessly by logical OR-ing the states in `InputController::update()`.
- **Physical Gamepads**: Future integration will require extending `ooey::InputManager` and `IWindowBackend` to read from platform joystick APIs (e.g., SDL GameController, evdev on Linux, or Gamepad API in WebAssembly) and pushing `GamepadEvent`s into the manager, which the `InputController` can then consume.
