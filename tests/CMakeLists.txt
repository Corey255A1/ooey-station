add_executable(ooey-station-tests
    vm_tests.cpp
    compiler_tests.cpp
    console_tests.cpp
)

target_include_directories(ooey-station-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(ooey-station-tests PRIVATE
    booey-compiler
    ooey
)

add_test(NAME ooey-station-tests COMMAND ooey-station-tests)
