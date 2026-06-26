# 11 — Sonic-Style Platformer Development Guide

> **Complete reference for building a high-speed multi-level platformer on Ooey-Station**
> 
> This guide covers both what you can build *today* with the current VM and language, and
> the engine gaps that must be addressed to achieve a fully-featured Sonic-style game.

---

## Table of Contents

1. [What Is a Sonic-Style Game?](#1-what-is-a-sonic-style-game)
2. [Current Engine Capabilities Audit](#2-current-engine-capabilities-audit)
3. [Gap Analysis: Missing Features](#3-gap-analysis-missing-features)
4. [Memory Layout for a Platformer](#4-memory-layout-for-a-platformer)
5. [Core Physics — What Works Today](#5-core-physics--what-works-today)
6. [Player State Machine](#6-player-state-machine)
7. [Tile-Based Collision System](#7-tile-based-collision-system)
8. [Camera System](#8-camera-system)
9. [Rings & Collectibles](#9-rings--collectibles)
10. [Enemies and Hazards](#10-enemies-and-hazards)
11. [Health System](#11-health-system)
12. [Score and HUD](#12-score-and-hud)
13. [Multiple Levels & Level Transitions](#13-multiple-levels--level-transitions)
14. [Persistent State Across Levels](#14-persistent-state-across-levels)
15. [Audio Design](#15-audio-design)
16. [Full Game Architecture](#16-full-game-architecture)
17. [Required VM/Language Additions](#17-required-vmlanguage-additions)
18. [Implementation Roadmap](#18-implementation-roadmap)

---

## 1. What Is a Sonic-Style Game?

A Sonic-style game is a 2D side-scrolling platformer built around these pillars:

| Feature | Description | Priority |
|---|---|---|
| **Momentum Physics** | Speed builds gradually; full inertia when changing direction | Critical |
| **Sub-pixel Positions** | Smooth movement at fractional pixel positions | Critical |
| **Sensor-Based Collision** | Raycasts from player hitbox edges into terrain | Critical |
| **Slope Physics** | Ground angle affects speed (up = slow, down = accelerate) | High |
| **360° Movement** | Player can walk on walls and ceilings at speed | High |
| **Rings** | Collected rings act as health; dropped on hit | Critical |
| **Multiple Acts/Levels** | Zones with 2–3 acts each; boss at end | Critical |
| **Score System** | Points from rings, enemies, bonus items, time | High |
| **Persistent State** | Rings, lives, and score carry between levels | Critical |
| **Animation FSM** | Idle, run, jump, roll, skid, hurt animations | High |
| **Parallax Backgrounds** | Multiple background layers at different scroll rates | High |
| **Enemies (Badniks)** | Multiple enemy types with AI patterns | Medium |
| **Scrolling Camera** | Camera follows player with deadzone and lead | Critical |

---

## 2. Current Engine Capabilities Audit

### ✅ What Works Today

Based on a full analysis of the VM (`vm.cpp`, `opcodes.hpp`, `vm.hpp`) and the Booey compiler (`codegen.cpp`, `parser.cpp`, `ast.hpp`):

#### Arithmetic & Logic
- ✅ All 32-bit integer arithmetic: ADD, SUB, MUL, DIV, MOD
- ✅ Bitwise operations: AND, OR, XOR, NOT, SHL, SHR
- ✅ Comparison: CMP with Z and N flags
- ✅ Conditional jumps: JZ, JNZ, JL, JG
- ✅ `fixed` point (16.16 format) represented as raw `uint32_t` — **arithmetic works if done manually**

#### Memory
- ✅ 64 KB Game RAM (addresses `0x00000`–`0x0FFFF`)
- ✅ 32 KB VRAM (addresses `0x10000`–`0x17FFF`)
- ✅ 16 KB ARAM (addresses `0x18000`–`0x1BFFF`)
- ✅ MMIO for input registers (`0x1C000` held, `0x1C004` pressed, `0x1C008` released)
- ✅ MMIO for frame counter (`0x1C010`) and elapsed time (`0x1C014`)
- ✅ 8-bit and 32-bit load/store (`LOAD`, `STORE`, `LOADB`, `STOREB`, `LOADR`, `STORER`)

#### Graphics
- ✅ Screen clear (`CLS`)
- ✅ Pixel, line, rect, filled rect drawing
- ✅ `SPR` — Draw sprite at (x, y)
- ✅ `SPREX` — Draw sprite with flip/rotation flags
- ✅ `TILE` — Set a tile on a background layer
- ✅ `TSCROLL` — Set scroll offset for a background layer
- ✅ `TDRAW` — Render a background layer
- ✅ `TEXT` — Draw a null-terminated string from static data
- ✅ VBLANK synchronization

#### Input
- ✅ `BTNH` — Button held state
- ✅ `BTNP` — Button just-pressed state
- ✅ D-Pad, 6 face buttons, Start, Select

#### Sound (Opcodes Exist)
- ✅ `SFX` — Play a sound effect by ID
- ✅ `PLAY` — Play a raw tone on an audio channel

#### Math Utilities
- ✅ `RND` — Random integer
- ✅ `SIN` — Sine function (fixed-point)
- ✅ `DIST` — Distance between two points
- ✅ `FRAME` — Current frame counter
- ✅ `EXIT` — Return to game browser

#### Control Flow
- ✅ `CALL` / `RET` — User-defined functions with return values via R0
- ✅ `JMP`, `JZ`, `JNZ`, `JL`, `JG`
- ✅ `HALT` — Stop execution

#### Language Features
- ✅ `int`, `fixed`, `bool` types
- ✅ `if` / `elif` / `else`
- ✅ `while` loops
- ✅ `for i in range(N)` loops
- ✅ `fn` functions with parameters and return values
- ✅ Global `vars:` block
- ✅ Local `let` variables
- ✅ `palette:`, `sprites:`, `tiles:`, `sounds:` asset blocks

---

## 3. Gap Analysis: Missing Features

This section catalogs every feature gap that prevents a complete Sonic-style game, ranked by severity.

### 🔴 Critical Gaps — Cannot build the core game without these

#### GAP-1: No Array / Buffer Types in Booey Language

**The Problem:**  
A Sonic game needs to track many runtime objects: active rings on screen, dropped ring physics,
enemy positions, hit-stun timers, score tallies per item type. These are inherently array-structured.

The current language has no array syntax. The existing `sonic-style-game-guide.md` used syntax like
`dropped_x: fixed[32]` which does not parse or compile.

**What's needed in the language:**
```booey
# This syntax needs to be added to the parser and codegen
vars:
    ring_x: int[64]       # Array of 64 ints
    ring_y: int[64]
    ring_alive: int[64]   # 0 = dead, 1 = alive
    enemy_x: int[16]
    enemy_y: int[16]
    enemy_type: int[16]
```

**Workaround today:**  
Manually lay out arrays in Game RAM using raw `STORE`/`LOAD` instructions at calculated
memory addresses. The compiler already supports `LOADR`/`STORER` for indirect addressing.

```booey
# Manual array at RAM address 0x1000 — each element is 4 bytes
# ring_x[i] = addr 0x1000 + i*4
# ring_y[i] = addr 0x1100 + i*4
# ring_alive[i] = addr 0x1200 + i*4

fn get_ring_x(i: int) -> int:
    # Manually compute address and LOAD
    # NOT YET SUPPORTED as Booey syntax — requires OP_LOADR bytecode
    return 0 # placeholder
```

**VM Impact:** The VM bytecode supports this via `LOADR`/`STORER`. The gap is purely
in the Booey language and compiler not exposing these instructions via array syntax.

---

#### GAP-2: No `OP_SCOL` (Sprite Collision) Implementation

**The Problem:**  
The opcode `0x52 SCOL` is defined in `opcodes.hpp` and documented, but checking `vm.cpp` reveals
there is **no `case OP_SCOL:` handler** in `execute_instruction()`. The opcode silently falls
through to the `default:` case and triggers a VM error.

This means sprite-vs-sprite collision (needed for: player touching rings, player touching enemies,
player stomping enemies, projectile-enemy collision) cannot use the intended hardware path.

**What's needed:**
```cpp
// In vm.cpp execute_instruction():
case OP_SCOL: {
    uint8_t rd   = code_[pc_++] & 0xF;
    uint8_t rid1 = code_[pc_++] & 0xF;
    uint8_t rx1  = code_[pc_++] & 0xF;
    uint8_t ry1  = code_[pc_++] & 0xF;
    uint8_t rid2 = code_[pc_++] & 0xF;
    uint8_t rx2  = code_[pc_++] & 0xF;
    uint8_t ry2  = code_[pc_++] & 0xF;
    // Load sprite dimensions from VRAM, do AABB check
    registers_[rd] = check_sprite_collision(
        registers_[rid1], registers_[rx1], registers_[ry1],
        registers_[rid2], registers_[rx2], registers_[ry2]
    ) ? 1 : 0;
    break;
}
```

**Workaround today:**  
Manual AABB in Booey using subtraction and comparison (works, but uses more CPU budget per frame).

```booey
fn aabb_overlap(ax: int, ay: int, aw: int, ah: int,
                bx: int, by: int, bw: int, bh: int) -> int:
    if ax + aw <= bx: return 0
    if bx + bw <= ax: return 0
    if ay + ah <= by: return 0
    if by + bh <= ay: return 0
    return 1
```

---

#### GAP-3: No Level/Map Loading — Tilemap Data is Compile-Time Only

**The Problem:**  
The current asset system embeds all tilemap data statically in the binary at compile time. There
is no mechanism for the game to select which level's tiles to render at runtime.

For multiple levels, you need either:
- **Option A**: Multiple sets of tilemaps compiled into the same binary, and a runtime
  "level select" variable that controls which tilemap is used with `TILE` instructions.
- **Option B**: A level data format stored in Game RAM that the `update()` loop reads.

Neither option is documented or formally supported. Option A is feasible today if the game
procedurally issues `TILE` opcodes in `init()` to write level layout to VRAM, but this
consumes significant `init()` time and Booey lacks loops efficient enough for 40×30 tile maps.

**What's needed:**
- Compiler support for a `levels:` block with multiple named tilemaps
- A new opcode or built-in function `load_level(level_id)` that swaps the active tilemap set
- OR: Document and support the pattern of encoding level data as a byte array in Game RAM
  and having a `build_level()` function issue many `TILE` calls

---

#### GAP-4: No Persistent State Between Game Loads

**The Problem:**  
When a game runs `EXIT` or is restarted from the game browser, the VM calls `reset()`, which
wipes all RAM. There is no non-volatile storage (save data, EEPROM equivalent).

A complete Sonic-style game needs to persist:
- Current zone/act number
- Ring count
- Lives remaining  
- Total score
- High score

**What's needed:**

Option A — **MMIO save registers** (simplest):
```
0x1C020 (uint32): Save slot 0 — lives remaining
0x1C024 (uint32): Save slot 1 — current level index
0x1C028 (uint32): Save slot 2 — score (low 32 bits)
0x1C02C (uint32): Save slot 3 — rings collected total
```
The host application persists these to a file when `EXIT` is called or the game browser
takes focus, and restores them when the game is next launched.

Option B — **Level context handoff via MMIO**:  
A dedicated MMIO region that is NOT cleared on `reset()`, allowing a game to write its
inter-level data before triggering a level transition restart. This is the cleanest model.

---

#### GAP-5: No Trigonometric Functions Beyond `SIN` (Missing `COS`, `ATAN2`)

**The Problem:**  
Sonic-style 360° physics requires:
- `cos(angle)` — to decompose velocity into X component from ground angle
- `atan2(dy, dx)` — to compute the angle of the slope beneath the player
- `sin(angle)` — already exists in the VM as `OP_SIN`

The opcode `0x71 SIN` exists in `opcodes.hpp` but the Booey language has no built-in
`sin()` or `cos()` function that the compiler knows how to emit.

**What's needed:**
- Add `cos()` opcode (`0x75 COS`) to the VM
- Add `atan2()` opcode (`0x76 ATAN2`) to the VM  
- Wire `sin()` and `cos()` in the Booey compiler/codegen as built-in functions

---

#### GAP-6: Only 15 Usable Registers — Not Enough for Complex Logic

**The Problem:**  
The VM has 16 registers (R0–R15), but `R15` is documented as the stack pointer and `R0`
is the return value register. The Booey compiler uses registers R1–R14 for expression
evaluation via a simple linear allocator (`next_free_register_`).

Complex expressions in a platformer update loop can easily require more than 14 temporary
registers simultaneously:

```booey
# This expression requires ~8 registers simultaneously:
let hit = aabb_overlap(int(px), int(py), 16, 16,
                       enemy_x[i], enemy_y[i], 16, 16)
```

The compiler will throw: `"Codegen Error: Out of registers for expression evaluation"`
for sufficiently complex nested function calls.

**What's needed:**
- Implement register spilling to Game RAM in the compiler (spill the lowest-priority
  register to a scratch RAM location when all 14 are occupied)
- OR: Increase the register file to 32 registers and update the opcode encoding

---

### 🟡 High Priority Gaps — Significantly degrade the game without these

#### GAP-7: `draw_sprite_ex` Not Exposed in Booey Language

**The Problem:**  
`SPREX` (`0x51`) exists in the VM and handles flip/rotate flags. But the Booey compiler's
`codegen.cpp` has no handler for `draw_sprite_ex` in `FuncCallNode::visit`. Calling it
from Booey code throws "Call to undefined function 'draw_sprite_ex'".

Horizontal sprite flipping is essential for a platformer — the character faces left or right
based on movement direction.

**Fix needed in `codegen.cpp`:**
```cpp
else if (node->name == "draw_sprite_ex") {
    // args: sprite_id, x, y, flags
    node->args[0]->accept(this);  int rid = next_free_register_ - 1;
    node->args[1]->accept(this);  int rx  = next_free_register_ - 1;
    node->args[2]->accept(this);  int ry  = next_free_register_ - 1;
    node->args[3]->accept(this);  int rfl = next_free_register_ - 1;
    emit_8(vm::OP_SPREX);
    emit_8(rid); emit_8(rx); emit_8(ry); emit_8(rfl);
    free_register(rfl); free_register(ry);
    free_register(rx);  free_register(rid);
}
```

---

#### GAP-8: No `abs()`, `min()`, `max()`, `clamp()` Built-in Functions

**The Problem:**  
These are documented in `07-booey-scripting-language-reference.md` as built-in functions,
but `codegen.cpp` has no handlers for `abs`, `min`, `max`, or `clamp`. Calling them fails
with "Call to undefined function".

These are used constantly in physics code:
```booey
vx = clamp(vx, -TOP_SPEED, TOP_SPEED)  # Enforce speed cap
```

**Fix needed in `codegen.cpp`:** Add cases for all four functions using arithmetic opcodes.

---

#### GAP-9: `tscroll` / `tdraw` Not Exposed in Booey Language

**The Problem:**  
The opcodes `TSCROLL` (`0x54`) and `TDRAW` (`0x55`) are implemented in the VM but
`codegen.cpp` has no built-in handlers for `tscroll()` or `tdraw()` Booey function calls.
These are fundamental to any scrolling game — without them you cannot scroll backgrounds.

**Fix needed in `codegen.cpp`:**
```cpp
else if (node->name == "tscroll") {
    // args: layer, dx, dy
    node->args[0]->accept(this); int rl = next_free_register_ - 1;
    node->args[1]->accept(this); int rdx = next_free_register_ - 1;
    node->args[2]->accept(this); int rdy = next_free_register_ - 1;
    emit_8(vm::OP_TSCROLL);
    emit_8(rl); emit_8(rdx); emit_8(rdy);
    free_register(rdy); free_register(rdx); free_register(rl);
}
else if (node->name == "tdraw") {
    // args: layer
    node->args[0]->accept(this); int rl = next_free_register_ - 1;
    emit_8(vm::OP_TDRAW);
    emit_8(rl);
    free_register(rl);
}
```

---

#### GAP-10: No String Formatting (Score/Ring Display)

**The Problem:**  
`draw_text()` in the VM takes a `addr` pointing to a static null-terminated string in Game RAM.
Displaying a dynamic integer value like `score = 12500` requires formatting an integer into
a string buffer at runtime.

There is no `str(int)` or `itoa()` built-in function. There is no string concatenation operator.
The HUD `draw_text(10, 10, "Score: " + str(score), white)` code shown in the existing guide
does not compile.

**What's needed:**
- New opcode: `ITOA Rd_addr, Rs_int` — converts the integer in Rs to ASCII digits written
  to the Game RAM address in Rd
- New built-in: `draw_int(x, y, value, color)` — draws an integer directly
- Both are simpler than full string formatting

---

#### GAP-11: No `tile()` Built-in Function Exposed in Booey

**The Problem:**  
`TILE` (`0x53`) sets a tile at coordinates on a background layer. This is the mechanism for
building the level map at runtime. Like `tscroll`/`tdraw`, it is not wired in `codegen.cpp`.

Without `tile()`, levels cannot be procedurally constructed.

---

#### GAP-12: No Elapsed Time / Delta-Time for Physics

**The Problem:**  
MMIO register `0x1C014` is documented as elapsed time in milliseconds. The opcode `FRAME` (`0x73`)
returns the frame counter. But neither is exposed as a Booey built-in function (`time()` or
`frame()` do not have codegen handlers).

Consistent physics requires either:
- Assuming a fixed 60 FPS (frame counter sufficient)
- Or true delta-time measurement (elapsed time MMIO needed)

**Fix:** Wire `frame()` as a built-in in codegen using `OP_FRAME`.

---

### 🟠 Medium Priority Gaps — Important for polish and completeness

#### GAP-13: No Animation Frame Support

**The Problem:**  
Animations require cycling through multiple sprite frames. Sonic has: idle (1 frame), run
(8 frames at speed-dependent rate), jump (ball, rotates), skid (1 frame), hurt (1 frame).

The current sprite system stores individual sprites, indexed by ID. To animate, you need to:
1. Know the current frame index within an animation
2. Map that to the correct sprite ID
3. Advance the frame at the right time

This is entirely doable with current int variables + modulo logic, but the Booey compiler's
`mod` operator (`OP_MOD`) is not tested with the `%` operator from Booey syntax.

---

#### GAP-14: No Background Scrolling Parallax Configuration

**The Problem:**  
A Sonic-style game needs 4 background layers:
- Layer 0: Far background (sky, hills) — scroll at 25% of camera speed
- Layer 1: Mid background (distant trees) — scroll at 50%  
- Layer 2: Foreground tiles (playfield terrain) — scroll at 100%
- Layer 3: Foreground detail (grass, water surface) — scroll at 100%

The `TSCROLL` opcode exists. What's missing is documentation of how to define and load
the initial tile content for each layer, and the parallax rate convention.

---

#### GAP-15: No `play_sound()` / `sfx()` Built-in in Codegen

**The Problem:**  
`OP_SFX` is implemented in the VM but not exposed via a Booey built-in function in codegen.
Sound effects (jump, ring collect, hurt, level clear) are critical for game feel.

---

#### GAP-16: No `BTNR` (Button Released) Opcode / Built-in

**The Problem:**  
The MMIO register `0x1C008` holds the "just released" button mask. The VM documentation
mentions `btn_released()` as a built-in, but there is no corresponding opcode (`OP_BTNR`)
in `opcodes.hpp` and no handler in `vm.cpp`. Variable jump height (tap = small jump, hold = big
jump) requires detecting when the jump button is released mid-air.

---

## 4. Memory Layout for a Platformer

Given the 64 KB Game RAM, here is a recommended memory map for a Sonic-style game:

```
Game RAM (0x00000 - 0x0FFFF)
┌────────────────────────────────────────────────────────┐
│ 0x0000 - 0x0003 │  4 B │ px (player x, fixed 16.16)   │
│ 0x0004 - 0x0007 │  4 B │ py (player y, fixed 16.16)   │
│ 0x0008 - 0x000B │  4 B │ vx (velocity x, fixed)       │
│ 0x000C - 0x000F │  4 B │ vy (velocity y, fixed)       │
│ 0x0010          │  1 B │ player_state                  │
│ 0x0011          │  1 B │ grounded (bool)               │
│ 0x0012          │  1 B │ facing_right (bool)           │
│ 0x0013          │  1 B │ anim_frame                    │
│ 0x0014          │  1 B │ anim_timer                    │
│ 0x0015 - 0x001F │ 11 B │ (reserved player fields)     │
│ 0x0020 - 0x0023 │  4 B │ cam_x (camera x)             │
│ 0x0024 - 0x0027 │  4 B │ cam_y (camera y)             │
│ 0x0028 - 0x002B │  4 B │ cam_target_x                 │
│ 0x002C - 0x002F │  4 B │ cam_target_y                 │
│ 0x0030 - 0x0033 │  4 B │ score (uint32)               │
│ 0x0034 - 0x0035 │  2 B │ rings (uint16)               │
│ 0x0036          │  1 B │ lives (uint8)                 │
│ 0x0037          │  1 B │ current_level (uint8)        │
│ 0x0038 - 0x003B │  4 B │ level_timer (frame count)    │
│ 0x003C - 0x003F │  4 B │ (reserved HUD fields)        │
│ 0x0040 - 0x013F │ 256 B│ Rings array [64 × 4 bytes]  │
│ 0x0140 - 0x023F │ 256 B│ Ring_x[i] (fixed)           │
│ 0x0240 - 0x033F │ 256 B│ Ring_y[i] (fixed)           │
│ 0x0340 - 0x037F │  64 B│ Ring_alive[i] (byte)        │
│ 0x0380 - 0x03FF │ 128 B│ Dropped ring physics [32]   │
│ 0x0400 - 0x04FF │ 256 B│ Enemy data [16 × 16 bytes]  │
│ 0x0500 - 0x05FF │ 256 B│ Particle/VFX data           │
│ 0x0600 - 0x0FFF │ 2.5K │ General scratch             │
│ 0x1000 - 0x3FFF │ 12KB │ Level tile map data         │
│ 0x4000 - 0x7FFF │ 16KB │ Level 2 tile map data       │
│ 0x8000 - 0xBFFF │ 16KB │ Level 3 tile map data       │
│ 0xC000 - 0xCFFF │  4KB │ String buffers (HUD text)   │
│ 0xD000 - 0xFFFF │ 12KB │ Stack / miscellaneous       │
└────────────────────────────────────────────────────────┘
```

> [!NOTE]  
> The 12 KB available for MMIO save slots (once GAP-4 is resolved) would live at
> `0x1C020` onward in the MMIO region. Accessing this today causes a VM fault.

---

## 5. Core Physics — What Works Today

The following physics implementation is **buildable right now** using current Booey features.
All arithmetic uses fixed-point 16.16 representation stored in global `int` variables
(the `fixed` type keyword exists in the language but `int` variables work equivalently
since the VM treats all registers as 32-bit values).

### Physics Constants

```booey
vars:
    # Physics (all stored as fixed 16.16 — multiply by 65536 for true value)
    # Example: 0.046875 * 65536 = 3072
    ACCEL:      int = 3072       # 0.046875 px/frame²
    DECEL:      int = 32768      # 0.5 px/frame² (skid deceleration)
    FRICTION:   int = 3072       # 0.046875 px/frame²
    TOP_SPEED:  int = 393216     # 6.0 px/frame (6 * 65536)
    GRAVITY:    int = 14336      # 0.21875 px/frame²
    JUMP_FORCE: int = -425984    # -6.5 px/frame (negative = up)
    AIR_CAP:    int = 24576      # 0.375 px/frame² (air deceleration)
    
    # Player state
    px: int = 0       # Position X (fixed 16.16)
    py: int = 0       # Position Y (fixed 16.16)
    vx: int = 0       # Velocity X (fixed 16.16)
    vy: int = 0       # Velocity Y (fixed 16.16)
    
    # Ground state
    grounded:     int = 0
    facing_right: int = 1
    player_state: int = 0   # 0=normal, 1=jumping, 2=rolling, 3=hurt
    
    # Camera
    cam_x: int = 0
    cam_y: int = 0
    
    # HUD
    rings: int = 0
    score: int = 0
    lives: int = 3
    current_level: int = 0
```

### Movement Update Function

```booey
fn update_movement():
    if player_state == 0:   # STATE_NORMAL
        if btn_held(LEFT):
            facing_right = 0
            if vx > 0:
                vx = vx - DECEL    # Skidding right → left
            else:
                vx = vx - ACCEL    # Normal acceleration left
        elif btn_held(RIGHT):
            facing_right = 1
            if vx < 0:
                vx = vx + DECEL    # Skidding left → right
            else:
                vx = vx + ACCEL    # Normal acceleration right
        else:
            # Friction
            if vx > 0:
                vx = vx - FRICTION
                if vx < 0:
                    vx = 0
            elif vx < 0:
                vx = vx + FRICTION
                if vx > 0:
                    vx = 0
        
        # Clamp to top speed (manual clamp — see GAP-8)
        if vx > TOP_SPEED:
            vx = TOP_SPEED
        if vx < -TOP_SPEED:
            vx = -TOP_SPEED
    
    elif player_state == 2:  # STATE_ROLLING
        # Reduced friction while rolling
        if vx > 0:
            vx = vx - 1536     # 0.0234375 = roll friction
        elif vx < 0:
            vx = vx + 1536

fn update_gravity_and_jump():
    if grounded == 0:
        vy = vy + GRAVITY
        # Terminal velocity cap
        if vy > 720896:   # ~11 px/frame
            vy = 720896
    else:
        if btn_pressed(A):
            vy = JUMP_FORCE
            grounded = 0
            player_state = 1   # STATE_JUMPING
        if btn_held(DOWN):
            if vx > 65536 or vx < -65536:  # abs(vx) > 1.0
                player_state = 2   # Enter rolling
```

### Position Integration

```booey
fn integrate_position():
    # Apply velocity to position (both in fixed 16.16)
    px = px + vx
    py = py + vy
```

> [!IMPORTANT]  
> The key insight is that `fixed` type variables stored as `int` work perfectly with integer
> arithmetic since all physics constants are pre-multiplied by 65536. When passing
> position to drawing functions (`draw_sprite`, etc.), shift right by 16 to get the
> integer pixel position: the Booey language currently has no bitshift syntax in expressions,
> so store separate integer pixel positions computed from the fixed values.

---

## 6. Player State Machine

```
                    ┌──────────┐
                    │  NORMAL  │◄──────────────────────────────────────┐
                    └──────────┘                                        │
                    │         │                                         │
               Hold DOWN   Press A                                      │
               + speed>1   (jump)                                       │
                    │         │                                         │
                    ▼         ▼                                         │
              ┌─────────┐ ┌─────────┐   Hit ceiling     ┌──────────┐   │
              │ ROLLING │ │ JUMPING │──────────────────► │  NORMAL  │   │
              └─────────┘ └─────────┘                   (grounded) │   │
                    │         │                                         │
                    │    Land on ground                                 │
                    └─────────┴───────────────────────────────────────►┘
                              │
                          Take damage
                              │
                              ▼
                         ┌─────────┐
                         │  HURT   │──── Timer expires ────► NORMAL
                         └─────────┘
                              │
                         rings == 0
                              │
                              ▼
                         ┌─────────┐
                         │  DEAD   │──── Lose life ────► Respawn / Game Over
                         └─────────┘
```

### State Constants and Implementation

```booey
vars:
    STATE_NORMAL:  int = 0
    STATE_JUMPING: int = 1
    STATE_ROLLING: int = 2
    STATE_HURT:    int = 3
    STATE_DEAD:    int = 4
    
    hurt_timer:    int = 0
    invincible:    int = 0   # Invincibility frames after being hurt
    inv_timer:     int = 0

fn apply_hurt():
    if invincible == 0:
        if rings > 0:
            # Scatter rings
            scatter_rings()
            rings = 0
            player_state = STATE_HURT
            hurt_timer = 120         # 2 seconds at 60 FPS
            inv_timer = 180          # 3 seconds invincibility
            invincible = 1
            vy = -196608             # -3.0 bounce up
            vx = 131072              # +2.0 knock back
        else:
            # No rings — die
            player_state = STATE_DEAD
            lives = lives - 1

fn update_hurt_timer():
    if player_state == STATE_HURT:
        hurt_timer = hurt_timer - 1
        if hurt_timer <= 0:
            player_state = STATE_NORMAL
    
    if invincible == 1:
        inv_timer = inv_timer - 1
        if inv_timer <= 0:
            invincible = 0
```

---

## 7. Tile-Based Collision System

A Sonic game uses a sensor system rather than simple rectangle collision. Here is a
practical sensor implementation for flat/near-flat terrain using the current VM.

### Ground Sensors

The player has two ground sensors (A = left foot, B = right foot), each extending
downward from the player's bottom edge.

```booey
vars:
    PLAYER_W:    int = 12    # Player hitbox width
    PLAYER_H:    int = 28    # Player hitbox height
    SENSOR_HALF: int = 6     # Half-width for ground sensors
    TILE_SIZE:   int = 16    # Tile size in pixels

fn tile_is_solid(tile_id: int) -> int:
    # Returns 1 if tile_id represents a solid tile, 0 if empty/passthrough
    if tile_id == 0:
        return 0   # Air
    if tile_id == 1:
        return 1   # Solid block
    if tile_id == 2:
        return 1   # Platform (solid from top only — handled in sensor)
    return 0

fn get_tile_at_world(world_x: int, world_y: int) -> int:
    # Convert world pixel to tile coordinate
    # world_x, world_y are INTEGER pixel positions (not fixed)
    let tx: int = world_x / TILE_SIZE
    let ty: int = world_y / TILE_SIZE
    
    # Bounds check
    if tx < 0: return 0
    if ty < 0: return 0
    if tx >= 80: return 0    # Map width in tiles (80 * 16 = 1280 pixels)
    if ty >= 30: return 1    # Floor at bottom (30 * 16 = 480 pixels)
    
    # Read from Game RAM tile map at 0x1000
    # Each row is 80 tiles * 1 byte = 80 bytes
    # tile_map[ty][tx] = RAM[0x1000 + ty*80 + tx]
    # NOTE: This requires LOADR bytecode + manual address calculation
    # Currently not directly expressible in Booey — see GAP-1/GAP-2
    return 0

fn check_ground():
    # Convert fixed-point position to integer pixels
    let ix: int = px / 65536
    let iy: int = py / 65536
    
    # Sensor A (left foot), Sensor B (right foot)
    let sa_x: int = ix - SENSOR_HALF
    let sb_x: int = ix + SENSOR_HALF
    let feet_y: int = iy + PLAYER_H
    
    let tile_a: int = get_tile_at_world(sa_x, feet_y)
    let tile_b: int = get_tile_at_world(sb_x, feet_y)
    
    if tile_is_solid(tile_a) == 1 or tile_is_solid(tile_b) == 1:
        # Snap to tile top
        let snap_y: int = (feet_y / TILE_SIZE) * TILE_SIZE
        py = (snap_y - PLAYER_H) * 65536   # Convert back to fixed
        vy = 0
        grounded = 1
        if player_state == STATE_JUMPING:
            player_state = STATE_NORMAL
    else:
        grounded = 0

fn check_ceiling():
    let ix: int = px / 65536
    let iy: int = py / 65536
    let head_y: int = iy
    
    let tile_l: int = get_tile_at_world(ix - 4, head_y - 1)
    let tile_r: int = get_tile_at_world(ix + 4, head_y - 1)
    
    if tile_is_solid(tile_l) == 1 or tile_is_solid(tile_r) == 1:
        # Hit ceiling — cancel upward velocity
        let ceil_y: int = ((head_y / TILE_SIZE) + 1) * TILE_SIZE
        py = ceil_y * 65536
        vy = 0

fn check_walls():
    let ix: int = px / 65536
    let iy: int = py / 65536
    let mid_y: int = iy + PLAYER_H / 2
    
    # Left wall
    if tile_is_solid(get_tile_at_world(ix - PLAYER_W / 2 - 1, mid_y)) == 1:
        let wall_x: int = ((ix - PLAYER_W / 2) / TILE_SIZE + 1) * TILE_SIZE
        px = (wall_x + PLAYER_W / 2) * 65536
        if vx < 0: vx = 0
    
    # Right wall
    if tile_is_solid(get_tile_at_world(ix + PLAYER_W / 2, mid_y)) == 1:
        let wall_x: int = ((ix + PLAYER_W / 2) / TILE_SIZE) * TILE_SIZE
        px = (wall_x - PLAYER_W / 2) * 65536
        if vx > 0: vx = 0
```

> [!WARNING]  
> `get_tile_at_world()` currently cannot be fully implemented in Booey because reading
> a dynamic array address requires `LOADR`/`STORER` and array syntax (GAP-1). Until
> arrays are supported, use a simplified linear collision check against a hard-coded
> ground Y position for prototyping.

---

## 8. Camera System

A professional Sonic-style camera has a horizontal deadzone and vertical lead:

```booey
vars:
    cam_x:     int = 0    # Current camera position
    cam_y:     int = 0
    DEAD_X:    int = 32   # Horizontal deadzone half-width
    CAM_SPEED: int = 8    # Max camera catch-up pixels per frame
    LEVEL_W:   int = 1280 # Level width in pixels
    LEVEL_H:   int = 480  # Level height in pixels

fn update_camera():
    let px_int: int = px / 65536
    let py_int: int = py / 65536
    
    let target_x: int = px_int - 320   # Center on screen
    let target_y: int = py_int - 240
    
    # Horizontal deadzone: only move camera if player is outside deadzone
    let dx: int = target_x - cam_x
    if dx > DEAD_X:
        let move: int = dx - DEAD_X
        if move > CAM_SPEED: move = CAM_SPEED
        cam_x = cam_x + move
    elif dx < -DEAD_X:
        let move: int = -dx - DEAD_X
        if move > CAM_SPEED: move = CAM_SPEED
        cam_x = cam_x - move
    
    # Vertical: simple lerp (smooth follow)
    let dy: int = target_y - cam_y
    if dy > 4: cam_y = cam_y + 4
    elif dy < -4: cam_y = cam_y - 4
    else: cam_y = target_y
    
    # Clamp camera to level bounds
    if cam_x < 0: cam_x = 0
    if cam_x > LEVEL_W - 640: cam_x = LEVEL_W - 640
    if cam_y < 0: cam_y = 0
    if cam_y > LEVEL_H - 480: cam_y = LEVEL_H - 480
    
    # Apply to background layers
    # tscroll(0, cam_x / 4, cam_y / 4)   # Far BG: 25% parallax
    # tscroll(1, cam_x / 2, cam_y / 2)   # Mid BG: 50% parallax
    # tscroll(2, cam_x, cam_y)            # Foreground: 100%
    # NOTE: tscroll() requires GAP-9 to be fixed before this works
```

---

## 9. Rings & Collectibles

Rings are the core mechanic. They serve as the health system AND reward system.

### Static Ring Layout (at Level Load)

```booey
# Ring positions for Level 1 — stored as pairs in Game RAM
# After GAP-1 is resolved, this becomes: ring_x: int[64], ring_y: int[64]
# For now, encode ring count + positions as separate variables:

vars:
    NUM_RINGS: int = 0        # Total rings in current level
    rings:     int = 0        # Player's carried ring count

# Ring data lives at these RAM addresses (raw memory approach):
# Ring X positions: 0x0140 + i*4
# Ring Y positions: 0x0240 + i*4
# Ring alive:       0x0340 + i   (byte per ring)

fn init_rings():
    # Hard-code ring positions for level 1
    # With STORER opcode we could write these dynamically
    NUM_RINGS = 10
    # ...ring layout defined per level

fn collect_ring(i: int):
    # Mark ring i as dead, increment ring count
    rings = rings + 1
    score = score + 10

fn check_ring_collisions():
    let px_int: int = px / 65536
    let py_int: int = py / 65536
    let i: int = 0
    while i < NUM_RINGS:
        # Read ring_alive[i] from RAM
        # read_ring_alive(i) -- requires array support
        # Simplified: check distance to ring position
        i = i + 1
```

### Ring Scatter Physics

When the player is hit, rings explode outward in a circular pattern:

```booey
vars:
    NUM_DROPPED:  int = 0
    DROP_TIMER:   int = 0

# Dropped ring data in RAM: addr 0x0380
# Each dropped ring: dx(4), dy(4), dvx(4), dvy(4) = 16 bytes × 32 rings = 512 bytes

fn scatter_rings():
    let count: int = rings
    if count > 32: count = 32
    rings = 0
    NUM_DROPPED = count
    DROP_TIMER = 256    # Rings vanish after ~4 seconds
    
    let i: int = 0
    while i < count:
        # Circular spread: angle = i * (2π / count)
        # Use OP_SIN for y-component, COS for x-component
        # This requires array writes (GAP-1) and cos() (GAP-5)
        i = i + 1

fn update_dropped_rings():
    if DROP_TIMER > 0:
        DROP_TIMER = DROP_TIMER - 1
        # Update physics for each dropped ring (gravity, wall bounce)
        # Re-collect if player touches them (after a 2-second grace)
```

---

## 10. Enemies and Hazards

### Enemy Data Structure

With arrays (post GAP-1), each enemy needs:
- Position (x, y) — fixed point
- Type (determines AI behavior and sprite)
- State (alive, stunned, dead)
- Direction (facing left or right)
- Timer (for AI pattern timing)

### Basic Enemy Type: "Motobug" (Rolling Ball)

```booey
vars:
    ENEMY_ALIVE:   int = 1
    ENEMY_DEAD:    int = 0
    ENEMY_STUNNED: int = 2
    
    NUM_ENEMIES:   int = 0
    
    # Enemy arrays — requires GAP-1:
    # enemy_x[16], enemy_y[16], enemy_vx[16]
    # enemy_type[16], enemy_state[16], enemy_dir[16]

fn update_enemy(i: int):
    # Simple patrol AI: move in direction, reverse on wall
    # Requires array access (GAP-1)
    pass

fn check_enemy_player_collision(i: int):
    # Player stomps enemy from above = defeat enemy
    # Player touches enemy from side = player hurt
    let px_int: int = px / 65536
    let py_int: int = py / 65536
    let vy_int: int = vy / 65536
    
    # Get enemy position (requires array, so placeholder):
    let ex: int = 100
    let ey: int = 200
    let ew: int = 16
    let eh: int = 16
    
    # AABB check (using manual workaround for GAP-2)
    let overlap: int = aabb_overlap(px_int - 6, py_int, 12, 28, ex, ey, ew, eh)
    
    if overlap == 1:
        # Is player falling onto the enemy?
        if vy > 0 and py_int + 28 <= ey + 4:
            # Stomp! Defeat enemy
            score = score + 100
            vy = -262144    # -4.0 bounce
        else:
            # Side collision — player hurt
            apply_hurt()
```

---

## 11. Health System

The ring-as-health system:

```
┌─────────────────────────────────────────────────────────────────┐
│                     HEALTH STATE MACHINE                         │
│                                                                  │
│  rings > 0: Hit → scatter rings, become invincible (3 sec)      │
│  rings = 0: Hit → die, lose 1 life                              │
│  lives = 0: Die → Game Over screen                              │
│  lives > 0: Die → Respawn at last checkpoint (if any)           │
│                                                                  │
│  Ring Cap: 999 (max rings displayed)                            │
│  Lives Cap: 99 (max extra lives)                                │
└─────────────────────────────────────────────────────────────────┘
```

```booey
vars:
    RING_CAP: int = 999
    LIFE_CAP: int = 99
    
    checkpoint_x: int = 0    # Last checkpoint pixel X
    checkpoint_y: int = 0    # Last checkpoint pixel Y
    checkpoint_set: int = 0  # 1 if checkpoint activated

fn add_rings(count: int):
    rings = rings + count
    score = score + count * 10
    if rings > RING_CAP:
        rings = RING_CAP

fn add_life():
    lives = lives + 1
    if lives > LIFE_CAP:
        lives = LIFE_CAP

fn respawn_player():
    if lives > 0:
        lives = lives - 1
        rings = 0
        if checkpoint_set == 1:
            px = checkpoint_x * 65536
            py = checkpoint_y * 65536
        else:
            px = 0   # Level start
            py = 0
        vx = 0
        vy = 0
        grounded = 0
        player_state = STATE_NORMAL
        inv_timer = 180   # 3 seconds invincibility
        invincible = 1
    else:
        # Game Over
        player_state = STATE_DEAD   # triggers game over screen in update()

fn activate_checkpoint(x: int, y: int):
    checkpoint_x = x
    checkpoint_y = y
    checkpoint_set = 1
```

---

## 12. Score and HUD

The HUD must display: **Rings**, **Score**, **Time**, **Lives**.

### Score Calculation

| Action | Points |
|---|---|
| Collect ring | 10 pts |
| Defeat enemy (stomp) | 100 pts |
| Defeat enemy (rolling) | 200 pts |
| Extra life power-up | 1000 pts |
| Level clear (base) | 1000 pts |
| Time bonus (<30s) | 5000 pts |
| Time bonus (<60s) | 4000 pts |
| Time bonus (<90s) | 3000 pts |
| Time bonus (>90s) | 0 pts |

### HUD Drawing

```booey
vars:
    level_timer:   int = 0    # Counts up in frames
    TIMER_LIMIT:   int = 5400 # 90 seconds × 60 FPS = 9:00

fn update_hud():
    level_timer = level_timer + 1
    
    # Draw HUD background band
    fill_rect(0, 0, 640, 24, black)
    
    # Lives (top-left)
    draw_text(8, 6, "LIVES", white)
    # draw_int(60, 6, lives, white)   -- requires GAP-10
    
    # Rings (top-center-left)  
    draw_text(140, 6, "RINGS", yellow)
    # draw_int(196, 6, rings, yellow)  -- requires GAP-10
    
    # Score (top-center)
    draw_text(280, 6, "SCORE", white)
    # draw_int(336, 6, score, white)   -- requires GAP-10
    
    # Timer (top-right)
    draw_text(500, 6, "TIME", white)
    let seconds: int = level_timer / 60
    # draw_int(548, 6, seconds, white) -- requires GAP-10
```

> [!IMPORTANT]  
> The HUD is non-functional until GAP-10 (integer-to-string conversion) is implemented.
> As a workaround, write digits manually to a RAM buffer using bitwise decomposition,
> then `draw_text` the buffer contents. This is complex but possible without new opcodes.

---

## 13. Multiple Levels & Level Transitions

### Level Structure

A complete Sonic-style game has:
```
Zone 1 → Act 1 → Act 2 → Boss
Zone 2 → Act 1 → Act 2 → Boss
Zone 3 → Act 1 → Act 2 → Boss (Final)
```

### Level Data Design

Each level needs:
1. **Palette**: Different color schemes per zone
2. **Tile set**: Different graphics per zone  
3. **Level map**: Different layout per act
4. **Enemy placement**: Different enemies per level
5. **Ring placement**: Unique ring positions per act
6. **Music track**: Different BGM per zone

Today, all of this is compile-time static — the binary contains one palette, one tile set,
one level layout. Multi-level support requires a design decision:

**Approach A — Single Binary, Multiple Level Data in RAM:**
Pack multiple levels' tile map data into the binary's Data Segment. At runtime, copy
the appropriate level's data into the active tilemap region of Game RAM.

```booey
# Level data in binary (conceptually):
# Level 1 tilemap: bytes 0x1000 - 0x2FFF in Data Segment
# Level 2 tilemap: bytes 0x3000 - 0x4FFF in Data Segment
# Level 3 tilemap: bytes 0x5000 - 0x6FFF in Data Segment

# The loader copies the right block to the active tilemap
# This requires a new built-in: load_level(level_id)

fn load_level(level_id: int):
    # Select source offset in data segment
    let src_offset: int = level_id * 8192     # 8KB per level
    current_level = level_id
    level_timer = 0
    rings = 0
    checkpoint_set = 0
    # Copy level data to active tilemap RAM...
    # Then rebuild TILE entries (requires TILE opcode access via GAP-11)
```

**Approach B — Multiple Binaries (Recommended for full game):**
Each level is a separate `.booey` binary. The game manager (outside the VM) handles
launching the next level binary. Cross-level state is passed via MMIO save slots (GAP-4).

### Level Transition Sequence

```booey
vars:
    GAME_STATE_PLAYING:    int = 0
    GAME_STATE_LEVELCLEAR: int = 1
    GAME_STATE_GAMEOVER:   int = 2
    GAME_STATE_TITLE:      int = 3
    
    game_state: int = 3
    transition_timer: int = 0

fn trigger_level_clear():
    game_state = GAME_STATE_LEVELCLEAR
    transition_timer = 300   # 5 second celebration before next level
    
    # Calculate time bonus
    let seconds: int = level_timer / 60
    if seconds < 30: score = score + 5000
    elif seconds < 60: score = score + 4000
    elif seconds < 90: score = score + 3000

fn update_level_clear():
    transition_timer = transition_timer - 1
    
    # Draw celebration screen
    cls(black)
    draw_text(220, 200, "LEVEL CLEAR!", white)
    # draw_int(280, 240, score, yellow) -- needs GAP-10
    
    if transition_timer <= 0:
        # Load next level or show credits
        current_level = current_level + 1
        if current_level >= 3:
            # Game complete!
            game_state = GAME_STATE_TITLE
        else:
            # Transition to next level
            # Approach A: reload tilemap in RAM
            # Approach B: call exit() and host loads next binary
            exit()
```

---

## 14. Persistent State Across Levels

This section details the inter-level save system once **GAP-4** (MMIO save slots) is resolved.

### MMIO Save Region Layout (Proposed)

```
MMIO (0x1C000 - 0x1CFFF)
0x1C000  Input: button held mask (read-only)
0x1C004  Input: button pressed mask (read-only)
0x1C008  Input: button released mask (read-only)
0x1C00C  (reserved)
0x1C010  System: frame counter (read-only)
0x1C014  System: elapsed time ms (read-only)
0x1C018  (reserved)
0x1C01C  (reserved)
─────── NEW SAVE REGION (not cleared on VM reset) ───────
0x1C020  Save: lives remaining (uint32)
0x1C024  Save: current level index (uint32)
0x1C028  Save: score - low 32 bits (uint32)
0x1C02C  Save: score - high 32 bits (uint32, for scores > 4 billion)
0x1C030  Save: rings carried into level (uint32)
0x1C034  Save: player_x spawn override (uint32, 0 = use default)
0x1C038  Save: player_y spawn override (uint32)
0x1C03C  Save: game flags bitmask (checkpoints, secrets unlocked, etc.)
```

### Using Save State in Game Code

```booey
fn save_inter_level_state():
    # Write state to MMIO save region before calling exit()
    # Requires STORE opcode to MMIO range — currently causes VM fault
    # After GAP-4 is resolved:
    # store(0x1C020, lives)
    # store(0x1C024, current_level + 1)  # Next level to load
    # store(0x1C028, score)
    # store(0x1C030, rings)
    exit()

fn load_inter_level_state():
    # On game init, read previous level's saved state
    # Requires LOAD from MMIO save region:
    # lives = load(0x1C020)
    # current_level = load(0x1C024)
    # score = load(0x1C028)
    # rings = load(0x1C030)
    pass
```

---

## 15. Audio Design

A Sonic-style game needs at least these audio events:

| Event | Sound Type | Notes |
|---|---|---|
| Jump | Sweep up | 200 Hz → 800 Hz, 150ms |
| Ring collect | Short tone | 800 Hz, 80ms, square wave |
| Rings scatter | Descending sweep | Multiple pitches |
| Player hurt | Low thud + noise | 2-part: impact + noise |
| Enemy stomp | Pop | 400 Hz, 60ms |
| Level clear | Fanfare | Multi-note melody |
| Level BGM | Multi-channel | Repeating music loop |
| Checkpoint | Chime | C major arpeggio |

### Sound Definitions

```booey
sounds:
    snd_jump:
        type: sweep
        start_freq: 200
        end_freq: 800
        duration_ms: 150
        wave: square
        volume: 70
    
    snd_ring:
        type: tone
        start_freq: 800
        duration_ms: 80
        wave: square
        volume: 60
    
    snd_hurt:
        type: noise
        duration_ms: 200
        decay: fast
        volume: 90
    
    snd_stomp:
        type: tone
        start_freq: 400
        duration_ms: 60
        wave: square
        volume: 75
    
    snd_checkpoint:
        type: arpeggio
        notes: [262, 330, 392, 523]  # C E G C (C major)
        duration_ms: 400
        wave: square
        volume: 80
```

### Playing Sounds

```booey
# Requires GAP-15 (sfx() codegen wiring) to work:
fn play_jump_sfx():
    # sfx(snd_jump)  -- once GAP-15 is fixed

fn play_ring_collect_sfx():
    # sfx(snd_ring)

fn play_hurt_sfx():
    # sfx(snd_hurt)
```

---

## 16. Full Game Architecture

The complete update loop for a Sonic-style game:

```booey
fn init():
    # Load save state from MMIO (GAP-4)
    load_inter_level_state()
    
    # Initialize player at spawn point
    px = 100 * 65536    # Fixed-point: 100.0
    py = 300 * 65536
    vx = 0
    vy = 0
    grounded = 0
    player_state = STATE_NORMAL
    facing_right = 1
    
    # Initialize level data
    init_rings()
    init_enemies()
    level_timer = 0
    game_state = GAME_STATE_PLAYING

fn update():
    # Branch on game state
    if game_state == GAME_STATE_TITLE:
        update_title_screen()
        return
    elif game_state == GAME_STATE_GAMEOVER:
        update_game_over_screen()
        return
    elif game_state == GAME_STATE_LEVELCLEAR:
        update_level_clear()
        return
    
    # ─── GAME_STATE_PLAYING ───────────────────────
    
    # 1. Input & Physics Update
    update_movement()
    update_gravity_and_jump()
    update_hurt_timer()
    
    # 2. Integrate position
    integrate_position()
    
    # 3. Collision Resolution
    check_ground()
    check_ceiling()
    check_walls()
    
    # 4. Game Object Updates
    check_ring_collisions()
    update_dropped_rings()
    update_enemies()
    check_enemy_player_collision_all()
    check_hazards()
    check_goal_ring()    # End of level trigger
    
    # 5. Camera
    update_camera()
    
    # ─── RENDERING ────────────────────────────────
    # Clear screen with sky color
    cls(sky)
    
    # Background layers (requires GAP-9)
    # tdraw(0)   # Far background (sky, hills)
    # tdraw(1)   # Mid background (trees)
    # tdraw(2)   # Foreground tiles (terrain)
    
    # Render rings
    draw_rings()
    
    # Render enemies
    draw_enemies()
    
    # Render player (with invincibility flicker)
    draw_player()
    
    # Render particles / VFX
    draw_particles()
    
    # Foreground layer on top (requires GAP-9)
    # tdraw(3)   # Foreground details above player
    
    # HUD (always drawn on top, no parallax)
    update_hud()

fn draw_player():
    let ix: int = px / 65536 - cam_x
    let iy: int = py / 65536 - cam_y
    
    # Skip drawing every other frame when invincible (flicker)
    if invincible == 1:
        let f: int = frame()
        if f / 4 * 4 == f:   # Every 4 frames, skip
            return
    
    # Select sprite based on state and velocity
    if player_state == STATE_HURT:
        draw_sprite(spr_hurt, ix, iy)
    elif player_state == STATE_ROLLING or player_state == STATE_JUMPING:
        draw_sprite(spr_roll, ix, iy)
    elif vx > 65536 or vx < -65536:   # abs(vx) > 1.0
        # Running animation: cycle through 8 frames
        let run_speed: int = vx
        if run_speed < 0: run_speed = -run_speed
        let anim_rate: int = 8 - run_speed / 65536   # Faster running = faster anim
        if anim_rate < 2: anim_rate = 2
        let anim_idx: int = (frame() / anim_rate) % 8
        # draw_sprite(spr_run_frames[anim_idx], ix, iy) -- needs arrays
        draw_sprite(spr_run, ix, iy)  # Placeholder
    else:
        # Idle: breathing animation (slow 2-frame cycle)
        if (frame() / 30) % 2 == 0:
            draw_sprite(spr_idle, ix, iy)
        else:
            draw_sprite(spr_idle2, ix, iy)
```

---

## 17. Required VM/Language Additions

This table summarizes every gap and its resolution. Items marked ✅ are available today;
items marked ❌ require implementation work.

### Language & Compiler Additions

| Feature | Gap # | Effort | Where to Fix |
|---|---|---|---|
| Array types (`int[N]`) | GAP-1 | High | `parser.cpp`, `codegen.cpp`, `ast.hpp` |
| `draw_sprite_ex()` built-in | GAP-7 | Low | `codegen.cpp` FuncCallNode handler |
| `abs()`, `min()`, `max()`, `clamp()` | GAP-8 | Low | `codegen.cpp` FuncCallNode handler |
| `tscroll()`, `tdraw()` built-ins | GAP-9 | Low | `codegen.cpp` FuncCallNode handler |
| `tile()` built-in | GAP-11 | Low | `codegen.cpp` FuncCallNode handler |
| `frame()` built-in | GAP-12 | Low | `codegen.cpp` FuncCallNode handler |
| `sfx()`, `play()` built-ins | GAP-15 | Low | `codegen.cpp` FuncCallNode handler |
| String format / `draw_int()` | GAP-10 | Medium | New opcode `ITOA` + codegen handler |
| `%` (modulo) operator in expressions | GAP-13 | Low | Verify `codegen.cpp` BinaryOpNode `->`  `OP_MOD` |
| `btn_released()` built-in | GAP-16 | Low | New opcode `BTNR` + codegen handler |

### VM Additions

| Feature | Gap # | Effort | Where to Fix |
|---|---|---|---|
| `OP_SCOL` sprite collision | GAP-2 | Medium | `vm.cpp` `case OP_SCOL:` implementation |
| `OP_COS` cosine function | GAP-5 | Low | New opcode `0x75`, `vm.cpp`, `opcodes.hpp` |
| `OP_ATAN2` arc tangent | GAP-5 | Low | New opcode `0x76`, `vm.cpp`, `opcodes.hpp` |
| `OP_ITOA` int-to-ASCII | GAP-10 | Medium | New opcode `0x77`, writes digits to RAM |
| `OP_BTNR` button released | GAP-16 | Low | New opcode `0x62` slot, `vm.cpp` |

### Architecture Additions

| Feature | Gap # | Effort | Where to Fix |
|---|---|---|---|
| Level/tilemap loading API | GAP-3 | High | New `levels:` compiler block + `load_level()` built-in |
| MMIO save slots (non-volatile) | GAP-4 | Medium | `vm.cpp` — don't clear `mmio_[0x20..0xFF]` on `reset()` |
| Register spilling | GAP-6 | High | `codegen.cpp` allocator |

### Proposed New Opcodes

```
0x62 | BTNR  | Rd, R_btn  | Set Rd=1 if R_btn was just released this frame, else 0
0x75 | COS   | Rd, Rs     | Rd = Cosine(Rs), fixed-point (same format as SIN)
0x76 | ATAN2 | Rd, Ry, Rx | Rd = atan2(Ry, Rx), returns angle in fixed-point radians
0x77 | ITOA  | Rd, Rs     | Write decimal ASCII digits of Rs to RAM addr in Rd, null-terminated
0x78 | LOADL | Rd, Rs, imm8 | Load 32-bit from (Rs + imm8*4) — for array element access
0x79 | STORL | Rd, Rs, imm8 | Store 32-bit to (Rd + imm8*4) — for array element write
```

---

## 18. Implementation Roadmap

Follow this order to build from the current working state to a complete Sonic-style game:

### Sprint 1 — Fix Easy Codegen Gaps (1–2 days)

These are all trivial additions to `codegen.cpp`'s `FuncCallNode::visit`:

1. **Wire `draw_sprite_ex()`** → emit `OP_SPREX`
2. **Wire `tscroll()`** → emit `OP_TSCROLL`
3. **Wire `tdraw()`** → emit `OP_TDRAW`
4. **Wire `tile()`** → emit `OP_TILE`
5. **Wire `frame()`** → emit `OP_FRAME`
6. **Wire `sfx()`** → emit `OP_SFX`
7. **Wire `abs()`** → inline with `SUB`/`NOT`/`AND` sequence
8. **Wire `min()`, `max()`, `clamp()`** → inline with `CMP`/`JG`/`JL` sequences
9. **Verify `%` operator** maps to `OP_MOD` in `BinaryOpNode`

**Test:** A sprite that scrolls a tiled background, plays sounds, and uses flip flags.

---

### Sprint 2 — Implement `OP_SCOL` and Display Integers (2–3 days)

1. **Add `OP_ITOA`** to `opcodes.hpp` and implement in `vm.cpp`
   - Converts uint32 to ASCII decimal in a RAM buffer
2. **Add `draw_int()` built-in** to Booey (wrapper around `ITOA` + `TEXT`)
3. **Implement `OP_SCOL`** in `vm.cpp`
   - Reads sprite dimensions from VRAM at `sprite_base + id * stride`
   - Performs AABB overlap check
4. **Wire `check_collision()`** in `codegen.cpp` → emit `OP_SCOL`

**Test:** HUD displays live ring/score counts; player can collide with rings.

---

### Sprint 3 — Add Array Support to Booey Language (3–5 days)

1. **Extend `ast.hpp`** with `ArrayDeclNode`, `ArrayAccessNode`, `ArrayAssignNode`
2. **Extend `parser.cpp`** to parse `name: type[size]` and `name[idx]` access
3. **Extend `codegen.cpp`** to:
   - Allocate arrays in Game RAM (track next free RAM address)
   - Emit `LOADR`/`STORER` for `name[i]` access
4. **Add `OP_LOADL`/`OP_STORL`** for constant-offset array access (optimization)

**Test:** Ring placement array, enemy position array, dropped ring physics.

---

### Sprint 4 — Persistent State & Multi-Level (2–3 days)

1. **Reserve MMIO 0x1C020–0x1C03F** as non-volatile save area
2. **Modify `vm.cpp` `reset()`** to skip clearing the save region
3. **Add `save()` / `load()` built-ins** in codegen for MMIO save slot access
4. **Implement multi-level design pattern** (Approach A or B from section 13)

**Test:** Score and lives persist across a level restart triggered by `exit()`.

---

### Sprint 5 — Trig Functions + Full Physics (2 days)

1. **Add `OP_COS`** (`0x75`) to opcodes and implement in `vm.cpp`
2. **Add `OP_ATAN2`** (`0x76`) to opcodes and implement in `vm.cpp`
3. **Wire `cos()`, `atan2()`, `sin()`** in codegen
4. **Implement slope physics** using ground angle → velocity decomposition

**Test:** Player navigates curved terrain at speed without leaving the surface.

---

### Sprint 6 — Polish: Animations, Audio, Particles (3–5 days)

1. **Multi-frame animation system** using arrays (post Sprint 3)
2. **Ring scatter with circular spread** using `sin()`/`cos()`
3. **BGM loop system** using `OP_PLAY` multi-channel
4. **Particle system** for dust, ring sparkles, enemy explosions
5. **Invincibility flicker** using `frame()` modulo

---

### Final — A Complete Playable Game

After all sprints, the resulting game structure would be:

```
sonic-style-game/
├── main.booey         # Zone 1 Act 1
├── zone1act2.booey    # Zone 1 Act 2
├── zone1boss.booey    # Zone 1 Boss
├── zone2act1.booey    # Zone 2 Act 1
├── ...
└── credits.booey      # Ending screen
```

Each binary shares the save state via MMIO slots, creating a continuous experience
across the level sequence.

---

> [!TIP]  
> Start with **Sprint 1** immediately — those changes take less than an hour each
> and unlock scrolling backgrounds, sounds, and sprite flipping, which dramatically
> improves the prototype. A minimal playable Sonic demo (running, jumping, collecting
> rings on a scrolling background) is achievable before completing all 6 sprints.

> [!NOTE]  
> The existing `games/hello-world/main.booey` is a working reference. All tests pass
> with the current VM after the register allocation and lexer fixes made to the compiler.
> Use the test suite (`./build/tests/ooey-station-tests`) to verify changes don't regress
> existing functionality.

---

*See also:*
- [06: VM & Bytecode Specification](./06-booey-virtual-machine-and-bytecode.md) — Full opcode table
- [07: Booey Language Reference](./07-booey-scripting-language-reference.md) — Language syntax
- [09: Asset System](./09-asset-system.md) — Sprite and tile definition format
- [10: Game Development Guide](./10-game-development-guide.md) — General game development patterns
