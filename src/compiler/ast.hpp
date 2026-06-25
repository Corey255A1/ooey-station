#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <ooey/types.hpp>

namespace ooey_station::compiler {

enum class DataType {
    Void,
    Int,
    Fixed,
    Bool,
    String,
    Color,
    SpriteId,
    TileId,
    SoundId,
    Unknown
};

class ASTVisitor;

struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor* visitor) = 0;
};

struct ExpressionNode : public ASTNode {};
struct StatementNode : public ASTNode {};

// Expressions

struct IntLiteralNode : public ExpressionNode {
    int value;
    IntLiteralNode(int val) : value(val) {}
    void accept(ASTVisitor* visitor) override;
};

struct FixedLiteralNode : public ExpressionNode {
    float value; // Stored as float, will be converted to fixed16.16 during codegen
    FixedLiteralNode(float val) : value(val) {}
    void accept(ASTVisitor* visitor) override;
};

struct StringLiteralNode : public ExpressionNode {
    std::string value;
    StringLiteralNode(const std::string& val) : value(val) {}
    void accept(ASTVisitor* visitor) override;
};

struct VarAccessNode : public ExpressionNode {
    std::string name;
    VarAccessNode(const std::string& n) : name(n) {}
    void accept(ASTVisitor* visitor) override;
};

struct BinaryOpNode : public ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
    
    BinaryOpNode(const std::string& o, std::unique_ptr<ExpressionNode> l, std::unique_ptr<ExpressionNode> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    void accept(ASTVisitor* visitor) override;
};

struct UnaryOpNode : public ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> operand;
    
    UnaryOpNode(const std::string& o, std::unique_ptr<ExpressionNode> opnd)
        : op(o), operand(std::move(opnd)) {}
    void accept(ASTVisitor* visitor) override;
};

struct FuncCallNode : public ExpressionNode {
    std::string name;
    std::vector<std::unique_ptr<ExpressionNode>> args;
    
    FuncCallNode(const std::string& n, std::vector<std::unique_ptr<ExpressionNode>> a)
        : name(n), args(std::move(a)) {}
    void accept(ASTVisitor* visitor) override;
};

// Statements

struct BlockNode : public StatementNode {
    std::vector<std::unique_ptr<StatementNode>> statements;
    void accept(ASTVisitor* visitor) override;
};

struct VarDeclNode : public StatementNode {
    std::string name;
    DataType type;
    std::unique_ptr<ExpressionNode> init_value;
    
    VarDeclNode(const std::string& n, DataType t, std::unique_ptr<ExpressionNode> init = nullptr)
        : name(n), type(t), init_value(std::move(init)) {}
    void accept(ASTVisitor* visitor) override;
};

struct IfNode : public StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<BlockNode> true_branch;
    std::unique_ptr<BlockNode> false_branch;
    
    IfNode(std::unique_ptr<ExpressionNode> cond, std::unique_ptr<BlockNode> tb, std::unique_ptr<BlockNode> fb = nullptr)
        : condition(std::move(cond)), true_branch(std::move(tb)), false_branch(std::move(fb)) {}
    void accept(ASTVisitor* visitor) override;
};

struct WhileNode : public StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<BlockNode> body;
    
    WhileNode(std::unique_ptr<ExpressionNode> cond, std::unique_ptr<BlockNode> b)
        : condition(std::move(cond)), body(std::move(b)) {}
    void accept(ASTVisitor* visitor) override;
};

struct ForNode : public StatementNode {
    std::string var_name;
    std::unique_ptr<ExpressionNode> start;
    std::unique_ptr<ExpressionNode> end;
    std::unique_ptr<BlockNode> body;
    
    ForNode(const std::string& vn, std::unique_ptr<ExpressionNode> s, std::unique_ptr<ExpressionNode> e, std::unique_ptr<BlockNode> b)
        : var_name(vn), start(std::move(s)), end(std::move(e)), body(std::move(b)) {}
    void accept(ASTVisitor* visitor) override;
};

struct ReturnNode : public StatementNode {
    std::unique_ptr<ExpressionNode> value;
    ReturnNode(std::unique_ptr<ExpressionNode> val = nullptr) : value(std::move(val)) {}
    void accept(ASTVisitor* visitor) override;
};

struct ExprStatementNode : public StatementNode {
    std::unique_ptr<ExpressionNode> expr;
    ExprStatementNode(std::unique_ptr<ExpressionNode> e) : expr(std::move(e)) {}
    void accept(ASTVisitor* visitor) override;
};

// Top-Level Declarations

struct FunctionNode : public ASTNode {
    std::string name;
    struct Param {
        std::string name;
        DataType type;
    };
    std::vector<Param> params;
    DataType return_type;
    std::unique_ptr<BlockNode> body;
    
    FunctionNode(const std::string& n, std::vector<Param> p, DataType rt, std::unique_ptr<BlockNode> b)
        : name(n), params(p), return_type(rt), body(std::move(b)) {}
    void accept(ASTVisitor* visitor) override;
};

// Declarative Sections

struct SpriteAsset {
    std::string name;
    int width;
    int height;
    std::vector<std::string> grid; // ASCII-grid representation
};

struct TileAsset {
    std::string name;
    int width;
    int height;
    std::vector<std::string> grid;
};

struct ProgramNode : public ASTNode {
    std::map<std::string, std::string> metadata;
    std::map<std::string, ooey::Color> palette;
    std::vector<SpriteAsset> sprites;
    std::vector<TileAsset> tiles;
    std::vector<std::unique_ptr<VarDeclNode>> global_vars;
    std::vector<std::unique_ptr<FunctionNode>> functions;
    
    void accept(ASTVisitor* visitor) override;
};

// Visitor Interface

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(IntLiteralNode* node) = 0;
    virtual void visit(FixedLiteralNode* node) = 0;
    virtual void visit(StringLiteralNode* node) = 0;
    virtual void visit(VarAccessNode* node) = 0;
    virtual void visit(BinaryOpNode* node) = 0;
    virtual void visit(UnaryOpNode* node) = 0;
    virtual void visit(FuncCallNode* node) = 0;
    virtual void visit(BlockNode* node) = 0;
    virtual void visit(VarDeclNode* node) = 0;
    virtual void visit(IfNode* node) = 0;
    virtual void visit(WhileNode* node) = 0;
    virtual void visit(ForNode* node) = 0;
    virtual void visit(ReturnNode* node) = 0;
    virtual void visit(ExprStatementNode* node) = 0;
    virtual void visit(FunctionNode* node) = 0;
    virtual void visit(ProgramNode* node) = 0;
};

inline void IntLiteralNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void FixedLiteralNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void StringLiteralNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void VarAccessNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void BinaryOpNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void UnaryOpNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void FuncCallNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void BlockNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void VarDeclNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void IfNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void WhileNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void ForNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void ReturnNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void ExprStatementNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void FunctionNode::accept(ASTVisitor* visitor) { visitor->visit(this); }
inline void ProgramNode::accept(ASTVisitor* visitor) { visitor->visit(this); }

} // namespace ooey_station::compiler
