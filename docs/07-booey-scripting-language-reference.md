# Ooey-Station: Booey Scripting Language Reference

Booey is a statically-typed, indentation-scoped scripting language designed explicitly for 2D game development on the Ooey-Station. It compiles directly into Booey bytecode (`.booey` binaries).

## 1. Design Goals

1. **LLM-Friendly**: Syntax is predictable, minimal, and Python-like. Built-in functions handle complex tasks (like drawing sprites) in a single line, minimizing the need for complex, bug-prone loops.
2. **Self-Contained**: Booey does not rely on external asset files (no `.png` or `.wav` files). All sprites, tilemaps, and audio data are defined procedurally within the script itself. This allows an entire game to be generated and shared as a single block of text.
3. **Readable**: Strong use of significant indentation.

---

## 2. File Structure & Blocks

A `.booey` file is divided into declarative blocks (for metadata and assets) and procedural blocks (functions).

```booey
# 1. Metadata Block
game:
    title: "Frogger Clone"
    author: "GameDev LLM"

# 2. Asset Definitions
palette:
    green: rgb(0, 200, 0)
    white: rgb(255, 255, 255)

sprites:
    frog 16x16:
        . . G G . .
        . G G G G .
        # ... pixel art definition ...

# 3. Global Variables
vars:
    score: int = 0
    frog_x: int = 304
    frog_y: int = 448

# 4. Functions
fn init():
    spawn_sprite(frog, frog_x, frog_y)

fn update():
    handle_input()
    draw_text(8, 8, "Score: " + str(score), white)
```

---

## 3. Asset Definitions

Assets are defined declaratively. The compiler parses these and embeds them in the Asset Segment of the compiled binary.

### Colors and Palettes
Colors are defined by name and RGB values. The first color defined is implicitly the background/transparent color unless otherwise specified.
```booey
palette:
    black: rgb(0, 0, 0)
    red: rgb(255, 0, 0)
```

### Sprites (Pixel Art Syntax)
Sprites are defined using an intuitive ASCII-art style syntax. 
- Dimensions must be provided (e.g., `16x16`).
- Characters map to the **first letter** of a defined palette color (case sensitive if multiple colors share a starting letter, though it's best to use unique letters).
- `.` or space represents transparency.

```booey
sprites:
    player 8x8:
        . . r r r r . .
        . r r r r r r .
        r r w b r w b r
        r r r r r r r r
        . r r r r r r .
        . . b . . b . .
        . b b . . b b .
        b b b . . b b b
```

---

## 4. Lexical Structure

- **Comments**: Start with `#` and go to the end of the line.
- **Indentation**: Must be exactly 4 spaces per level. Tabs are illegal.
- **Identifiers**: `[a-zA-Z_][a-zA-Z0-9_]*`
- **Keywords**: `fn`, `if`, `elif`, `else`, `while`, `for`, `in`, `return`, `break`, `continue`.

---

## 5. Type System

Booey is statically typed, but types can often be inferred.

- `int`: 32-bit signed integer.
- `fixed`: 16.16 fixed-point number (e.g., `1.5f`). Useful for smooth sub-pixel movement.
- `bool`: `true` or `false`.
- `string`: Immutable text strings.
- `color`: A reference to a palette color.
- `sprite_id`: A reference to a defined sprite.

Variables must be declared in the `vars:` block or locally within functions using `let`.

```booey
fn example():
    let speed: int = 5
    let is_moving: bool = false
```

---

## 6. Control Flow

### If / Elif / Else
```booey
if x > 10:
    x = 10
elif x < 0:
    x = 0
else:
    x += 1
```

### While Loops
```booey
while x < 100:
    x += speed
```

### For Loops
Iteration over ranges.
```booey
# 0 to 9
for i in range(10):
    draw_sprite(enemy, i * 16, 0)

# Start, End (exclusive)
for i in range(5, 10):
    pass
```

---

## 7. Built-in Functions

Booey provides a rich standard library that compiles directly to single VM opcodes or optimized intrinsic routines.

### Graphics & Drawing
- `cls(c: color)`: Clears the screen with the specified color.
- `pixel(x: int, y: int, c: color)`
- `line(x1: int, y1: int, x2: int, y2: int, c: color)`
- `rect(x: int, y: int, w: int, h: int, c: color)` (Outline)
- `fill_rect(x: int, y: int, w: int, h: int, c: color)` (Filled)
- `circle(cx: int, cy: int, r: int, c: color)`
- `fill_circle(cx: int, cy: int, r: int, c: color)`
- `draw_text(x: int, y: int, text: string, c: color)`

### Sprites
- `draw_sprite(id: sprite_id, x: int, y: int)`: Draws a sprite immediately.
- `check_collision(id1: sprite_id, x1: int, y1: int, id2: sprite_id, x2: int, y2: int) -> bool`: Checks bounding box collision.

*(Note: Advanced entity/handle-based sprite management is planned but drawing immediately is the core API).*

### Input
Input is polled per frame.
- `btn_pressed(btn: button) -> bool`: True if the button was just pressed this frame.
- `btn_held(btn: button) -> bool`: True if the button is currently held down.
- `btn_released(btn: button) -> bool`: True if the button was just released.

**Constants**: `UP`, `DOWN`, `LEFT`, `RIGHT`, `A`, `B`, `C`, `X`, `Y`, `Z`, `START`, `SELECT`.

### Math
- `rnd(max: int) -> int`: Returns random integer `0 <= n < max`.
- `abs(val: int) -> int`
- `min(a: int, b: int) -> int`
- `max(a: int, b: int) -> int`
- `clamp(val: int, lo: int, hi: int) -> int`
- `dist(x1: int, y1: int, x2: int, y2: int) -> int`: Returns integer distance.

### System
- `frame() -> int`: Returns the current frame counter.
- `time() -> int`: Returns elapsed time in milliseconds.
- `exit()`: Quits the game and returns to the Ooey-Station browser.

---

## 8. Complete Game Example

Here is a very simple "Avoid the falling block" game.

```booey
game:
    title: "Dodger"
    author: "Documentation"

palette:
    bg: rgb(20, 20, 30)
    player_col: rgb(0, 255, 0)
    enemy_col: rgb(255, 0, 0)
    white: rgb(255, 255, 255)

vars:
    px: int = 320
    py: int = 400
    ex: int = 320
    ey: int = 0
    score: int = 0
    game_over: bool = false

fn init():
    # Called once when game loads
    ex = rnd(600) + 20

fn update():
    # Called 60 times per second
    cls(bg)
    
    if game_over:
        draw_text(280, 240, "GAME OVER!", white)
        if btn_pressed(START):
            # Reset
            score = 0
            game_over = false
            ey = 0
            px = 320
        return
        
    # Input
    if btn_held(LEFT):
        px -= 5
    if btn_held(RIGHT):
        px += 5
        
    px = clamp(px, 0, 620)
    
    # Enemy Logic
    ey += 6 + (score / 5)
    if ey > 480:
        ey = 0
        ex = rnd(600) + 20
        score += 1
        
    # Collision (Simple rect overlap)
    if abs(px - ex) < 20 and abs(py - ey) < 20:
        game_over = true
        
    # Draw
    fill_rect(px, py, 20, 20, player_col)
    fill_rect(ex, ey, 20, 20, enemy_col)
    
    draw_text(10, 10, "Score: " + str(score), white)
```
