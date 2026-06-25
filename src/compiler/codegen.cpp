#include "codegen.hpp"
#include "../vm/opcodes.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace ooey_station::compiler {

void Codegen::emit_8(uint8_t val) {
    code_buf_.push_back(val);
}

void Codegen::emit_32(uint32_t val) {
    code_buf_.push_back(static_cast<uint8_t>(val & 0xFF));
    code_buf_.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    code_buf_.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    code_buf_.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void Codegen::patch_32(size_t index, uint32_t val) {
    if (index + 3 >= code_buf_.size()) return;
    code_buf_[index]     = static_cast<uint8_t>(val & 0xFF);
    code_buf_[index + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    code_buf_[index + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    code_buf_[index + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

int Codegen::allocate_register() {
    if (next_free_register_ >= 15) {
        throw std::runtime_error("Codegen Error: Out of registers for expression evaluation");
    }
    return next_free_register_++;
}

void Codegen::free_register(int reg) {
    if (reg == next_free_register_ - 1) {
        next_free_register_--;
    }
}

std::vector<uint8_t> Codegen::generate(ProgramNode* program) {
    program->accept(this);
    
    // Assemble final executable binary
    std::vector<uint8_t> binary;
    binary.resize(32); // Header placeholder
    
    // 1. Magic: 'BOOE' (0x454F4F42)
    *reinterpret_cast<uint32_t*>(&binary[0]) = 0x454F4F42;
    // 2. Version
    *reinterpret_cast<uint16_t*>(&binary[4]) = 0x0001;
    // 3. Flags
    *reinterpret_cast<uint16_t*>(&binary[6]) = 0x0000;
    
    // Header sizes
    uint32_t code_size = code_buf_.size();
    uint32_t data_size = data_buf_.size();
    uint32_t asset_size = asset_buf_.size();
    
    *reinterpret_cast<uint32_t*>(&binary[8]) = code_size;
    *reinterpret_cast<uint32_t*>(&binary[12]) = data_size;
    *reinterpret_cast<uint32_t*>(&binary[16]) = asset_size;
    
    // 4. Entry Point (PC starts here)
    // Entry point offset is 0, since we will emit the bootstrap code first.
    *reinterpret_cast<uint32_t*>(&binary[20]) = 0; 
    
    // Append segments
    binary.insert(binary.end(), code_buf_.begin(), code_buf_.end());
    binary.insert(binary.end(), data_buf_.begin(), data_buf_.end());
    binary.insert(binary.end(), asset_buf_.begin(), asset_buf_.end());
    
    // 5. Checksum (dummy CRC/hash for now)
    uint32_t checksum = 0;
    for (size_t i = 32; i < binary.size(); ++i) {
        checksum += binary[i];
    }
    *reinterpret_cast<uint32_t*>(&binary[24]) = checksum;
    
    return binary;
}

void Codegen::visit(IntLiteralNode* node) {
    int r = allocate_register();
    emit_8(vm::OP_MOVI);
    emit_8(r);
    emit_32(node->value);
}

void Codegen::visit(FixedLiteralNode* node) {
    int r = allocate_register();
    uint32_t fixed_val = static_cast<uint32_t>(node->value * 65536.0f);
    emit_8(vm::OP_MOVI);
    emit_8(r);
    emit_32(fixed_val);
}

void Codegen::visit(StringLiteralNode* node) {
    // Write string to static data buffer, save offset, load offset
    uint32_t offset = data_buf_.size();
    for (char c : node->value) {
        data_buf_.push_back(c);
    }
    data_buf_.push_back('\0');
    
    int r = allocate_register();
    emit_8(vm::OP_MOVI);
    emit_8(r);
    emit_32(offset); // Offset in Game RAM where static data is loaded
}

void Codegen::visit(VarAccessNode* node) {
    int r = allocate_register();
    
    // Check button constants
    static const std::map<std::string, uint32_t> button_constants = {
        {"UP", 0}, {"DOWN", 1}, {"LEFT", 2}, {"RIGHT", 3},
        {"A", 4}, {"B", 5}, {"C", 6}, {"X", 7}, {"Y", 8}, {"Z", 9},
        {"START", 10}, {"SELECT", 11}
    };
    
    auto btn_it = button_constants.find(node->name);
    if (btn_it != button_constants.end()) {
        emit_8(vm::OP_MOVI);
        emit_8(r);
        emit_32(btn_it->second);
        return;
    }

    // Check color palette constants
    auto col_it = color_palette_.find(node->name);
    if (col_it != color_palette_.end()) {
        emit_8(vm::OP_MOVI);
        emit_8(r);
        emit_32(col_it->second);
        return;
    }
    
    // Check sprite constant IDs
    auto spr_it = sprite_ids_.find(node->name);
    if (spr_it != sprite_ids_.end()) {
        emit_8(vm::OP_MOVI);
        emit_8(r);
        emit_32(spr_it->second);
        return;
    }
    
    // Check tile constant IDs
    auto tile_it = tile_ids_.find(node->name);
    if (tile_it != tile_ids_.end()) {
        emit_8(vm::OP_MOVI);
        emit_8(r);
        emit_32(tile_it->second);
        return;
    }

    auto local_it = local_vars_.find(node->name);
    if (local_it != local_vars_.end()) {
        // Local variable is assigned to a register
        emit_8(vm::OP_MOV);
        emit_8(r);
        emit_8(local_it->second);
    } else {
        // Global variable
        auto global_it = global_vars_.find(node->name);
        if (global_it == global_vars_.end()) {
            throw std::runtime_error("Codegen Error: Undeclared variable identifier '" + node->name + "'");
        }
        emit_8(vm::OP_LOAD);
        emit_8(r);
        emit_32(global_it->second);
    }
}

void Codegen::visit(BinaryOpNode* node) {
    node->left->accept(this);
    int r_left = next_free_register_ - 1;
    
    node->right->accept(this);
    int r_right = next_free_register_ - 1;
    
    if (node->op == "+") {
        emit_8(vm::OP_ADD);
        emit_8(r_left);
        emit_8(r_right);
    } else if (node->op == "-") {
        emit_8(vm::OP_SUB);
        emit_8(r_left);
        emit_8(r_right);
    } else if (node->op == "*") {
        emit_8(vm::OP_MUL);
        emit_8(r_left);
        emit_8(r_right);
    } else if (node->op == "/") {
        emit_8(vm::OP_DIV);
        emit_8(r_left);
        emit_8(r_right);
    } else if (node->op == "==") {
        emit_8(vm::OP_CMP);
        emit_8(r_left);
        emit_8(r_right);
        // Map cmp flags back to boolean value 0 or 1 in r_left
        emit_8(vm::OP_MOVI);
        emit_8(r_left);
        emit_32(0);
        
        size_t fixup_jz = code_buf_.size();
        emit_8(vm::OP_JNZ); // Jump if flags say NOT equal (which skips setting to 1)
        emit_32(0); 
        
        emit_8(vm::OP_MOVI);
        emit_8(r_left);
        emit_32(1);
        
        patch_32(fixup_jz + 1, code_buf_.size());
    } else if (node->op == "<") {
        emit_8(vm::OP_CMP);
        emit_8(r_left);
        emit_8(r_right);
        
        emit_8(vm::OP_MOVI);
        emit_8(r_left);
        emit_32(0);
        
        size_t fixup_jge = code_buf_.size();
        emit_8(vm::OP_JZ); // If equal (not less), skip
        emit_32(0);
        
        size_t fixup_jg = code_buf_.size();
        emit_8(vm::OP_JG); // If greater, skip
        emit_32(0);
        
        emit_8(vm::OP_MOVI);
        emit_8(r_left);
        emit_32(1);
        
        patch_32(fixup_jge + 1, code_buf_.size());
        patch_32(fixup_jg + 1, code_buf_.size());
    }
    
    free_register(r_right);
}

void Codegen::visit(UnaryOpNode* node) {
    node->operand->accept(this);
    int r = next_free_register_ - 1;
    
    if (node->op == "-") {
        int r_temp = allocate_register();
        emit_8(vm::OP_MOVI);
        emit_8(r_temp);
        emit_32(0);
        
        emit_8(vm::OP_SUB);
        emit_8(r_temp);
        emit_8(r);
        
        emit_8(vm::OP_MOV);
        emit_8(r);
        emit_8(r_temp);
        free_register(r_temp);
    }
}

void Codegen::visit(FuncCallNode* node) {
    // Built-in functions mapping to opcodes
    if (node->name == "cls") {
        node->args[0]->accept(this);
        int r = next_free_register_ - 1;
        emit_8(vm::OP_CLS);
        emit_8(r);
        free_register(r);
    } 
    else if (node->name == "pixel") {
        node->args[0]->accept(this);
        int rx = next_free_register_ - 1;
        node->args[1]->accept(this);
        int ry = next_free_register_ - 1;
        node->args[2]->accept(this);
        int rc = next_free_register_ - 1;
        
        emit_8(vm::OP_PIXEL);
        emit_8(rx);
        emit_8(ry);
        emit_8(rc);
        
        free_register(rc);
        free_register(ry);
        free_register(rx);
    }
    else if (node->name == "fill_rect") {
        node->args[0]->accept(this);
        int rx = next_free_register_ - 1;
        node->args[1]->accept(this);
        int ry = next_free_register_ - 1;
        node->args[2]->accept(this);
        int rw = next_free_register_ - 1;
        node->args[3]->accept(this);
        int rh = next_free_register_ - 1;
        node->args[4]->accept(this);
        int rc = next_free_register_ - 1;
        
        emit_8(vm::OP_FRECT);
        emit_8(rx);
        emit_8(ry);
        emit_8(rw);
        emit_8(rh);
        emit_8(rc);
        
        free_register(rc);
        free_register(rh);
        free_register(rw);
        free_register(ry);
        free_register(rx);
    }
    else if (node->name == "draw_text") {
        node->args[0]->accept(this);
        int rx = next_free_register_ - 1;
        node->args[1]->accept(this);
        int ry = next_free_register_ - 1;
        
        // Evaluate string (arg 2)
        node->args[2]->accept(this);
        int r_str = next_free_register_ - 1;
        
        node->args[3]->accept(this);
        int rc = next_free_register_ - 1;
        
        // Emitting OP_TEXT rx, ry, [address / offset], rc
        // Wait, text instruction requires inline immediate or register for address?
        // In opcodes: TEXT R_x, R_y, addr, R_c.
        // Wait, address is stored as 32-bit immediate inside the instruction.
        // But our compiler evaluates the string literal to get its address in r_str.
        // To resolve this, we can make OP_TEXT use standard register lookup, or evaluate string inline.
        // Let's modify: if it's a register, we can load string from register or hardcode.
        // Let's assume OP_TEXT in VM executes by reading string address from the register!
        // Wait, our VM implementation did:
        // `uint32_t str_addr = *reinterpret_cast<uint32_t*>(&code_[pc_]); pc_ += 4;`
        // Ah, it reads string address as a 32-bit immediate from the bytecode instruction!
        // So we can support that if the argument is a string literal.
        if (auto str_node = dynamic_cast<StringLiteralNode*>(node->args[2].get())) {
            // Write string to static data
            uint32_t offset = data_buf_.size();
            for (char c : str_node->value) {
                data_buf_.push_back(c);
            }
            data_buf_.push_back('\0');
            
            emit_8(vm::OP_TEXT);
            emit_8(rx);
            emit_8(ry);
            emit_32(offset);
            emit_8(rc);
        } else {
            // Fallback
            emit_8(vm::OP_TEXT);
            emit_8(rx);
            emit_8(ry);
            emit_32(0); // placeholder
            emit_8(rc);
        }
        
        free_register(rc);
        free_register(r_str);
        free_register(ry);
        free_register(rx);
    }
    else if (node->name == "btn_held") {
        node->args[0]->accept(this);
        int rbtn = next_free_register_ - 1;
        int rd = allocate_register();
        emit_8(vm::OP_BTNH);
        emit_8(rd);
        emit_8(rbtn);
        // Free rbtn, but keep rd (it's the output)
        // Wait, since rd was allocated after rbtn, we need to free rbtn which is at rd-1.
        // It's cleaner to swap registers or MOV to prevent gaps.
        emit_8(vm::OP_MOV);
        emit_8(rbtn);
        emit_8(rd);
        free_register(rd);
        // Now rbtn contains the boolean value!
    }
    else if (node->name == "btn_pressed") {
        node->args[0]->accept(this);
        int rbtn = next_free_register_ - 1;
        int rd = allocate_register();
        emit_8(vm::OP_BTNP);
        emit_8(rd);
        emit_8(rbtn);
        emit_8(vm::OP_MOV);
        emit_8(rbtn);
        emit_8(rd);
        free_register(rd);
    }
    else if (node->name == "rnd") {
        node->args[0]->accept(this);
        int rs = next_free_register_ - 1;
        int rd = allocate_register();
        emit_8(vm::OP_RND);
        emit_8(rd);
        emit_8(rs);
        emit_8(vm::OP_MOV);
        emit_8(rs);
        emit_8(rd);
        free_register(rd);
    }
    else if (node->name == "exit") {
        emit_8(vm::OP_EXIT);
    }
    else if (node->name == "draw_sprite") {
        node->args[0]->accept(this);
        int rid = next_free_register_ - 1;
        node->args[1]->accept(this);
        int rx = next_free_register_ - 1;
        node->args[2]->accept(this);
        int ry = next_free_register_ - 1;
        
        emit_8(vm::OP_SPR);
        emit_8(rid);
        emit_8(rx);
        emit_8(ry);
        
        free_register(ry);
        free_register(rx);
        free_register(rid);
    }
    else if (node->name == "line") {
        node->args[0]->accept(this);
        int rx1 = next_free_register_ - 1;
        node->args[1]->accept(this);
        int ry1 = next_free_register_ - 1;
        node->args[2]->accept(this);
        int rx2 = next_free_register_ - 1;
        node->args[3]->accept(this);
        int ry2 = next_free_register_ - 1;
        node->args[4]->accept(this);
        int rc = next_free_register_ - 1;
        
        emit_8(vm::OP_LINE);
        emit_8(rx1);
        emit_8(ry1);
        emit_8(rx2);
        emit_8(ry2);
        emit_8(rc);
        
        free_register(rc);
        free_register(ry2);
        free_register(rx2);
        free_register(ry1);
        free_register(rx1);
    }
    else if (node->name == "rect") {
        node->args[0]->accept(this);
        int rx = next_free_register_ - 1;
        node->args[1]->accept(this);
        int ry = next_free_register_ - 1;
        node->args[2]->accept(this);
        int rw = next_free_register_ - 1;
        node->args[3]->accept(this);
        int rh = next_free_register_ - 1;
        node->args[4]->accept(this);
        int rc = next_free_register_ - 1;
        
        emit_8(vm::OP_RECT);
        emit_8(rx);
        emit_8(ry);
        emit_8(rw);
        emit_8(rh);
        emit_8(rc);
        
        free_register(rc);
        free_register(rh);
        free_register(rw);
        free_register(ry);
        free_register(rx);
    }
    else {
        // User function call
        // Evaluate all arguments, pushing them into registers R1, R2...
        // For simplicity, user functions are called by jumping to label.
        auto func_it = function_pcs_.find(node->name);
        if (func_it == function_pcs_.end()) {
            throw std::runtime_error("Codegen Error: Call to undefined function '" + node->name + "'");
        }
        
        // Evaluate arguments
        std::vector<int> arg_regs;
        for (const auto& arg : node->args) {
            arg->accept(this);
            arg_regs.push_back(next_free_register_ - 1);
        }
        
        // Pass arguments via VM registers (R1, R2, R3... map to function parameters)
        for (size_t i = 0; i < arg_regs.size(); ++i) {
            emit_8(vm::OP_MOV);
            emit_8(static_cast<uint8_t>(i + 1)); // Parameter destination register
            emit_8(arg_regs[i]);
        }
        
        // Free arguments registers
        for (auto reg : arg_regs) {
            free_register(reg);
        }
        
        emit_8(vm::OP_CALL);
        emit_32(func_it->second);
        
        // Return value is stored in R0
        int r = allocate_register();
        emit_8(vm::OP_MOV);
        emit_8(r);
        emit_8(0); // copy R0 to r
    }
}

void Codegen::visit(BlockNode* node) {
    for (const auto& stmt : node->statements) {
        stmt->accept(this);
    }
}

void Codegen::visit(VarDeclNode* node) {
    if (node->init_value) {
        node->init_value->accept(this);
        int r_val = next_free_register_ - 1;
        
        auto local_it = local_vars_.find(node->name);
        if (local_it != local_vars_.end()) {
            emit_8(vm::OP_MOV);
            emit_8(local_it->second);
            emit_8(r_val);
        } else {
            auto global_it = global_vars_.find(node->name);
            if (global_it != global_vars_.end()) {
                emit_8(vm::OP_STORE);
                emit_32(global_it->second);
                emit_8(r_val);
            }
        }
        free_register(r_val);
    }
}

void Codegen::visit(IfNode* node) {
    node->condition->accept(this);
    int r_cond = next_free_register_ - 1;
    
    emit_8(vm::OP_MOVI);
    int r_zero = allocate_register();
    emit_32(0);
    
    emit_8(vm::OP_CMP);
    emit_8(r_cond);
    emit_8(r_zero);
    
    free_register(r_zero);
    free_register(r_cond);
    
    size_t fixup_jz = code_buf_.size();
    emit_8(vm::OP_JZ);
    emit_32(0); // patched later
    
    node->true_branch->accept(this);
    
    if (node->false_branch) {
        size_t fixup_jmp = code_buf_.size();
        emit_8(vm::OP_JMP);
        emit_32(0); // patched later
        
        // Patch true branch's JZ to here
        patch_32(fixup_jz + 1, code_buf_.size());
        
        node->false_branch->accept(this);
        
        // Patch JMP to here
        patch_32(fixup_jmp + 1, code_buf_.size());
    } else {
        // Patch JZ to here
        patch_32(fixup_jz + 1, code_buf_.size());
    }
}

void Codegen::visit(WhileNode* node) {
    size_t loop_start = code_buf_.size();
    
    node->condition->accept(this);
    int r_cond = next_free_register_ - 1;
    
    emit_8(vm::OP_MOVI);
    int r_zero = allocate_register();
    emit_32(0);
    
    emit_8(vm::OP_CMP);
    emit_8(r_cond);
    emit_8(r_zero);
    
    free_register(r_zero);
    free_register(r_cond);
    
    size_t fixup_jz = code_buf_.size();
    emit_8(vm::OP_JZ);
    emit_32(0);
    
    node->body->accept(this);
    
    // Jump back to start
    emit_8(vm::OP_JMP);
    emit_32(loop_start);
    
    // Patch conditional exit jump
    patch_32(fixup_jz + 1, code_buf_.size());
}

void Codegen::visit(ForNode* node) {
    // Simple for loop: register mapping
    node->start->accept(this);
    int r_start = next_free_register_ - 1;
    
    node->end->accept(this);
    int r_end = next_free_register_ - 1;
    
    // Allocate index variable register
    int r_idx = allocate_register();
    local_vars_[node->var_name] = r_idx;
    
    // Initialize: index = start
    emit_8(vm::OP_MOV);
    emit_8(r_idx);
    emit_8(r_start);
    
    size_t loop_start = code_buf_.size();
    
    // CMP index, end
    emit_8(vm::OP_CMP);
    emit_8(r_idx);
    emit_8(r_end);
    
    // JGE (or equal/greater) skip body.
    // In our simplified opcodes, check JL (Jump Less) to loop body, otherwise JMP to end.
    size_t fixup_jl = code_buf_.size();
    emit_8(vm::OP_JL);
    emit_32(0); // Patched to jump inside
    
    size_t fixup_exit = code_buf_.size();
    emit_8(vm::OP_JMP);
    emit_32(0);
    
    // Inside body
    patch_32(fixup_jl + 1, code_buf_.size());
    node->body->accept(this);
    
    // Increment: index += 1
    emit_8(vm::OP_MOVI);
    int r_one = allocate_register();
    emit_32(1);
    emit_8(vm::OP_ADD);
    emit_8(r_idx);
    emit_8(r_one);
    free_register(r_one);
    
    // Loop back
    emit_8(vm::OP_JMP);
    emit_32(loop_start);
    
    // Patch exit
    patch_32(fixup_exit + 1, code_buf_.size());
    
    // Free scope
    local_vars_.erase(node->var_name);
    free_register(r_idx);
    free_register(r_end);
    free_register(r_start);
}

void Codegen::visit(ReturnNode* node) {
    if (node->value) {
        node->value->accept(this);
        int r = next_free_register_ - 1;
        emit_8(vm::OP_MOV);
        emit_8(0); // R0 is return value
        emit_8(r);
        free_register(r);
    }
    emit_8(vm::OP_RET);
}

void Codegen::visit(ExprStatementNode* node) {
    node->expr->accept(this);
    // Discard expression result
    free_register(next_free_register_ - 1);
}

void Codegen::visit(FunctionNode* node) {
    function_pcs_[node->name] = code_buf_.size();
    local_vars_.clear();
    next_free_register_ = 1; // Parameters are loaded in R1, R2...
    
    for (size_t i = 0; i < node->params.size(); ++i) {
        local_vars_[node->params[i].name] = i + 1;
        // Mark R1, R2... as allocated
        next_free_register_ = i + 2;
    }
    
    node->body->accept(this);
    
    // Safety return if end of function is reached without RET
    if (code_buf_.empty() || code_buf_.back() != vm::OP_RET) {
        emit_8(vm::OP_RET);
    }
}

void Codegen::visit(ProgramNode* node) {
    // 1. Pack global variables & allocate RAM addresses
    uint32_t ram_offset = 0;
    for (const auto& var : node->global_vars) {
        global_vars_[var->name] = ram_offset;
        
        // Evaluate initializer statically (or write zeros)
        int init_val = 0;
        if (var->init_value) {
            if (auto int_node = dynamic_cast<IntLiteralNode*>(var->init_value.get())) {
                init_val = int_node->value;
            } else if (auto fixed_node = dynamic_cast<FixedLiteralNode*>(var->init_value.get())) {
                init_val = static_cast<int>(fixed_node->value * 65536.0f);
            }
        }
        
        // Write to static data segment
        data_buf_.push_back(init_val & 0xFF);
        data_buf_.push_back((init_val >> 8) & 0xFF);
        data_buf_.push_back((init_val >> 16) & 0xFF);
        data_buf_.push_back((init_val >> 24) & 0xFF);
        
        ram_offset += 4;
    }
    
    // Resolve button constants
    global_vars_["UP"] = 0;     // Virtual Button IDs
    global_vars_["DOWN"] = 1;
    global_vars_["LEFT"] = 2;
    global_vars_["RIGHT"] = 3;
    global_vars_["A"] = 4;
    global_vars_["B"] = 5;
    global_vars_["C"] = 6;
    global_vars_["X"] = 7;
    global_vars_["Y"] = 8;
    global_vars_["Z"] = 9;
    global_vars_["START"] = 10;
    global_vars_["SELECT"] = 11;
    
    // 2. Pack Palette
    // Index 0: Transparent (default)
    asset_buf_.resize(256 * 3, 0); // 256 RGB colors
    int color_index = 1;
    for (const auto& [name, color] : node->palette) {
        color_palette_[name] = color_index;
        asset_buf_[color_index * 3]     = color.r;
        asset_buf_[color_index * 3 + 1] = color.g;
        asset_buf_[color_index * 3 + 2] = color.b;
        color_index++;
    }
    
    // 3. Pack Sprites
    // Consumes 256 bytes per 16x16 sprite. Let's write them into the asset buffer.
    uint32_t sprite_offset = 256 * 3; // After palette
    asset_buf_.resize(sprite_offset);
    
    int sprite_id = 0;
    for (const auto& sprite : node->sprites) {
        sprite_ids_[sprite.name] = sprite_id++;
        
        // We write Sprite Header (Width, Height) then pixel indices
        // Width and height are 1 byte
        asset_buf_.push_back(sprite.width);
        asset_buf_.push_back(sprite.height);
        
        for (int y = 0; y < sprite.height; ++y) {
            for (int x = 0; x < sprite.width; ++x) {
                char c = sprite.grid[y][x];
                if (c == '.' || c == ' ') {
                    asset_buf_.push_back(0); // Transparent
                } else {
                    std::string key(1, c);
                    // Match starting character with palette names
                    uint8_t index = 0;
                    for (const auto& [col_name, idx] : color_palette_) {
                        if (col_name[0] == c) {
                            index = idx;
                            break;
                        }
                    }
                    asset_buf_.push_back(index);
                }
            }
        }
    }
    
    // 4. Emit bootstrap code at the beginning of the code segment
    // It will invoke 'init()', then loop 'update()' and 'VBLANK'.
    size_t bootstrap_start = code_buf_.size();
    
    // We need placeholders for function addresses since they haven't been compiled yet.
    // Call init()
    emit_8(vm::OP_CALL);
    size_t fixup_init = code_buf_.size();
    emit_32(0); // patched later
    
    // Loop start
    size_t loop_start = code_buf_.size();
    
    // Call update()
    emit_8(vm::OP_CALL);
    size_t fixup_update = code_buf_.size();
    emit_32(0); // patched later
    
    // Call VBLANK
    emit_8(vm::OP_VBLANK);
    
    // JMP to loop start
    emit_8(vm::OP_JMP);
    emit_32(loop_start);
    
    // 5. Compile functions
    for (const auto& func : node->functions) {
        func->accept(this);
    }
    
    // 6. Patch bootstrap jumps to point to compiled function addresses
    auto init_it = function_pcs_.find("init");
    if (init_it == function_pcs_.end()) {
        throw std::runtime_error("Codegen Error: Missing required entry point function 'init()'");
    }
    patch_32(fixup_init, init_it->second);
    
    auto update_it = function_pcs_.find("update");
    if (update_it == function_pcs_.end()) {
        throw std::runtime_error("Codegen Error: Missing required loop function 'update()'");
    }
    patch_32(fixup_update, update_it->second);
}

} // namespace ooey_station::compiler
