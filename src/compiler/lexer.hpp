#pragma once

#include <string>
#include <vector>
#include <ostream>

namespace ooey_station::compiler {

enum class TokenType {
    // Structural
    Indent, Dedent, Newline, Eof,
    
    // Keywords
    KwGame, KwPalette, KwSprites, KwTiles, KwVars, KwFn, 
    KwIf, KwElif, KwElse, KwWhile, KwFor, KwIn, KwReturn, KwLet,
    
    // Identifiers & Literals
    Identifier, IntLiteral, FixedLiteral, StringLiteral,
    
    // Operators
    Plus, Minus, Star, Slash, Equals, EqualsEquals, 
    NotEquals, LessThan, GreaterThan, LessEqual, GreaterEqual,
    
    // Punctuation
    Colon, Comma, Dot, LeftParen, RightParen, LeftBracket, RightBracket
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

std::ostream& operator<<(std::ostream& os, const Token& token);

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source_;
    size_t cursor_{0};
    int line_{1};
    int col_{1};
    
    std::vector<int> indent_stack_;
    std::vector<Token> pending_tokens_;
    bool at_line_start_{true};

    char peek() const;
    char advance();
    bool is_at_end() const;
    
    void handle_indentation();
    Token scan_token();
    
    Token make_token(TokenType type, const std::string& val = "");
    void skip_whitespace_and_comments();
};

} // namespace ooey_station::compiler
