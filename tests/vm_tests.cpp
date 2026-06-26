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
        16, 0, 0, 0,         // Code size: 16 bytes
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

#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"
#include <fstream>
#include <sstream>

void test_hello_world_execution() {
    std::cout << "Running test_hello_world_execution..." << std::endl;
    std::ifstream in("games/hello-world/main.booey");
    assert(in.is_open());
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();
    in.close();

    using namespace ooey_station::compiler;
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    Codegen codegen;
    auto binary = codegen.generate(ast.get());

    BooeyVM vm;
    bool loaded = vm.load_program(binary);
    assert(loaded);

    std::cout << "Compiled bytecode (size " << binary.size() << "):" << std::endl;
    for (size_t i = 0; i < binary.size(); ++i) {
        printf("%02x ", binary[i]);
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;

    // Run first frame (executes bootstrap which calls init() and then update() once before hitting VBLANK)
    vm.run_frame();
    
    // x is at RAM address 0, y is at RAM address 4
    uint32_t x = vm.read_memory_32(0);
    uint32_t y = vm.read_memory_32(4);
    std::cout << "After init, x=" << x << ", y=" << y << std::endl;
    assert(x == 300);
    assert(y == 200);

    // Now hold LEFT (bit 2 of 0x1C000)
    vm.write_memory_32(0x1C000, 1 << 2); // LEFT is bit 2 (ButtonId::Left)
    
    // Run second frame (should run update() again)
    vm.run_frame();

    x = vm.read_memory_32(0);
    y = vm.read_memory_32(4);
    std::cout << "After holding LEFT, x=" << x << ", y=" << y << std::endl;
    assert(x == 296); // Should decrease by 4

    // Now hold RIGHT (bit 3)
    vm.write_memory_32(0x1C000, 1 << 3); // RIGHT is bit 3 (ButtonId::Right)
    vm.run_frame();

    x = vm.read_memory_32(0);
    y = vm.read_memory_32(4);
    std::cout << "After holding RIGHT, x=" << x << ", y=" << y << std::endl;
    assert(x == 300); // Should increase back to 300
    
    std::cout << "test_hello_world_execution passed!" << std::endl;
}

int main() {
    std::cout << "Starting Ooey-Station Test Suite..." << std::endl;
    
    test_basic_arithmetic();
    test_hello_world_execution();
    run_compiler_tests();
    run_console_tests();
    
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
