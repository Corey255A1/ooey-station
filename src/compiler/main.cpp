#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace ooey_station::compiler;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: booeyc <input.booey> -o <output.bin>" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = "output.booey.bin";

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) {
            output_path = argv[i + 1];
            i++;
        }
    }

    // Read source file
    std::ifstream in(input_path);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open input file: " << input_path << std::endl;
        return 1;
    }

    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();
    in.close();

    try {
        // 1. Lexical analysis
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        // 2. Parser
        Parser parser(tokens);
        auto ast = parser.parse();

        // 3. Codegen
        Codegen codegen;
        auto binary = codegen.generate(ast.get());

        // Write output
        std::ofstream out(output_path, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "Error: Could not open output file: " << output_path << std::endl;
            return 1;
        }

        out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        out.close();

        std::cout << "Successfully compiled " << input_path << " to " << output_path 
                  << " (" << binary.size() << " bytes)" << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Compilation failed: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
