#pragma once

#include "lexer.hpp"
#include "ast.hpp"
#include <vector>
#include <memory>
#include <string>

namespace ooey_station::compiler {

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ProgramNode> parse();

private:
    std::vector<Token> tokens_;
    size_t cursor_{0};

    // Helper utilities
    bool is_at_end() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    const Token& consume(TokenType type, const std::string& error_msg);
    void synchronize();

    // Declarative block parsing
    void parse_game_block(ProgramNode& program);
    void parse_palette_block(ProgramNode& program);
    void parse_sprites_block(ProgramNode& program);
    void parse_tiles_block(ProgramNode& program);
    void parse_vars_block(ProgramNode& program);

    // Procedural parsing
    std::unique_ptr<FunctionNode> parse_function();
    std::unique_ptr<BlockNode> parse_block();
    std::unique_ptr<StatementNode> parse_statement();
    std::unique_ptr<StatementNode> parse_if_statement();
    std::unique_ptr<StatementNode> parse_while_statement();
    std::unique_ptr<StatementNode> parse_for_statement();
    std::unique_ptr<StatementNode> parse_return_statement();
    std::unique_ptr<StatementNode> parse_let_statement();

    // Expression parsing (precendence climbing/pratt parser style)
    std::unique_ptr<ExpressionNode> parse_expression();
    std::unique_ptr<ExpressionNode> parse_logical_or();
    std::unique_ptr<ExpressionNode> parse_logical_and();
    std::unique_ptr<ExpressionNode> parse_equality();
    std::unique_ptr<ExpressionNode> parse_comparison();
    std::unique_ptr<ExpressionNode> parse_term();
    std::unique_ptr<ExpressionNode> parse_factor();
    std::unique_ptr<ExpressionNode> parse_unary();
    std::unique_ptr<ExpressionNode> parse_primary();

    DataType parse_data_type(const std::string& type_str);
    void parse_dimensions(int& w, int& h);
};

} // namespace ooey_station::compiler
