#include <iostream>
#include <cassert>
#include "console/display.hpp"
#include "console/input.hpp"

using namespace ooey_station::console;

void test_console_display() {
    std::cout << "Running test_console_display..." << std::endl;
    
    ConsoleDisplay display;
    display.set_palette_color(1, 255, 0, 0); // Red at index 1
    
    // Draw a pixel
    display.set_pixel(100, 100, 1);
    assert(display.get_pixel(100, 100) == 1);
    
    // Draw an out of bounds pixel (should do nothing/be ignored safely)
    display.set_pixel(1000, 1000, 1);
    
    std::cout << "test_console_display passed!" << std::endl;
}

void test_console_input() {
    std::cout << "Running test_console_input..." << std::endl;
    
    InputController input;
    // Input is updated via ooey::InputManager, we can test state query defaults
    assert(input.get_held_mask() == 0);
    assert(input.get_pressed_mask() == 0);
    assert(input.get_released_mask() == 0);
    
    std::cout << "test_console_input passed!" << std::endl;
}

void run_console_tests() {
    test_console_display();
    test_console_input();
}
