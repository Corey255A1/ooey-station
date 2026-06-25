#pragma once

#include "ast.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <map>

namespace ooey_station::compiler {

class Codegen : public ASTVisitor {
public:
    Codegen() = default;
    ~Codegen() = default;

    std::vector<uint8_t> generate(ProgramNode* program);

    // Visitor methods
    void visit(IntLiteralNode* node) override;
    void visit(FixedLiteralNode* node) override;
    void visit(StringLiteralNode* node) override;
    void visit(VarAccessNode* node) override;
    void visit(BinaryOpNode* node) override;
    void visit(UnaryOpNode* node) override;
    void visit(FuncCallNode* node) override;
    void visit(BlockNode* node) override;
    void visit(VarDeclNode* node) override;
    void visit(IfNode* node) override;
    void visit(WhileNode* node) override;
    void visit(ForNode* node) override;
    void visit(ReturnNode* node) override;
    void visit(ExprStatementNode* node) override;
    void visit(FunctionNode* node) override;
    void visit(ProgramNode* node) override;

private:
    std::vector<uint8_t> code_buf_;
    std::vector<uint8_t> data_buf_;
    std::vector<uint8_t> asset_buf_;

    // Symbol tables
    std::map<std::string, uint32_t> global_vars_; // Map of variable name to Game RAM address offset
    std::map<std::string, uint32_t> local_vars_;  // Map of local variable name to register index or stack offset
    std::map<std::string, uint32_t> function_pcs_; // Map of function name to code buffer index

    // Asset indices
    std::map<std::string, uint32_t> color_palette_; // name -> index
    std::map<std::string, uint32_t> sprite_ids_;    // name -> index
    std::map<std::string, uint32_t> tile_ids_;      // name -> index

    // Helper functions to emit instructions
    void emit_8(uint8_t val);
    void emit_32(uint32_t val);
    void patch_32(size_t index, uint32_t val);

    // Keep track of the active register for expression output
    int next_free_register_{1}; 
    int allocate_register();
    void free_register(int reg);

    // Fixup structure for forward jumps
    struct JumpFixup {
        size_t index;
        std::string label;
    };
    std::vector<JumpFixup> jump_fixups_;

    void resolve_jumps();
};

} // namespace ooey_station::compiler
