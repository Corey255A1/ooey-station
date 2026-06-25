#include <iostream>
#include <cassert>
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"

using namespace ooey_station::compiler;

void test_lexer() {
    std::cout << "Running test_lexer..." << std::endl;
    
    std::string source = 
        "game:\n"
        "    title: \"Test Game\"\n"
        "fn init():\n"
        "    let x: int = 10\n"
        "    if x == 10:\n"
        "        x = 20\n";
        
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    
    assert(!tokens.empty());
    assert(tokens[0].type == TokenType::KwGame);
    assert(tokens[1].type == TokenType::Colon);
    assert(tokens[2].type == TokenType::Newline);
    assert(tokens[3].type == TokenType::Indent);
    
    std::cout << "test_lexer passed!" << std::endl;
}

void test_parser_and_codegen() {
    std::cout << "Running test_parser_and_codegen..." << std::endl;
    
    std::string source = 
        "game:\n"
        "    title: \"Test Game\"\n"
        "palette:\n"
        "    black: rgb(0, 0, 0)\n"
        "vars:\n"
        "    score: int = 5\n"
        "fn init():\n"
        "    score = 10\n"
        "fn update():\n"
        "    exit()\n";
        
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    
    Parser parser(tokens);
    auto ast = parser.parse();
    
    assert(ast != nullptr);
    assert(ast->metadata["title"] == "Test Game");
    assert(ast->functions.size() == 2);
    
    Codegen codegen;
    auto binary = codegen.generate(ast.get());
    assert(!binary.empty());
    
    std::cout << "test_parser_and_codegen passed!" << std::endl;
}

#include <fstream>
#include <sstream>

void test_hello_world() {
    std::cout << "Running test_hello_world..." << std::endl;
    std::ifstream in("games/hello-world/main.booey");
    assert(in.is_open());
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();
    in.close();

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        std::cout << "Tokens count: " << tokens.size() << std::endl;
        Parser parser(tokens);
        auto ast = parser.parse();
        assert(ast != nullptr);
        Codegen codegen;
        auto binary = codegen.generate(ast.get());
        assert(!binary.empty());
        std::cout << "test_hello_world passed! Compiled size: " << binary.size() << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "test_hello_world FAILED with exception: " << ex.what() << std::endl;
        throw;
    }
}

void run_compiler_tests() {
    test_lexer();
    test_parser_and_codegen();
    test_hello_world();
}

