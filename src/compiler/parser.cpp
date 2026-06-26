#include "parser.hpp"
#include <stdexcept>
#include <iostream>
#include <sstream>

namespace ooey_station::compiler {

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

bool Parser::is_at_end() const {
    return peek().type == TokenType::Eof;
}

const Token& Parser::peek() const {
    return tokens_[cursor_];
}

const Token& Parser::previous() const {
    return tokens_[cursor_ - 1];
}

const Token& Parser::advance() {
    if (!is_at_end()) cursor_++;
    return previous();
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

const Token& Parser::consume(TokenType type, const std::string& error_msg) {
    if (check(type)) return advance();
    throw std::runtime_error("Parser Error on line " + std::to_string(peek().line) + ": " + error_msg + " (got '" + peek().value + "')");
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TokenType::Newline) return;
        switch (peek().type) {
            case TokenType::KwGame:
            case TokenType::KwPalette:
            case TokenType::KwSprites:
            case TokenType::KwVars:
            case TokenType::KwFn:
            case TokenType::KwIf:
            case TokenType::KwWhile:
            case TokenType::KwFor:
            case TokenType::KwReturn:
                return;
            default:
                break;
        }
        advance();
    }
}

DataType Parser::parse_data_type(const std::string& type_str) {
    if (type_str == "int") return DataType::Int;
    if (type_str == "fixed") return DataType::Fixed;
    if (type_str == "bool") return DataType::Bool;
    if (type_str == "string") return DataType::String;
    if (type_str == "color") return DataType::Color;
    if (type_str == "sprite_id") return DataType::SpriteId;
    if (type_str == "tile_id") return DataType::TileId;
    if (type_str == "sound_id") return DataType::SoundId;
    return DataType::Unknown;
}

void Parser::parse_dimensions(int& w, int& h) {
    if (check(TokenType::IntLiteral)) {
        Token width_tok = consume(TokenType::IntLiteral, "Expected width");
        w = std::stoi(width_tok.value);
        
        Token x_tok = consume(TokenType::Identifier, "Expected 'x' or 'x<height>'");
        if (x_tok.value == "x") {
            Token height_tok = consume(TokenType::IntLiteral, "Expected height");
            h = std::stoi(height_tok.value);
        } else if (x_tok.value[0] == 'x') {
            h = std::stoi(x_tok.value.substr(1));
        } else {
            throw std::runtime_error("Parser Error: Invalid dimension format. Expected <width>x<height>");
        }
    } else {
        Token size_tok = consume(TokenType::Identifier, "Expected dimensions (e.g. 16x16)");
        if (std::sscanf(size_tok.value.c_str(), "%dx%d", &w, &h) != 2) {
            throw std::runtime_error("Parser Error: Invalid size format: " + size_tok.value);
        }
    }
}


std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    
    while (!is_at_end()) {
        // Skip leading newlines at file scope
        if (match(TokenType::Newline)) continue;

        if (match(TokenType::KwGame)) {
            parse_game_block(*program);
        } else if (match(TokenType::KwPalette)) {
            parse_palette_block(*program);
        } else if (match(TokenType::KwSprites)) {
            parse_sprites_block(*program);
        } else if (match(TokenType::KwTiles)) {
            parse_tiles_block(*program);
        } else if (match(TokenType::KwVars)) {
            parse_vars_block(*program);
        } else if (match(TokenType::KwFn)) {
            program->functions.push_back(parse_function());
        } else {
            throw std::runtime_error("Parser Error: Unexpected top-level token on line " + std::to_string(peek().line) + ": " + peek().value);
        }
    }
    
    return program;
}

void Parser::parse_game_block(ProgramNode& program) {
    consume(TokenType::Colon, "Expected ':' after 'game'");
    consume(TokenType::Newline, "Expected newline after 'game:'");
    consume(TokenType::Indent, "Expected indented block for 'game'");

    while (!check(TokenType::Dedent) && !is_at_end()) {
        if (match(TokenType::Newline)) continue;
        
        std::string key = consume(TokenType::Identifier, "Expected metadata key").value;
        consume(TokenType::Colon, "Expected ':' after key");
        
        std::string val;
        if (match(TokenType::StringLiteral)) {
            val = previous().value;
        } else if (match(TokenType::IntLiteral)) {
            val = previous().value;
        } else {
            throw std::runtime_error("Parser Error: Expected string or integer value for metadata");
        }
        
        program.metadata[key] = val;
        consume(TokenType::Newline, "Expected newline after metadata entry");
    }
    
    consume(TokenType::Dedent, "Expected dedent at end of 'game' block");
}

void Parser::parse_palette_block(ProgramNode& program) {
    consume(TokenType::Colon, "Expected ':' after 'palette'");
    consume(TokenType::Newline, "Expected newline after 'palette:'");
    consume(TokenType::Indent, "Expected indented block for 'palette'");

    while (!check(TokenType::Dedent) && !is_at_end()) {
        if (match(TokenType::Newline)) continue;
        
        std::string name = consume(TokenType::Identifier, "Expected color name").value;
        consume(TokenType::Colon, "Expected ':' after color name");
        
        // Expecting rgb(r, g, b)
        consume(TokenType::Identifier, "Expected 'rgb' color constructor");
        if (previous().value != "rgb") {
            throw std::runtime_error("Parser Error: Colors must be defined using 'rgb(r,g,b)'");
        }
        consume(TokenType::LeftParen, "Expected '(' after 'rgb'");
        int r = std::stoi(consume(TokenType::IntLiteral, "Expected red component").value);
        consume(TokenType::Comma, "Expected ','");
        int g = std::stoi(consume(TokenType::IntLiteral, "Expected green component").value);
        consume(TokenType::Comma, "Expected ','");
        int b = std::stoi(consume(TokenType::IntLiteral, "Expected blue component").value);
        consume(TokenType::RightParen, "Expected ')'");
        
        program.palette[name] = ooey::Color(r, g, b);
        consume(TokenType::Newline, "Expected newline after palette entry");
    }
    
    consume(TokenType::Dedent, "Expected dedent at end of 'palette' block");
}

void Parser::parse_sprites_block(ProgramNode& program) {
    consume(TokenType::Colon, "Expected ':' after 'sprites'");
    consume(TokenType::Newline, "Expected newline after 'sprites:'");
    consume(TokenType::Indent, "Expected indented block for 'sprites'");

    while (!check(TokenType::Dedent) && !is_at_end()) {
        if (match(TokenType::Newline)) continue;
        
        std::string name = consume(TokenType::Identifier, "Expected sprite name").value;
        int w = 0, h = 0;
        parse_dimensions(w, h);
        
        consume(TokenType::Colon, "Expected ':' after sprite header");
        consume(TokenType::Newline, "Expected newline after sprite header");
        consume(TokenType::Indent, "Expected indented grid for sprite pixel art");
        
        SpriteAsset sprite{name, w, h, {}};
        
        for (int i = 0; i < h; ++i) {
            std::string row_str;
            while (!check(TokenType::Newline) && !is_at_end()) {
                Token t = advance();
                if (t.type == TokenType::Dot) {
                    row_str += '.';
                } else if (t.type == TokenType::Identifier) {
                    row_str += t.value;
                } else {
                    throw std::runtime_error("Parser Error: Unexpected sprite pixel token '" + t.value + "'");
                }
            }
            sprite.grid.push_back(row_str);
            consume(TokenType::Newline, "Expected newline after sprite grid row");
        }
        
        program.sprites.push_back(sprite);
        consume(TokenType::Dedent, "Expected dedent after sprite grid");
        if (match(TokenType::Newline)) {} // optional newline
    }
    
    consume(TokenType::Dedent, "Expected dedent at end of 'sprites' block");
}

void Parser::parse_tiles_block(ProgramNode& program) {
    consume(TokenType::Colon, "Expected ':' after 'tiles'");
    consume(TokenType::Newline, "Expected newline after 'tiles:'");
    consume(TokenType::Indent, "Expected indented block for 'tiles'");

    while (!check(TokenType::Dedent) && !is_at_end()) {
        if (match(TokenType::Newline)) continue;
        
        std::string name = consume(TokenType::Identifier, "Expected tile name").value;
        int w = 0, h = 0;
        parse_dimensions(w, h);
        
        consume(TokenType::Colon, "Expected ':' after tile header");
        consume(TokenType::Newline, "Expected newline after tile header");
        consume(TokenType::Indent, "Expected indented grid for tile pixel art");
        
        TileAsset tile{name, w, h, {}};
        
        for (int i = 0; i < h; ++i) {
            std::string row_str;
            while (!check(TokenType::Newline) && !is_at_end()) {
                Token t = advance();
                if (t.type == TokenType::Dot) {
                    row_str += '.';
                } else if (t.type == TokenType::Identifier) {
                    row_str += t.value;
                } else {
                    throw std::runtime_error("Parser Error: Unexpected tile pixel token '" + t.value + "'");
                }
            }
            tile.grid.push_back(row_str);
            consume(TokenType::Newline, "Expected newline after tile grid row");
        }
        
        program.tiles.push_back(tile);
        consume(TokenType::Dedent, "Expected dedent after tile grid");
        if (match(TokenType::Newline)) {}
    }
    
    consume(TokenType::Dedent, "Expected dedent at end of 'tiles' block");
}

void Parser::parse_vars_block(ProgramNode& program) {
    consume(TokenType::Colon, "Expected ':' after 'vars'");
    consume(TokenType::Newline, "Expected newline after 'vars:'");
    consume(TokenType::Indent, "Expected indented block for 'vars'");

    while (!check(TokenType::Dedent) && !is_at_end()) {
        if (match(TokenType::Newline)) continue;
        
        std::string name = consume(TokenType::Identifier, "Expected variable name").value;
        consume(TokenType::Colon, "Expected ':' after variable name");
        std::string type_str = consume(TokenType::Identifier, "Expected type name").value;
        
        DataType type = parse_data_type(type_str);
        if (type == DataType::Unknown) {
            throw std::runtime_error("Parser Error: Unknown data type '" + type_str + "'");
        }
        
        std::unique_ptr<ExpressionNode> init = nullptr;
        if (match(TokenType::Equals)) {
            init = parse_expression();
        }
        
        program.global_vars.push_back(std::make_unique<VarDeclNode>(name, type, std::move(init)));
        consume(TokenType::Newline, "Expected newline after variable declaration");
    }
    
    consume(TokenType::Dedent, "Expected dedent at end of 'vars' block");
}

std::unique_ptr<FunctionNode> Parser::parse_function() {
    std::string name = consume(TokenType::Identifier, "Expected function name").value;
    consume(TokenType::LeftParen, "Expected '(' after function name");
    
    std::vector<FunctionNode::Param> params;
    if (!check(TokenType::RightParen)) {
        do {
            std::string param_name = consume(TokenType::Identifier, "Expected parameter name").value;
            consume(TokenType::Colon, "Expected ':' after parameter name");
            std::string type_str = consume(TokenType::Identifier, "Expected parameter type").value;
            params.push_back({param_name, parse_data_type(type_str)});
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightParen, "Expected ')' after parameters");
    
    DataType return_type = DataType::Void;
    if (match(TokenType::Minus)) {
        consume(TokenType::GreaterThan, "Expected '>' after '-' for return arrow '->'");
        std::string type_str = consume(TokenType::Identifier, "Expected return type").value;
        return_type = parse_data_type(type_str);
    }
    
    consume(TokenType::Colon, "Expected ':' before function body");
    consume(TokenType::Newline, "Expected newline before function body");
    
    auto body = parse_block();
    
    return std::make_unique<FunctionNode>(name, params, return_type, std::move(body));
}

std::unique_ptr<BlockNode> Parser::parse_block() {
    consume(TokenType::Indent, "Expected indent for block");
    auto block = std::make_unique<BlockNode>();
    
    while (!check(TokenType::Dedent) && !is_at_end()) {
        if (match(TokenType::Newline)) continue;
        block->statements.push_back(parse_statement());
    }
    
    consume(TokenType::Dedent, "Expected dedent at end of block");
    return block;
}

std::unique_ptr<StatementNode> Parser::parse_statement() {
    std::unique_ptr<StatementNode> stmt;
    if (match(TokenType::KwIf)) {
        stmt = parse_if_statement();
    } else if (match(TokenType::KwWhile)) {
        stmt = parse_while_statement();
    } else if (match(TokenType::KwFor)) {
        stmt = parse_for_statement();
    } else if (match(TokenType::KwReturn)) {
        stmt = parse_return_statement();
    } else if (match(TokenType::KwLet)) {
        stmt = parse_let_statement();
    } else if (check(TokenType::Identifier) && cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].type == TokenType::Equals) {
        std::string name = consume(TokenType::Identifier, "Expected variable name").value;
        consume(TokenType::Equals, "Expected '='");
        auto expr = parse_expression();
        stmt = std::make_unique<VarDeclNode>(name, DataType::Unknown, std::move(expr));
        consume(TokenType::Newline, "Expected newline after assignment");
    } else {
        auto expr = parse_expression();
        stmt = std::make_unique<ExprStatementNode>(std::move(expr));
        consume(TokenType::Newline, "Expected newline after statement");
    }
    return stmt;
}

std::unique_ptr<StatementNode> Parser::parse_if_statement() {
    auto condition = parse_expression();
    consume(TokenType::Colon, "Expected ':' after if condition");
    consume(TokenType::Newline, "Expected newline after ':'");
    
    auto true_branch = parse_block();
    std::unique_ptr<BlockNode> false_branch = nullptr;
    
    // Check for elif/else
    if (match(TokenType::Newline)) {}
    if (match(TokenType::KwElse)) {
        consume(TokenType::Colon, "Expected ':' after else");
        consume(TokenType::Newline, "Expected newline after else:");
        false_branch = parse_block();
    }
    
    return std::make_unique<IfNode>(std::move(condition), std::move(true_branch), std::move(false_branch));
}

std::unique_ptr<StatementNode> Parser::parse_while_statement() {
    auto condition = parse_expression();
    consume(TokenType::Colon, "Expected ':' after while condition");
    consume(TokenType::Newline, "Expected newline after while condition");
    auto body = parse_block();
    return std::make_unique<WhileNode>(std::move(condition), std::move(body));
}

std::unique_ptr<StatementNode> Parser::parse_for_statement() {
    std::string var_name = consume(TokenType::Identifier, "Expected loop variable name").value;
    consume(TokenType::KwIn, "Expected 'in' in for loop");
    
    consume(TokenType::Identifier, "Expected 'range' in for loop");
    if (previous().value != "range") {
        throw std::runtime_error("Parser Error: Loops only support 'range(end)' or 'range(start, end)'");
    }
    
    consume(TokenType::LeftParen, "Expected '(' after range");
    auto start_expr = parse_expression();
    std::unique_ptr<ExpressionNode> end_expr = nullptr;
    
    if (match(TokenType::Comma)) {
        end_expr = parse_expression();
    } else {
        // range(end) -> start is 0
        end_expr = std::move(start_expr);
        start_expr = std::make_unique<IntLiteralNode>(0);
    }
    consume(TokenType::RightParen, "Expected ')' after range arguments");
    consume(TokenType::Colon, "Expected ':' after range");
    consume(TokenType::Newline, "Expected newline after range");
    
    auto body = parse_block();
    return std::make_unique<ForNode>(var_name, std::move(start_expr), std::move(end_expr), std::move(body));
}

std::unique_ptr<StatementNode> Parser::parse_return_statement() {
    std::unique_ptr<ExpressionNode> val = nullptr;
    if (!check(TokenType::Newline)) {
        val = parse_expression();
    }
    consume(TokenType::Newline, "Expected newline after return statement");
    return std::make_unique<ReturnNode>(std::move(val));
}

std::unique_ptr<StatementNode> Parser::parse_let_statement() {
    std::string name = consume(TokenType::Identifier, "Expected variable name").value;
    consume(TokenType::Colon, "Expected ':' after variable name");
    std::string type_str = consume(TokenType::Identifier, "Expected variable type").value;
    DataType type = parse_data_type(type_str);
    
    std::unique_ptr<ExpressionNode> init = nullptr;
    if (match(TokenType::Equals)) {
        init = parse_expression();
    }
    consume(TokenType::Newline, "Expected newline after let declaration");
    return std::make_unique<VarDeclNode>(name, type, std::move(init));
}

// Expressions

std::unique_ptr<ExpressionNode> Parser::parse_expression() {
    return parse_logical_or();
}

std::unique_ptr<ExpressionNode> Parser::parse_logical_or() {
    auto expr = parse_logical_and();
    // Simplified: logical operators or binary operators
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_logical_and() {
    return parse_equality();
}

std::unique_ptr<ExpressionNode> Parser::parse_equality() {
    auto expr = parse_comparison();
    while (match(TokenType::EqualsEquals) || match(TokenType::NotEquals)) {
        std::string op = previous().value;
        auto right = parse_comparison();
        expr = std::make_unique<BinaryOpNode>(op, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_comparison() {
    auto expr = parse_term();
    while (match(TokenType::LessThan) || match(TokenType::GreaterThan) ||
           match(TokenType::LessEqual) || match(TokenType::GreaterEqual)) {
        std::string op = previous().value;
        auto right = parse_term();
        expr = std::make_unique<BinaryOpNode>(op, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_term() {
    auto expr = parse_factor();
    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        std::string op = previous().value;
        auto right = parse_factor();
        expr = std::make_unique<BinaryOpNode>(op, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_factor() {
    auto expr = parse_unary();
    while (match(TokenType::Star) || match(TokenType::Slash) || match(TokenType::Percent)) {
        std::string op = previous().value;
        auto right = parse_unary();
        expr = std::make_unique<BinaryOpNode>(op, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_unary() {
    if (match(TokenType::Minus)) {
        std::string op = previous().value;
        auto operand = parse_unary();
        return std::make_unique<UnaryOpNode>(op, std::move(operand));
    }
    return parse_primary();
}

std::unique_ptr<ExpressionNode> Parser::parse_primary() {
    if (match(TokenType::IntLiteral)) {
        return std::make_unique<IntLiteralNode>(std::stoi(previous().value));
    }
    if (match(TokenType::FixedLiteral)) {
        return std::make_unique<FixedLiteralNode>(std::stof(previous().value));
    }
    if (match(TokenType::StringLiteral)) {
        return std::make_unique<StringLiteralNode>(previous().value);
    }
    
    if (match(TokenType::Identifier)) {
        std::string name = previous().value;
        
        // Check for function call
        if (match(TokenType::LeftParen)) {
            std::vector<std::unique_ptr<ExpressionNode>> args;
            if (!check(TokenType::RightParen)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RightParen, "Expected ')' after function arguments");
            return std::make_unique<FuncCallNode>(name, std::move(args));
        }
        
        return std::make_unique<VarAccessNode>(name);
    }
    
    if (match(TokenType::LeftParen)) {
        auto expr = parse_expression();
        consume(TokenType::RightParen, "Expected ')' after expression");
        return expr;
    }
    
    throw std::runtime_error("Parser Error: Expected expression on line " + std::to_string(peek().line) + " (got '" + peek().value + "')");
}

} // namespace ooey_station::compiler
