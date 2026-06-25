# Ooey-Station: Booey Compiler Implementation

This document details the architecture and implementation of `booeyc`, the standalone compiler for the Booey scripting language.

## 1. Compiler Overview

The `booeyc` tool is a command-line application that reads a `.booey` source file and generates a `.booey` binary bytecode executable. It is built natively in C++ alongside the Ooey-Station project.

### Pipeline
The compiler follows a traditional multi-pass pipeline:
1. **Lexical Analysis (Lexer)**: Converts source text into a stream of tokens. Handles significant indentation.
2. **Parsing (Parser)**: Constructs an Abstract Syntax Tree (AST) from the token stream.
3. **Semantic Analysis**: Resolves identifiers, performs type checking, and validates function signatures.
4. **Asset Packing**: Processes declarative blocks (palettes, sprites) into binary structures.
5. **Code Generation (Codegen)**: Translates the AST into Booey VM opcodes.
6. **Linking/Binary Emission**: Assembles the header, code segment, data segment, and asset segment into the final `.booey` file.

---

## 2. Lexical Analysis (Lexer)

The Lexer scans the input string and produces `Token` objects.

### Significant Indentation
Booey uses Python-style significant indentation to define scope. The lexer manages this using an indentation stack:
- When a line has more leading spaces than the top of the stack, it emits an `INDENT` token and pushes the new level.
- When a line has fewer spaces, it pops levels off the stack, emitting a `DEDENT` token for each level popped.
- A strict rule of exactly 4 spaces per indentation level is enforced to prevent mixed-tab errors.

### Token Types
```cpp
enum class TokenType {
    // Structural
    Indent, Dedent, Newline, Eof,
    
    // Keywords
    KwGame, KwPalette, KwSprites, KwVars, KwFn, 
    KwIf, KwElif, KwElse, KwWhile, KwFor, KwIn, KwReturn,
    
    // Identifiers & Literals
    Identifier, IntLiteral, FixedLiteral, StringLiteral,
    
    // Operators
    Plus, Minus, Star, Slash, Equals, EqualsEquals, 
    NotEquals, LessThan, GreaterThan, LessEqual, GreaterEqual,
    
    // Punctuation
    Colon, Comma, Dot, LeftParen, RightParen
};
```

---

## 3. Parsing (AST Construction)

The parser is a hand-written Recursive Descent parser. It consumes tokens and builds a hierarchical AST.

### AST Node Hierarchy
All nodes inherit from `ASTNode`.
- **Declarations**: `GameMetaNode`, `PaletteNode`, `SpriteDefNode`, `VarDeclNode`, `FunctionNode`
- **Statements**: `BlockNode`, `IfNode`, `WhileNode`, `ForNode`, `ReturnNode`, `ExprStatementNode`
- **Expressions**: `BinaryOpNode`, `UnaryOpNode`, `IntLiteralNode`, `StringLiteralNode`, `VarAccessNode`, `FuncCallNode`

### Example: Parsing an `If` Statement
```cpp
// Pseudocode for parse_if_statement
std::unique_ptr<IfNode> Parser::parse_if() {
    consume(TokenType::KwIf);
    auto condition = parse_expression();
    consume(TokenType::Colon);
    consume(TokenType::Newline);
    
    auto true_block = parse_block(); // Expects INDENT, parses stmts, expects DEDENT
    
    std::unique_ptr<BlockNode> false_block = nullptr;
    if (match(TokenType::KwElse)) {
        consume(TokenType::Colon);
        consume(TokenType::Newline);
        false_block = parse_block();
    }
    
    return std::make_unique<IfNode>(std::move(condition), std::move(true_block), std::move(false_block));
}
```

---

## 4. Semantic Analysis

Before generating code, the compiler traverses the AST to ensure the program is valid.

1. **Symbol Table Generation**: Collects all global variables, functions, and asset identifiers into a `SymbolTable`.
2. **Type Checking**: Ensures that assignments and function arguments match their expected types. (e.g., passing a `string` to a math function throws an error).
3. **Asset Validation**: Ensures that sprites referenced in `draw_sprite()` actually exist in the `sprites:` block.

---

## 5. Code Generation

The Code Generator (`Codegen` class) traverses the validated AST and emits bytes into a buffer.

### Register Allocation
To keep the compiler simple, `booeyc` uses a basic register allocation scheme:
- `R0` is the accumulator / return value register.
- `R1` through `R14` are used as temporary scratch registers during expression evaluation.
- Local variables in functions are mapped to specific registers if there are few, or spilled to the Game RAM stack if there are many.

### Compiling Expressions
Expression trees are evaluated post-order (leaves first).
Example: `a + 5`
1. Load `a` into `R1`.
2. Load immediate `5` into `R2` (`MOVI R2, 5`).
3. Add `R2` to `R1` (`ADD R1, R2`).

### Compiling Control Flow
Control flow requires emitting jump instructions and patching the jump targets.

Example: `If/Else`
```
    [Evaluate Condition into R1]
    CMP R1, 0
    JZ .FalseBlock   <-- Jump to Else block if condition is 0 (false)
.TrueBlock:
    [Emit True Block statements]
    JMP .EndIf       <-- Skip the Else block
.FalseBlock:
    [Emit False Block statements]
.EndIf:
```
The compiler maintains a list of "fixups" (addresses that need to be patched once the destination offset is known).

### Compiling Built-in Functions
Calls to built-in functions (e.g., `draw_text`) do not use the standard `CALL` instruction. Instead, the compiler maps them directly to the corresponding VM opcodes.

Example: `fill_rect(x, y, 10, 10, red)`
1. Evaluate `x` into `R1`.
2. Evaluate `y` into `R2`.
3. Load `10` into `R3`.
4. Load `10` into `R4`.
5. Resolve `red` to its palette index and load into `R5`.
6. Emit `FRECT R1, R2, R3, R4, R5`.

---

## 6. Binary Emission

Once the code is generated and assets are packed, the compiler writes the final `.booey` file.

1. **Calculate Layout**: Determine the exact byte sizes of the Code, Data, and Asset segments.
2. **Write Header**: Write the 32-byte header with the `BOOE` magic number, segment sizes, and entry point (the address of the `init` function).
3. **Write Segments**: Stream the buffers for Code, Data, and Assets to the file.
4. **Checksum**: Calculate the CRC32 of the written segments and patch the header.

### Usage
```bash
$ booeyc mygame.booey -o mygame.booey.bin
Compiled successfully. Code: 402 bytes, Data: 120 bytes, Assets: 840 bytes.
```
