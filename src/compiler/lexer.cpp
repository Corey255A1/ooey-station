#include "lexer.hpp"
#include <cctype>
#include <sstream>
#include <map>
#include <stdexcept>
#include <iostream>

namespace ooey_station::compiler {

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "Token(type: " << static_cast<int>(token.type) 
       << ", value: \"" << token.value << "\""
       << ", line: " << token.line 
       << ", col: " << token.column << ")";
    return os;
}

Lexer::Lexer(const std::string& source) : source_(source) {
    indent_stack_.push_back(0);
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[cursor_];
}

char Lexer::advance() {
    char c = source_[cursor_++];
    if (c == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    return c;
}

bool Lexer::is_at_end() const {
    return cursor_ >= source_.size();
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            // If we are at the start of a line, we shouldn't just skip spaces yet 
            // since we need them to calculate indentation.
            if (at_line_start_) break;
            advance();
        } else if (c == '#') {
            // Comment goes to the end of the line
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

bool Lexer::handle_indentation() {
    if (!at_line_start_) return false;

    int spaces = 0;
    while (!is_at_end() && (peek() == ' ' || peek() == '\t')) {
        char c = advance();
        if (c == '\t') {
            throw std::runtime_error("Lexer Error: Tabs are not allowed for indentation. Use spaces.");
        }
        spaces++;
    }

    // Skip comment lines and empty lines without affecting indentation state
    skip_whitespace_and_comments();
    if (peek() == '\n' || is_at_end()) {
        if (peek() == '\n') advance();
        return true; 
    }

    int current_indent = indent_stack_.back();
    if (spaces > current_indent) {
        indent_stack_.push_back(spaces);
        pending_tokens_.push_back(make_token(TokenType::Indent, std::to_string(spaces)));
    } else if (spaces < current_indent) {
        while (!indent_stack_.empty() && indent_stack_.back() > spaces) {
            indent_stack_.pop_back();
            pending_tokens_.push_back(make_token(TokenType::Dedent));
        }
        if (indent_stack_.empty() || indent_stack_.back() != spaces) {
            throw std::runtime_error("Lexer Error: Inconsistent indentation on line " + std::to_string(line_));
        }
    }
    at_line_start_ = false;
    return false;
}

Token Lexer::make_token(TokenType type, const std::string& val) {
    return Token{type, val, line_, col_};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!is_at_end()) {
        skip_whitespace_and_comments();
        if (is_at_end()) break;

        if (at_line_start_) {
            if (handle_indentation()) {
                continue;
            }
            // Process any pending Indent/Dedent tokens we just generated
            for (const auto& t : pending_tokens_) {
                tokens.push_back(t);
            }
            pending_tokens_.clear();
            if (is_at_end()) break;
        }

        char c = peek();
        if (c == '\n') {
            advance();
            tokens.push_back(make_token(TokenType::Newline));
            at_line_start_ = true;
            continue;
        }

        tokens.push_back(scan_token());
    }

    // Unwind all remaining indent levels
    while (indent_stack_.size() > 1) {
        indent_stack_.pop_back();
        tokens.push_back(make_token(TokenType::Dedent));
    }
    
    tokens.push_back(make_token(TokenType::Eof));
    return tokens;
}

Token Lexer::scan_token() {
    char c = advance();

    // Check operators and punctuation
    switch (c) {
        case '(': return make_token(TokenType::LeftParen);
        case ')': return make_token(TokenType::RightParen);
        case '[': return make_token(TokenType::LeftBracket);
        case ']': return make_token(TokenType::RightBracket);
        case ':': return make_token(TokenType::Colon);
        case ',': return make_token(TokenType::Comma);
        case '.': return make_token(TokenType::Dot);
        
        case '+': return make_token(TokenType::Plus);
        case '-': return make_token(TokenType::Minus);
        case '*': return make_token(TokenType::Star);
        case '/': return make_token(TokenType::Slash);
        
        case '=':
            if (peek() == '=') {
                advance();
                return make_token(TokenType::EqualsEquals);
            }
            return make_token(TokenType::Equals);
            
        case '!':
            if (peek() == '=') {
                advance();
                return make_token(TokenType::NotEquals);
            }
            throw std::runtime_error("Lexer Error: Unexpected character '!' at line " + std::to_string(line_));
            
        case '<':
            if (peek() == '=') {
                advance();
                return make_token(TokenType::LessEqual);
            }
            return make_token(TokenType::LessThan);
            
        case '>':
            if (peek() == '=') {
                advance();
                return make_token(TokenType::GreaterEqual);
            }
            return make_token(TokenType::GreaterThan);
            
        case '"': {
            std::string str;
            while (!is_at_end() && peek() != '"') {
                char sc = advance();
                if (sc == '\\') {
                    if (peek() == 'n') { advance(); str += '\n'; }
                    else if (peek() == 't') { advance(); str += '\t'; }
                    else { str += advance(); }
                } else {
                    str += sc;
                }
            }
            if (is_at_end()) {
                throw std::runtime_error("Lexer Error: Unterminated string literal at line " + std::to_string(line_));
            }
            advance(); // Consume closing quote
            return make_token(TokenType::StringLiteral, str);
        }
    }

    // Number literals
    if (std::isdigit(c)) {
        std::string num(1, c);
        bool has_dot = false;
        
        while (std::isdigit(peek()) || (peek() == '.' && !has_dot)) {
            char nc = advance();
            if (nc == '.') has_dot = true;
            num += nc;
        }
        
        if (has_dot && peek() == 'f') {
            advance(); // consume 'f' for fixed suffix
            return make_token(TokenType::FixedLiteral, num);
        }
        
        if (has_dot) {
            return make_token(TokenType::FixedLiteral, num);
        }
        return make_token(TokenType::IntLiteral, num);
    }

    // Identifiers and Keywords
    if (std::isalpha(c) || c == '_') {
        std::string ident(1, c);
        while (std::isalnum(peek()) || peek() == '_') {
            ident += advance();
        }

        static const std::map<std::string, TokenType> keywords = {
            {"game", TokenType::KwGame},
            {"palette", TokenType::KwPalette},
            {"sprites", TokenType::KwSprites},
            {"tiles", TokenType::KwTiles},
            {"vars", TokenType::KwVars},
            {"fn", TokenType::KwFn},
            {"if", TokenType::KwIf},
            {"elif", TokenType::KwElif},
            {"else", TokenType::KwElse},
            {"while", TokenType::KwWhile},
            {"for", TokenType::KwFor},
            {"in", TokenType::KwIn},
            {"return", TokenType::KwReturn},
            {"let", TokenType::KwLet}
        };

        auto it = keywords.find(ident);
        if (it != keywords.end()) {
            return make_token(it->second, ident);
        }
        return make_token(TokenType::Identifier, ident);
    }

    // If it's space but we didn't consume it at line start, throw or skip
    if (c == ' ') {
        throw std::runtime_error("Lexer Error: Unexpected space character within line " + std::to_string(line_));
    }

    throw std::runtime_error("Lexer Error: Unexpected character '" + std::string(1, c) + "' at line " + std::to_string(line_));
}

} // namespace ooey_station::compiler
