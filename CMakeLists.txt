cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(ooey-station VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OOEY_STATION_ASAN   "Enable AddressSanitizer (debug builds)" ON)
option(OOEY_STATION_TESTS  "Build the test suite"                   ON)

if(OOEY_STATION_ASAN AND NOT EMSCRIPTEN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

set(OOEY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ooey"
    CACHE PATH "Path to the ooey source tree")

if(NOT EXISTS "${OOEY_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "Cannot find ooey at ${OOEY_ROOT}.\n"
        "Clone the ooey repo as a sibling directory, or set -DOOEY_ROOT=<path>.")
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory("${OOEY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/_ooey")

# Collect ooey-station source files

set(CONSOLE_SRCS
    src/console/display.cpp
    src/console/input.cpp
    src/console/audio.cpp
    src/console/memory.cpp
)

set(VM_SRCS
    src/vm/vm.cpp
    src/vm/debug.cpp
)

set(STATION_SRCS
    src/station/bootloader.cpp
    src/station/browser.cpp
)

set(ASSET_SRCS
    src/assets/sprite_gen.cpp
    src/assets/tilemap_gen.cpp
    src/assets/sound_gen.cpp
)

set(COMPILER_LIB_SRCS
    src/compiler/lexer.cpp
    src/compiler/parser.cpp
    src/compiler/codegen.cpp
)

add_library(booey-compiler STATIC ${COMPILER_LIB_SRCS})
target_include_directories(booey-compiler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(booey-compiler PUBLIC ooey)

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
    gooey
    booey-compiler
)

target_compile_definitions(ooey-station PRIVATE
    OOEY_STATION_GAMES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/games"
)

if(EMSCRIPTEN)
    set_target_properties(ooey-station PROPERTIES SUFFIX ".html")
    target_link_options(ooey-station PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sLEGACY_GL_EMULATION=1
        --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/games@/games"
    )
endif()

add_executable(booeyc src/compiler/main.cpp)
target_include_directories(booeyc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(booeyc PRIVATE booey-compiler)

if(OOEY_STATION_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ooey-station PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
    target_compile_options(booeyc PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

if(MSVC)
    target_compile_options(ooey-station PRIVATE /W4 /permissive-)
    target_compile_options(booeyc PRIVATE /W4 /permissive-)
endif()
