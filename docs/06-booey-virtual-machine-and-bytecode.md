# Ooey-Station: Booey Virtual Machine & Bytecode Specification

This document defines the complete architecture of the Booey Virtual Machine and the binary format it executes. The Booey VM is the heart of the Ooey-Station, responsible for enforcing hardware constraints, managing memory, and providing the execution environment for games.

## 1. Design Philosophy

The Booey VM is designed with these goals:
1. **LLM-Friendly**: The instruction set is high-level enough that an LLM can reason about it and generate it reliably. Complex operations (like drawing a sprite) are single opcodes rather than intricate memory manipulation loops.
2. **Powerful but Constrained**: It provides enough power for classic 2D game genres (platformers, RPGs, arcade) while enforcing strict resource limits (memory, sprites).
3. **Safe Execution**: The VM operates within a strict sandbox. Invalid memory accesses or illegal opcodes halt the VM safely, returning control to the Game Browser rather than crashing the host application.
4. **Register-Based**: A register-based architecture (rather than stack-based) maps more cleanly to the high-level Booey scripting language and is generally easier to optimize and interpret efficiently.

---

## 2. VM Architecture Overview

### CPU State
- **Registers**: 16 general-purpose 32-bit registers, denoted `R0` through `R15`.
  - `R0` is conventionally used for return values.
  - `R15` is conventionally used as the Stack Pointer (SP), though the hardware manages the call stack separately.
- **Program Counter (PC)**: Points to the next instruction in the Code segment.
- **Flags**:
  - `Z` (Zero): Set if the result of an operation is 0.
  - `N` (Negative): Set if the result is less than 0.
  - `C` (Carry): Set if an arithmetic operation overflows.

### Memory Model
The VM accesses a unified 128KB address space, partitioned as follows:

| Address Range | Size | Purpose | Description |
|---|---|---|---|
| `0x00000` - `0x0FFFF` | 64 KB | **Game RAM** | General purpose read/write memory for game state, variables, and stack. |
| `0x10000` - `0x17FFF` | 32 KB | **Video RAM (VRAM)** | Memory for sprite and tilemap data definitions. |
| `0x18000` - `0x1BFFF` | 16 KB | **Audio RAM (ARAM)** | Memory for sound effect and instrument definitions. |
| `0x1C000` - `0x1CFFF` | 4 KB | **Memory-Mapped I/O** | Registers for hardware interaction (Input, display control, system time). |
| `0x1D000` - `0x1FFFF` | 12 KB | **Reserved** | Unused. Accessing causes a fault. |

*Note: The actual executable code (.booey bytecode) and static assets are loaded into separate, read-only memory spaces managed by the VM, not within this 128KB address space.*

### Data Types
The VM natively operates on 32-bit values:
- `int32`: Signed 32-bit integer.
- `uint8`: Unsigned byte (when reading/writing to memory using byte-specific opcodes).
- `fixed16.16`: Fixed-point number for smooth movement (16 bits integer, 16 bits fractional).
- `bool`: Represented as 0 (false) or non-zero (true).

---

## 3. Memory-Mapped I/O (MMIO)

Specific addresses in the MMIO region control hardware features:

### Input Registers (Read-Only)
- `0x1C000` (uint32): Current button state bitmask (Held).
- `0x1C004` (uint32): Buttons just pressed this frame bitmask.
- `0x1C008` (uint32): Buttons just released this frame bitmask.

### System Registers (Read-Only)
- `0x1C010` (uint32): Frame Counter (increments once per `VBLANK`).
- `0x1C014` (uint32): Elapsed time since boot in milliseconds.

---

## 4. Binary File Format (`.booey`)

A compiled `.booey` game binary is structured into sections:

### Header (32 bytes)
| Offset | Type | Name | Description |
|---|---|---|---|
| 0x00 | char[4] | Magic | Must be `B`, `O`, `O`, `E` (0x454F4F42) |
| 0x04 | uint16 | Version | Format version (currently 0x0001) |
| 0x06 | uint16 | Flags | Reserved for future use |
| 0x08 | uint32 | Code Size | Size of the Code segment in bytes |
| 0x0C | uint32 | Data Size | Size of the Data segment in bytes (loaded into Game RAM) |
| 0x10 | uint32 | Asset Size | Size of the Asset segment in bytes |
| 0x14 | uint32 | Entry Point | Offset into the Code segment where execution begins |
| 0x18 | uint32 | Checksum | CRC32 of the entire file (excluding this field) |
| 0x1C | uint32 | Reserved | Padding |

### Segments
Following the header are the contiguous segments defined by their respective sizes:
1.  **Code Segment**: Contains the bytecode instructions. Read-only.
2.  **Data Segment**: Contains initial values for global variables, string literals, and level data. This data is copied to `0x00000` in Game RAM upon initialization.
3.  **Asset Segment**: Contains procedural definitions for sprites, tilemaps, and sounds.

---

## 5. Instruction Encoding

Instructions are variable-length (1 to 6 bytes).

**Encoding Format:**
`[Opcode: 1 byte] [Operand 1] [Operand 2] ...`

**Operand Types:**
- **Register (`r`)**: 1 byte. Values `0x00` to `0x0F` map to `R0` to `R15`.
- **Immediate (`i8`, `i16`, `i32`)**: 1, 2, or 4 bytes. Signed values are two's complement.
- **Address (`a`)**: 4 bytes (uint32). Points to a memory location.

*Endianness: All multi-byte values in the bytecode and memory are Little-Endian.*

---

## 6. Complete Opcode Table

*Legend:*
*   `Rd`, `Rs`: Destination / Source Register (1 byte)
*   `imm`: Immediate value
*   `addr`: Address value (4 bytes)

### 6.1. Arithmetic & Logic

| Hex | Mnemonic | Operands | Description |
|---|---|---|---|
| 0x00 | `NOP` | - | No operation. |
| 0x01 | `MOV` | `Rd, Rs` | `Rd = Rs` |
| 0x02 | `MOVI`| `Rd, i32`| `Rd = imm` |
| 0x10 | `ADD` | `Rd, Rs` | `Rd = Rd + Rs`. Sets C flag on overflow. |
| 0x11 | `SUB` | `Rd, Rs` | `Rd = Rd - Rs`. Sets C flag on underflow. |
| 0x12 | `MUL` | `Rd, Rs` | `Rd = Rd * Rs` |
| 0x13 | `DIV` | `Rd, Rs` | `Rd = Rd / Rs`. Faults if Rs is 0. |
| 0x14 | `MOD` | `Rd, Rs` | `Rd = Rd % Rs`. Faults if Rs is 0. |
| 0x15 | `AND` | `Rd, Rs` | `Rd = Rd & Rs` (Bitwise AND) |
| 0x16 | `OR`  | `Rd, Rs` | `Rd = Rd \| Rs` (Bitwise OR) |
| 0x17 | `XOR` | `Rd, Rs` | `Rd = Rd ^ Rs` (Bitwise XOR) |
| 0x18 | `NOT` | `Rd`     | `Rd = ~Rd` (Bitwise NOT) |
| 0x19 | `SHL` | `Rd, Rs` | `Rd = Rd << Rs` |
| 0x1A | `SHR` | `Rd, Rs` | `Rd = Rd >> Rs` |
| 0x1B | `CMP` | `Rd, Rs` | Performs `Rd - Rs` and sets Z, N flags based on result. Discards result. |

### 6.2. Memory Access

| Hex | Mnemonic | Operands | Description |
|---|---|---|---|
| 0x20 | `LOAD`  | `Rd, addr`| Load 32-bit value from `addr` into `Rd`. |
| 0x21 | `STORE` | `addr, Rs`| Store 32-bit value from `Rs` into `addr`. |
| 0x22 | `LOADR` | `Rd, Rs`  | Load 32-bit value from address specified by `Rs` into `Rd`. |
| 0x23 | `STORER`| `Rd, Rs`  | Store 32-bit value from `Rs` into address specified by `Rd`. |
| 0x24 | `LOADB` | `Rd, addr`| Load 8-bit unsigned byte from `addr` into `Rd` (zero-extended). |
| 0x25 | `STOREB`| `addr, Rs`| Store lowest 8 bits of `Rs` into `addr`. |

### 6.3. Control Flow

| Hex | Mnemonic | Operands | Description |
|---|---|---|---|
| 0x30 | `JMP`   | `addr`   | Unconditional jump. `PC = addr`. |
| 0x31 | `JZ`    | `addr`   | Jump if Zero flag is set. |
| 0x32 | `JNZ`   | `addr`   | Jump if Zero flag is NOT set. |
| 0x33 | `JL`    | `addr`   | Jump if Negative flag is set (Rd < Rs from CMP). |
| 0x34 | `JG`    | `addr`   | Jump if Negative flag is NOT set and Zero is NOT set (Rd > Rs). |
| 0x35 | `CALL`  | `addr`   | Push current PC to call stack, then `PC = addr`. |
| 0x36 | `RET`   | -        | Pop address from call stack into PC. |
| 0x37 | `HALT`  | -        | Stop execution. VM yields immediately. |

### 6.4. Graphics & Primitives

| Hex | Mnemonic | Operands | Description |
|---|---|---|---|
| 0x40 | `CLS`   | `Rs`     | Clear virtual display with color index `Rs`. |
| 0x41 | `PIXEL` | `R_x, R_y, R_c`| Set pixel at (x,y) to color `R_c`. |
| 0x42 | `LINE`  | `R_x1, R_y1, R_x2, R_y2, R_c`| Draw line. |
| 0x43 | `RECT`  | `R_x, R_y, R_w, R_h, R_c`| Draw rectangle outline. |
| 0x44 | `FRECT` | `R_x, R_y, R_w, R_h, R_c`| Draw filled rectangle. |
| 0x45 | `TEXT`  | `R_x, R_y, addr, R_c` | Draw null-terminated string at `addr` to (x,y). |
| 0x46 | `VBLANK`| -        | Suspend execution until the next frame. Crucial for game loops. |

### 6.5. Sprites & Tilemaps

| Hex | Mnemonic | Operands | Description |
|---|---|---|---|
| 0x50 | `SPR`   | `R_id, R_x, R_y` | Draw sprite `id` at position (x,y). |
| 0x51 | `SPREX` | `R_id, R_x, R_y, R_flags` | Draw sprite with flip/rotation flags. |
| 0x52 | `SCOL`  | `R_dst, R_id1, R_id2` | Check collision between two sprites. Sets `R_dst` to 1 if colliding, 0 otherwise. |
| 0x53 | `TILE`  | `R_layer, R_tx, R_ty, R_tid` | Set tile ID at (tx,ty) on background layer. |
| 0x54 | `TSCROLL`| `R_layer, R_dx, R_dy` | Set scroll offset for background layer. |
| 0x55 | `TDRAW` | `R_layer` | Render background layer to virtual display. |

### 6.6. Input & Audio

| Hex | Mnemonic | Operands | Description |
|---|---|---|---|
| 0x60 | `BTNP`  | `Rd, R_btn` | Set `Rd` to 1 if `R_btn` was just pressed this frame, else 0. |
| 0x61 | `BTNH`  | `Rd, R_btn` | Set `Rd` to 1 if `R_btn` is currently held, else 0. |
| 0x62 | `SFX`   | `R_id` | Play sound effect `id`. |
| 0x63 | `PLAY`  | `R_ch, R_freq, R_dur, R_wave` | Play raw tone on audio channel. |

### 6.7. Math & Utility

| Hex | Mnemonic | Operands | Description |
|---|---|---|---|
| 0x70 | `RND`   | `Rd, Rs` | `Rd = Random(0, Rs - 1)`. |
| 0x71 | `SIN`   | `Rd, Rs` | `Rd = Sine(Rs)`. (Uses fixed-point). |
| 0x72 | `DIST`  | `Rd, R_x1, R_y1, R_x2, R_y2` | `Rd = Distance between points`. |
| 0x73 | `FRAME` | `Rd`     | `Rd = Current Frame Counter`. |
| 0x74 | `EXIT`  | -        | Signal the host application to close the game and return to browser. |

---

## 7. VM Execution Loop

The core execution loop is implemented in C++ within the `BooeyVM` class.

```cpp
void BooeyVM::execute_frame() {
    // Run until a VBLANK instruction is encountered or execution halts
    while (!wait_for_vblank_ && !halted_) {
        uint8_t opcode = code_memory_[pc_++];
        
        switch (opcode) {
            case OP_MOV: {
                uint8_t rd = code_memory_[pc_++];
                uint8_t rs = code_memory_[pc_++];
                registers_[rd] = registers_[rs];
                break;
            }
            case OP_JMP: {
                uint32_t addr = read_uint32(code_memory_, pc_);
                pc_ = addr;
                break;
            }
            case OP_VBLANK: {
                wait_for_vblank_ = true;
                break;
            }
            // ... (implement all other opcodes)
            default:
                halted_ = true;
                trigger_error("Illegal Opcode");
                return;
        }
    }
    
    // Clear the VBLANK flag for the next frame iteration
    wait_for_vblank_ = false;
}
```

This loop is called 60 times per second by the Ooey-Station Application event loop.
