# 01 — Project Setup & Build System

> Ooey-Station build infrastructure, directory layout, CMake design, and
> step-by-step instructions for every supported platform.

---

## 1. Directory Structure

```
ooey-station/
├── CMakeLists.txt                  # Root build file (see §2)
├── README.md
├── docs/
│   ├── 00-architecture.md          # System architecture overview
│   ├── 01-project-setup-and-build-system.md   # ← this file
│   └── ...
├── games/                          # Game library (shipped ROMs / scripts)
│   ├── hello-world/
│   │   ├── main.booey              # Booey source
│   │   └── assets/                 # Per-game sprites, maps, sounds
│   └── ...
├── src/
│   ├── station/                    # Shell / OS-level code
│   │   ├── bootloader.hpp/.cpp     # Splash sequence, ROM loader
│   │   └── browser.hpp/.cpp        # Game library browser UI
│   ├── console/                    # Virtual hardware emulation
│   │   ├── display.hpp/.cpp        # 256×240 framebuffer, palette, layers
│   │   ├── input.hpp/.cpp          # Virtual gamepad → ooey::InputManager
│   │   ├── audio.hpp/.cpp          # PSG / wavetable synth engine
│   │   └── memory.hpp/.cpp         # 64 KiB address space, bank switching
│   ├── vm/                         # Booey bytecode virtual machine
│   │   ├── vm.hpp/.cpp             # Fetch-decode-execute, registers
│   │   ├── opcodes.hpp             # Instruction enum + encoding tables
│   │   └── debug.hpp/.cpp          # Breakpoints, step, memory inspector
│   ├── assets/                     # Content-generation tooling (built-in)
│   │   ├── sprite_gen.hpp/.cpp     # Pixel sprite editor / procedural gen
│   │   ├── tilemap_gen.hpp/.cpp    # Tile-map layout tools
│   │   └── sound_gen.hpp/.cpp      # Waveform / tracker helpers
│   └── compiler/                   # booeyc — Booey-language compiler
│       ├── lexer.hpp/.cpp          # Tokenizer
│       ├── parser.hpp/.cpp         # Recursive-descent parser → AST
│       ├── ast.hpp                 # AST node types
│       ├── codegen.hpp/.cpp        # AST → Booey bytecode
│       └── main.cpp                # CLI entry point for booeyc
├── tests/
│   ├── CMakeLists.txt
│   ├── vm_tests.cpp
│   ├── compiler_tests.cpp
│   └── console_tests.cpp
└── .gitignore
```

### Key conventions

| Convention | Rule |
|---|---|
| **Headers** | Every `.cpp` has a matching `.hpp` in the same directory. No separate `include/` tree — keeps locality tight for an application (not a redistributable library). |
| **Namespaces** | `ooey_station::station`, `ooey_station::console`, `ooey_station::vm`, `ooey_station::compiler`, etc. |
| **Games directory** | Each game is a subdirectory with a `main.booey` and an optional `assets/` folder. The build system does **not** compile games — the user runs `booeyc` manually or through a dev-mode hot-reload path. |

---

## 2. CMake Configuration

### 2.1 Complete Root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

# ── Forward CMake policies to match the parent ooey project ──────────────
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(ooey-station VERSION 0.1.0 LANGUAGES CXX)

# ── C++20 everywhere ────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──────────────────────────────────────────────────────────────
option(OOEY_STATION_ASAN   "Enable AddressSanitizer (debug builds)" ON)
option(OOEY_STATION_TESTS  "Build the test suite"                   ON)

# ── Sanitizers (dev builds only) ────────────────────────────────────────
if(OOEY_STATION_ASAN AND NOT EMSCRIPTEN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# §2.2  Pull in the parent ooey library
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#
# The ooey repo lives as a sibling directory.  add_subdirectory() brings
# in three targets:
#
#   ooey   — low-level rendering, windowing, input (ooey/include/ooey/)
#   gooey  — UI toolkit & MVVMC framework       (gooey/include/gooey/)
#   tooey  — terminal UI layer                   (tooey/include/tooey/)
#
# Only `ooey` and `gooey` are used by ooey-station.
# ─────────────────────────────────────────────────────────────────────────
set(OOEY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ooey"
    CACHE PATH "Path to the ooey source tree")

if(NOT EXISTS "${OOEY_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "Cannot find ooey at ${OOEY_ROOT}.\n"
        "Clone the ooey repo as a sibling directory, or set -DOOEY_ROOT=<path>.")
endif()

# Suppress ooey's own examples / tests when consumed as a dependency
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory("${OOEY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/_ooey")

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# §2.3  Collect ooey-station source files
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# ── Console (virtual hardware) ───────────────────────────────────────────
set(CONSOLE_SRCS
    src/console/display.cpp
    src/console/input.cpp
    src/console/audio.cpp
    src/console/memory.cpp
)

# ── VM (Booey bytecode interpreter) ─────────────────────────────────────
set(VM_SRCS
    src/vm/vm.cpp
    src/vm/debug.cpp
)

# ── Station (shell / UI) ────────────────────────────────────────────────
set(STATION_SRCS
    src/station/bootloader.cpp
    src/station/browser.cpp
)

# ── Asset tools (sprite / tilemap / sound generation) ───────────────────
set(ASSET_SRCS
    src/assets/sprite_gen.cpp
    src/assets/tilemap_gen.cpp
    src/assets/sound_gen.cpp
)

# ── Compiler library (shared between booeyc CLI and the station) ────────
set(COMPILER_LIB_SRCS
    src/compiler/lexer.cpp
    src/compiler/parser.cpp
    src/compiler/codegen.cpp
)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# §2.4  Targets
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# ── booey-compiler (STATIC library — reused by app + CLI + tests) ───────
add_library(booey-compiler STATIC ${COMPILER_LIB_SRCS})
target_include_directories(booey-compiler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(booey-compiler PRIVATE ooey)   # for types only

# ── ooey-station  (main application) ────────────────────────────────────
#
# Uses ooey's add_ooey_executable() helper so the same source
# automatically produces an Android shared-lib when cross-compiling.
# If that helper isn't available (e.g. ooey was updated), fall back.
if(COMMAND add_ooey_executable)
    add_ooey_executable(ooey-station
        src/station/main.cpp
        ${STATION_SRCS}
        ${CONSOLE_SRCS}
        ${VM_SRCS}
        ${ASSET_SRCS}
    )
else()
    add_executable(ooey-station
        src/station/main.cpp
        ${STATION_SRCS}
        ${CONSOLE_SRCS}
        ${VM_SRCS}
        ${ASSET_SRCS}
    )
endif()

target_include_directories(ooey-station PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(ooey-station PRIVATE
    gooey            # pulls in ooey transitively (gooey → PUBLIC ooey)
    booey-compiler
)

# Embed the games/ directory path so the browser can find ROMs at runtime
target_compile_definitions(ooey-station PRIVATE
    OOEY_STATION_GAMES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/games"
)

# ── Emscripten-specific link flags ──────────────────────────────────────
if(EMSCRIPTEN)
    set_target_properties(ooey-station PROPERTIES SUFFIX ".html")
    target_link_options(ooey-station PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sLEGACY_GL_EMULATION=1
        --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/games@/games"
    )
endif()

# ── booeyc  (compiler CLI) ──────────────────────────────────────────────
add_executable(booeyc src/compiler/main.cpp)
target_include_directories(booeyc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(booeyc PRIVATE booey-compiler)

# ── Tests ────────────────────────────────────────────────────────────────
if(OOEY_STATION_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# §2.5  Platform compile flags
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ooey-station PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter       # suppress noise during early dev
    )
    target_compile_options(booeyc PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

if(MSVC)
    target_compile_options(ooey-station PRIVATE /W4 /permissive-)
    target_compile_options(booeyc PRIVATE /W4 /permissive-)
endif()
```

### 2.2 `tests/CMakeLists.txt`

```cmake
# tests/CMakeLists.txt
# ────────────────────────────────────────────────────────────────────────
# Lightweight test runner — no external framework required for now.
# Switch to Catch2 or GoogleTest later if desired.
# ────────────────────────────────────────────────────────────────────────

add_executable(ooey-station-tests
    vm_tests.cpp
    compiler_tests.cpp
    console_tests.cpp
)

target_include_directories(ooey-station-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Link against the pieces under test
target_link_libraries(ooey-station-tests PRIVATE
    booey-compiler
    ooey        # for renderer types used by console tests
)

# Register with CTest
add_test(NAME ooey-station-tests COMMAND ooey-station-tests)
```

### 2.3 Dependency Graph

```
┌──────────────────────────────────────────────────────────────────┐
│                         ooey  (parent repo)                      │
│                                                                  │
│   ┌────────┐   PUBLIC    ┌────────┐                              │
│   │  ooey  │ ◄────────── │ gooey  │   (gooey links ooey PUBLIC) │
│   └────┬───┘             └────┬───┘                              │
│        │                      │                                  │
└────────┼──────────────────────┼──────────────────────────────────┘
         │                      │
    (types only)           (PRIVATE link)
         │                      │
   ┌─────▼──────┐    ┌─────────▼───────────┐
   │  booey-    │    │    ooey-station      │
   │  compiler  │    │    (main app)        │
   └─────┬──────┘    └─────────┬───────────┘
         │                      │
         │      (PRIVATE link)  │
         └──────────────────────┘
                    │
              ┌─────▼─────┐
              │  booeyc   │
              │  (CLI)    │
              └───────────┘
```

**Why this structure?**

| Link | Reason |
|---|---|
| `ooey-station → gooey (PRIVATE)` | The station app is a gooey `Application`. Because gooey links ooey `PUBLIC`, ooey-station automatically gets ooey's include paths, OpenGL/Vulkan/X11 libs, etc. — no manual plumbing. |
| `ooey-station → booey-compiler (PRIVATE)` | The station embeds the compiler for hot-reload / in-app editing. The compiler is a separate static lib so `booeyc` can also link it without duplicating sources. |
| `booey-compiler → ooey (PRIVATE)` | The compiler uses a handful of ooey types (`Color`, `Rect`) for asset metadata. It does **not** use gooey. |
| `booeyc → booey-compiler (PRIVATE)` | Thin CLI wrapper — `main.cpp` → parse args → call into the compiler library. |

---

## 3. Build Instructions

### 3.1 Prerequisites

```bash
# Ubuntu / Debian
sudo apt install build-essential cmake ninja-build \
    libx11-dev libgl-dev libvulkan-dev libpng-dev     # X11 build
# or for Wayland
sudo apt install libwayland-dev libxkbcommon-dev wayland-protocols \
    libegl-dev libwayland-egl-backend-dev

# Fedora
sudo dnf install cmake ninja-build gcc-c++ \
    libX11-devel mesa-libGL-devel vulkan-loader-devel libpng-devel

# Emscripten (any distro)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source emsdk_env.sh
```

### 3.2 Clone & Initial Layout

```bash
cd /home/corey/code

# Verify ooey is already present
ls ooey/CMakeLists.txt   # should exist

# The ooey-station repo is already initialised
cd ooey-station
```

### 3.3 Linux — X11 (Debug)

```bash
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DOOEY_BUILD_X11=ON \
    -DOOEY_BUILD_WAYLAND=OFF \
    -DOOEY_STATION_ASAN=ON

cmake --build build -j$(nproc)

# Run the station
./build/ooey-station

# Run the compiler
./build/booeyc games/hello-world/main.booey -o games/hello-world/main.rom

# Run the tests
cd build && ctest --output-on-failure
```

### 3.4 Linux — Wayland (Debug)

```bash
cmake -B build-wayland -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DOOEY_BUILD_X11=OFF \
    -DOOEY_BUILD_WAYLAND=ON \
    -DOOEY_STATION_ASAN=ON

cmake --build build-wayland -j$(nproc)

# Wayland sessions pick this up automatically
./build-wayland/ooey-station
```

### 3.5 Linux — Release

```bash
cmake -B build-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DOOEY_BUILD_X11=ON \
    -DOOEY_STATION_ASAN=OFF

cmake --build build-release -j$(nproc)

# Verify no ASAN overhead
./build-release/ooey-station
```

### 3.6 WebAssembly (Emscripten)

```bash
# Ensure emsdk is activated
source /path/to/emsdk/emsdk_env.sh

emcmake cmake -B build-wasm -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DOOEY_STATION_ASAN=OFF \
    -DOOEY_STATION_TESTS=OFF

cmake --build build-wasm -j$(nproc)

# Serve locally (Emscripten generates .html + .js + .wasm + .data)
cd build-wasm
python3 -m http.server 8080
# Open http://localhost:8080/ooey-station.html
```

> **Note:** The `--preload-file` flag in the CMakeLists bakes the `games/`
> directory into a `.data` bundle so the browser can access game ROMs
> through Emscripten's virtual filesystem at `/games/`.

### 3.7 Quick Reference

| Build | Command | Outputs |
|---|---|---|
| Debug X11 | `cmake -B build -DCMAKE_BUILD_TYPE=Debug` | `build/ooey-station`, `build/booeyc`, `build/tests/ooey-station-tests` |
| Debug Wayland | add `-DOOEY_BUILD_X11=OFF -DOOEY_BUILD_WAYLAND=ON` | same |
| Release | `-DCMAKE_BUILD_TYPE=Release -DOOEY_STATION_ASAN=OFF` | optimised binaries |
| WASM | `emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release` | `build-wasm/ooey-station.html` |

---

## 4. Development Workflow

### 4.1 Adding a New Game

```bash
# 1. Create the game directory
mkdir -p games/my-game/assets

# 2. Write the Booey source
cat > games/my-game/main.booey << 'EOF'
// Minimal Booey game
fn _init() {
    cls(0)
    spr(0, 64, 64)    // draw sprite 0 at (64,64)
}

fn _update() {
    if btn(0) { /* handle d-pad */ }
}

fn _draw() {
    cls(1)
    map(0, 0)          // draw tilemap
}
EOF

# 3. (Optional) Add sprite / tilemap assets in assets/

# 4. Compile to ROM
./build/booeyc games/my-game/main.booey -o games/my-game/main.rom

# 5. Launch the station — the browser will auto-detect new games
./build/ooey-station
```

### 4.2 The Compile → Run Loop

```
  ┌─────────────┐       booeyc        ┌──────────┐
  │  main.booey │  ──────────────────► │ main.rom │
  └─────────────┘                      └─────┬────┘
                                             │
                                     ooey-station
                                     loads ROM into
                                             │
                                      ┌──────▼──────┐
                                      │  Booey VM   │
                                      │  (execute)  │
                                      └─────────────┘
```

During development the hot-reload path will be:

1. Edit `.booey` source.
2. The station watches the `games/` directory (inotify on Linux, polling on WASM).
3. On change → re-invoke the embedded compiler (same `booey-compiler` library).
4. Reset the VM and reload the ROM — no restart required.

### 4.3 Compiling `.booey` Scripts with `booeyc`

```bash
# Basic compilation
./build/booeyc <input.booey> -o <output.rom>

# With verbose AST dump (for debugging the compiler itself)
./build/booeyc --dump-ast games/hello-world/main.booey

# Type-check only, no code generation
./build/booeyc --check games/hello-world/main.booey
```

### 4.4 Testing the VM

```bash
# Run the full test suite
cd build && ctest --output-on-failure

# Run only VM tests (by name filter)
cd build && ctest -R vm --output-on-failure

# Run only compiler tests
cd build && ctest -R compiler --output-on-failure

# Run with verbose output
cd build && ctest -V
```

Test files live in `tests/` and cover:

| File | Covers |
|---|---|
| `vm_tests.cpp` | Opcode execution, register operations, stack push/pop, memory load/store, branching, halt |
| `compiler_tests.cpp` | Lexer tokenisation, parser AST structure, codegen bytecode output, round-trip (source → compile → execute → verify) |
| `console_tests.cpp` | Display framebuffer writes, palette indexing, input mapping, memory bank switching |

---

## 5. Dependencies on OOEY

Ooey-Station is built entirely on top of the **ooey** rendering engine and the **gooey** UI framework. This section catalogues exactly which components are used and what extensions are needed.

### 5.1 Components Used As-Is

#### `gooey::Application` — Application Shell

```cpp
// src/station/main.cpp
#include <gooey/application.hpp>

int main() {
    gooey::Application app;
    // Application handles:
    //   • Window creation via IWindowBackend (X11/Wayland/Emscripten)
    //   • Main loop (run() / run_iteration())
    //   • DPI scaling
    //   • Render target lifecycle
    //   • UI thread dispatch
    app.set_root_view(/* station shell UI */);
    app.run();
}
```

**What it gives us for free:**
- Platform-agnostic window creation and main loop
- Automatic backend selection (X11, Wayland, Emscripten, Android)
- DPI-aware scaling via `set_dpi_scale_enabled()` / `get_dpi_scale()`
- `before_render_callback` / `after_render_callback` hooks — used to blit the console framebuffer onto the host window each frame
- `dispatch()` for marshalling work to the UI thread from VM/compiler threads

#### `ooey::InputManager` — Input Capture

```cpp
// src/console/input.cpp
#include <ooey/input.hpp>

void ConsoleInput::poll(ooey::InputManager& host_input) {
    // Map host keyboard → virtual gamepad
    // InputManager provides:
    //   • Pointer events  (touch / mouse)
    //   • Key events       (press / release with keycodes)
    //   • Text events      (Unicode codepoints)
    //   • Active pointer tracking
    //   • is_key_pressed() queries
    for (auto& key : host_input.get_key_events()) {
        map_to_gamepad(key);
    }
}
```

**What it gives us for free:**
- Unified keyboard/mouse/touch event model across X11, Wayland, Emscripten, and Android
- Per-frame event buffering (`get_key_events()`, `get_pointer_events()`)
- Key-held state tracking (`is_key_pressed()`)
- Scale-aware pointer coordinates (`set_scale()` / `get_scale()`)

#### `ooey::IRenderTarget` — Rendering Abstraction

```cpp
// The IRenderTarget interface provides:
virtual void clear(Color color) = 0;
virtual void draw_geometry(const Geometry& geometry) = 0;
virtual void draw_image(const Image& image, const Rect& dest_rect) = 0;
virtual void draw_text(const std::string& text, const Font& font,
                       const Point& position, Color color) = 0;
virtual Size measure_text(const std::string& text, const Font& font) = 0;
virtual void push_clip(const Rect& rect) = 0;
virtual void pop_clip() = 0;
virtual void present() = 0;
```

Used for:
- Rendering the station shell UI (game browser, bootloader splash)
- Displaying the console's virtual framebuffer onto the host window

#### `ooey::SoftwareRenderTarget` — Virtual Console Display

```cpp
// src/console/display.cpp
#include <ooey/renderer/software_render_target.hpp>

class ConsoleDisplay {
    // The console renders into a SoftwareRenderTarget backed by a
    // fixed-size pixel buffer (256×240 @ 32bpp RGBA).
    // SoftwareRenderTarget provides:
    //   • CPU-side pixel manipulation (draw_pixel, draw_filled_rect, etc.)
    //   • Triangle rasterisation (draw_triangle, flat_top, flat_bottom)
    //   • Clip stack support
    //   • Present callback for frame completion notification
    std::vector<uint8_t> framebuffer_;
    ooey::SoftwareRenderTarget render_target_;
};
```

#### `gooey::mvvmc::*` — UI Controls & Scene Graph

The station shell (bootloader screen, game browser) is built with gooey's
MVVMC widget system:

| gooey Component | Station Usage |
|---|---|
| `GooeyNode` | Scene graph root for the shell UI |
| `IController` | Shell navigation controller |
| `Button` | Game launch buttons in the browser |
| `Label` | Game titles, descriptions |
| `ImageControl` | Game thumbnails, splash screens |
| `Grid` / `FlowLayout` | Game browser grid layout |
| `ScrollContainer` | Scrollable game list |
| `Column` / `Row` | Shell layout structure |

### 5.2 Extensions Needed in OOEY

These features do **not** exist yet in the ooey codebase and must be
contributed upstream or shimmed locally.

#### Extension 1: Framebuffer Render-to-Texture (Blit Path)

**Problem:** The console renders to a 256×240 `SoftwareRenderTarget` (CPU
buffer). We need to efficiently display that buffer scaled up on the host
window's GPU-accelerated `IRenderTarget`.

**What's missing:** `IRenderTarget` has `draw_image()` but `Image` is loaded
from files. There's no API to create an `Image` from a raw pixel buffer that
updates every frame.

**Proposed extension to `ooey::renderer::Image`:**

```cpp
// NEW — ooey/include/ooey/renderer/image.hpp
class Image {
public:
    // Existing: load from file / decoder
    static std::optional<Image> load(const std::string& path);

    // NEW: create from a raw RGBA pixel buffer (non-owning)
    static Image from_raw_rgba(const uint8_t* data, int width, int height);

    // NEW: update pixel data in-place (avoids re-creating the texture)
    void update_pixels(const uint8_t* data, int width, int height);
};
```

**Implementation notes:**
- For `GLRenderTarget`: create a GL texture on first use, then
  `glTexSubImage2D` on each `update_pixels()` call.
- For `SoftwareRenderTarget`: just store the pointer — `draw_image()` already
  does CPU blitting.
- For `VulkanRenderTarget`: staging buffer → `vkCmdCopyBufferToImage`.

#### Extension 2: Audio Output

**Problem:** ooey currently has no audio subsystem whatsoever.

**What's needed:** A minimal mixer interface that the console's PSG/wavetable
synth can push samples into.

**Proposed new module — `ooey/include/ooey/audio.hpp`:**

```cpp
namespace ooey {

struct AudioConfig {
    int sample_rate  = 44100;
    int channels     = 2;       // stereo
    int buffer_size  = 1024;    // frames per callback
};

// Callback-based audio — called from the audio thread
using AudioCallback = std::function<void(float* output, int frame_count)>;

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    virtual bool open(const AudioConfig& config, AudioCallback callback) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

} // namespace ooey
```

**Platform backends needed:**
- Linux: PulseAudio or ALSA (`snd_pcm_writei`)
- Emscripten: Web Audio API (`ScriptProcessorNode` / `AudioWorklet`)
- Android: AAudio / OpenSL ES (already partially supported by NDK)

**Alternatively:** ooey-station can ship its own audio backend locally in
`src/console/audio.cpp` without modifying ooey, using a thin ALSA/PulseAudio
wrapper directly. This avoids blocking on upstream changes.

#### Extension 3: `IWindowBackend` — Vsync / Frame Pacing Control

**Problem:** The console targets a fixed 60 FPS tick rate. The current
`Application::run()` loop doesn't expose frame timing or vsync control.

**What's needed:**

```cpp
// Proposed addition to IWindowBackend or Application
void set_target_frame_rate(int fps);       // 0 = vsync, >0 = fixed
bool is_vsync_enabled() const;
```

**Workaround for now:** Use `set_before_render_callback()` to inject a
frame-rate limiter:

```cpp
auto last_frame = std::chrono::steady_clock::now();
app.set_before_render_callback([&](ooey::IRenderTarget*) {
    constexpr auto target = std::chrono::microseconds(16667); // ~60 FPS
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_frame;
    if (elapsed < target) {
        std::this_thread::sleep_for(target - elapsed);
    }
    last_frame = std::chrono::steady_clock::now();
});
```

### 5.3 Summary Table

| OOEY Component | Header | Used For | Status |
|---|---|---|---|
| `gooey::Application` | `gooey/application.hpp` | Window, main loop, UI thread | ✅ Ready |
| `ooey::InputManager` | `ooey/input.hpp` | Keyboard/mouse → gamepad mapping | ✅ Ready |
| `ooey::IRenderTarget` | `ooey/renderer/i_render_target.hpp` | Shell UI rendering | ✅ Ready |
| `ooey::SoftwareRenderTarget` | `ooey/renderer/software_render_target.hpp` | Console 256×240 framebuffer | ✅ Ready |
| `ooey::ScaledRenderTarget` | `ooey/renderer/scaled_render_target.hpp` | DPI-aware scaling wrapper | ✅ Ready |
| `ooey::Image` | `ooey/renderer/image.hpp` | Game thumbnails, splash art | ✅ Ready |
| `gooey::mvvmc::*` | `gooey/mvvmc/*.hpp` | Shell UI widgets & navigation | ✅ Ready |
| `Image::from_raw_rgba()` | — | Blit console FB → host window | 🔧 Needs extension |
| `ooey::IAudioBackend` | — | PSG / synth audio output | 🔧 Needs new module |
| Frame pacing / vsync | — | 60 FPS console tick | ⚡ Workaround available |

---

## Appendix A: `.gitignore`

```gitignore
# Build directories
build/
build-*/

# IDE
.vscode/
.idea/
*.swp
*~

# Compiled ROMs (generated, not tracked)
*.rom

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
compile_commands.json

# OS
.DS_Store
Thumbs.db
```
