#include <iostream>
#include <cassert>
#include "vm/vm.hpp"
#include "vm/opcodes.hpp"

using namespace ooey_station::vm;

void run_compiler_tests();
void run_console_tests();

void test_basic_arithmetic() {
    std::cout << "Running test_basic_arithmetic..." << std::endl;
    
    // We construct a simple program:
    // MOVI R1, 10
    // MOVI R2, 20
    // ADD R1, R2
    // HALT
    std::vector<uint8_t> program = {
        'B', 'O', 'O', 'E', // Magic
        0x01, 0x00,          // Version
        0x00, 0x00,          // Flags
        18, 0, 0, 0,         // Code size: 18 bytes
        0, 0, 0, 0,          // Data size
        0, 0, 0, 0,          // Asset size
        0, 0, 0, 0,          // Entry point
        0, 0, 0, 0,          // Checksum (placeholder)
        0, 0, 0, 0,          // Reserved
        
        // Code
        OP_MOVI, 1, 10, 0, 0, 0, // MOVI R1, 10
        OP_MOVI, 2, 20, 0, 0, 0, // MOVI R2, 20
        OP_ADD, 1, 2,            // ADD R1, R2
        OP_HALT                  // HALT
    };
    
    BooeyVM vm;
    bool loaded = vm.load_program(program);
    assert(loaded);
    
    vm.run_frame();
    assert(vm.is_halted());
    assert(vm.get_register(1) == 30);
    
    std::cout << "test_basic_arithmetic passed!" << std::endl;
}

int main() {
    std::cout << "Starting Ooey-Station Test Suite..." << std::endl;
    
    test_basic_arithmetic();
    run_compiler_tests();
    run_console_tests();
    
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
