add_executable(ooey-station-tests
    vm_tests.cpp
    compiler_tests.cpp
    console_tests.cpp
    ../src/console/display.cpp
    ../src/console/input.cpp
    ../src/console/audio.cpp
    ../src/console/memory.cpp
    ../src/vm/vm.cpp
    ../src/vm/debug.cpp
)

target_include_directories(ooey-station-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(ooey-station-tests PRIVATE
    booey-compiler
    ooey
)

add_test(NAME ooey-station-tests COMMAND ooey-station-tests)
