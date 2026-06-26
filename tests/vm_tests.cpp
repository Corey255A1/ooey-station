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

void test_new_features() {
    std::cout << "Running test_new_features..." << std::endl;
    
    std::string source = 
        "game:\n"
        "    title: \"Test New Features\"\n"
        "palette:\n"
        "    white: rgb(255, 255, 255)\n"
        "sprites:\n"
        "    smiley 8x8:\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "tiles:\n"
        "    brick 8x8:\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "        . . . . . . . .\n"
        "vars:\n"
        "    res_abs_neg: int = 0\n"
        "    res_abs_pos: int = 0\n"
        "    res_min: int = 0\n"
        "    res_max: int = 0\n"
        "    res_clamp_lo: int = 0\n"
        "    res_clamp_hi: int = 0\n"
        "    res_clamp_mid: int = 0\n"
        "    res_mod: int = 0\n"
        "    res_col_true: int = 0\n"
        "    res_col_false: int = 0\n"
        "    res_sin: int = 0\n"
        "    res_cos: int = 0\n"
        "    res_atan2: int = 0\n"
        "    res_dist: int = 0\n"
        "fn init():\n"
        "    res_abs_neg = abs(-42)\n"
        "    res_abs_pos = abs(42)\n"
        "    res_min = min(10, 20)\n"
        "    res_max = max(10, 20)\n"
        "    res_clamp_lo = clamp(5, 10, 20)\n"
        "    res_clamp_hi = clamp(25, 10, 20)\n"
        "    res_clamp_mid = clamp(15, 10, 20)\n"
        "    res_mod = 17 % 5\n"
        "    res_col_true = check_collision(smiley, 0, 0, smiley, 4, 4)\n"
        "    res_col_false = check_collision(smiley, 0, 0, smiley, 20, 20)\n"
        "    res_sin = sin(0)\n"
        "    res_cos = cos(0)\n"
        "    res_atan2 = atan2(65536, 65536)\n"
        "    res_dist = dist(0, 0, 3, 4)\n"
        "    draw_int(10, 10, 12345, white)\n"
        "    tile(0, 5, 6, brick)\n"
        "    tscroll(0, 100, 200)\n"
        "    tdraw(0)\n"
        "fn update():\n"
        "    exit()\n";
        
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
    
    vm.run_frame();
    
    // Check results in RAM
    assert(vm.read_memory_32(0) == 42);   // res_abs_neg
    assert(vm.read_memory_32(4) == 42);   // res_abs_pos
    assert(vm.read_memory_32(8) == 10);   // res_min
    assert(vm.read_memory_32(12) == 20);  // res_max
    assert(vm.read_memory_32(16) == 10);  // res_clamp_lo
    assert(vm.read_memory_32(20) == 20);  // res_clamp_hi
    assert(vm.read_memory_32(24) == 15);  // res_clamp_mid
    assert(vm.read_memory_32(28) == 2);   // res_mod
    assert(vm.read_memory_32(32) == 1);   // res_col_true
    assert(vm.read_memory_32(36) == 0);   // res_col_false
    assert(vm.read_memory_32(40) == 0);   // res_sin
    assert(vm.read_memory_32(44) == 65536); // res_cos
    
    uint32_t got_atan2 = vm.read_memory_32(48);
    std::cout << "got_atan2: " << got_atan2 << " (expected approx 51471)" << std::endl;
    assert(got_atan2 > 51400 && got_atan2 < 51550);
    
    assert(vm.read_memory_32(52) == 5);   // res_dist
    
    // Check formatted string in RAM at 0xFC00
    assert(vm.read_memory_8(0xFC00) == '1');
    assert(vm.read_memory_8(0xFC01) == '2');
    assert(vm.read_memory_8(0xFC02) == '3');
    assert(vm.read_memory_8(0xFC03) == '4');
    assert(vm.read_memory_8(0xFC04) == '5');
    assert(vm.read_memory_8(0xFC05) == '\0');
    
    std::cout << "test_new_features passed!" << std::endl;
}

void test_sonic_game_execution() {
    std::cout << "Running test_sonic_game_execution..." << std::endl;
    
    std::ifstream in("games/sonic-style/main.booey");
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
    
    std::map<std::string, uint32_t> var_offsets;
    uint32_t offset = 0;
    for (const auto& var : ast->global_vars) {
        var_offsets[var->name] = offset;
        offset += 4;
    }
    
    assert(var_offsets.find("game_over") != var_offsets.end());
    uint32_t game_over_addr = var_offsets["game_over"];
    
    // Run 100 frames to make sure it executes physics, drawing, bounds, etc.
    for (int i = 0; i < 100; ++i) {
        vm.run_frame();
        assert(!vm.is_halted());
        // Assert game over is NOT triggered
        assert(vm.read_memory_32(game_over_addr) == 0);
    }
    
    std::cout << "test_sonic_game_execution passed!" << std::endl;
}

int main() {
    std::cout << "Starting Ooey-Station Test Suite..." << std::endl;
    
    test_basic_arithmetic();
    test_hello_world_execution();
    test_new_features();
    test_sonic_game_execution();
    run_compiler_tests();
    run_console_tests();
    
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
