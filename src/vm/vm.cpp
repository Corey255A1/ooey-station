#include "vm.hpp"
#include "opcodes.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace ooey_station::vm {

BooeyVM::BooeyVM() {
    ram_.resize(64 * 1024, 0);   // 64 KB
    vram_.resize(32 * 1024, 0);  // 32 KB
    aram_.resize(16 * 1024, 0);  // 16 KB
    mmio_.resize(4 * 1024, 0);   // 4 KB
}

void BooeyVM::reset() {
    std::fill(ram_.begin(), ram_.end(), 0);
    std::fill(vram_.begin(), vram_.end(), 0);
    std::fill(aram_.begin(), aram_.end(), 0);
    std::fill(mmio_.begin(), mmio_.end(), 0);
    
    registers_.fill(0);
    pc_ = entry_point_;
    
    flag_z_ = false;
    flag_n_ = false;
    flag_c_ = false;
    
    call_stack_.clear();
    halted_ = false;
    exited_ = false;
    wait_for_vblank_ = false;
    error_msg_.clear();
}

bool BooeyVM::load_program(const std::vector<uint8_t>& binary) {
    if (binary.size() < 32) {
        std::cerr << "Binary too small to have a valid header" << std::endl;
        return false;
    }

    // Parse header
    // Magic check: 'BOOE' (0x454F4F42)
    uint32_t magic = *reinterpret_cast<const uint32_t*>(&binary[0]);
    if (magic != 0x454F4F42) {
        std::cerr << "Invalid binary magic number: expected 0x454F4F42, got " << std::hex << magic << std::endl;
        return false;
    }

    uint16_t version = *reinterpret_cast<const uint16_t*>(&binary[4]);
    if (version != 0x0001) {
        std::cerr << "Unsupported VM version: " << version << std::endl;
        return false;
    }

    uint32_t code_size = *reinterpret_cast<const uint32_t*>(&binary[8]);
    uint32_t data_size = *reinterpret_cast<const uint32_t*>(&binary[12]);
    uint32_t asset_size = *reinterpret_cast<const uint32_t*>(&binary[16]);
    entry_point_ = *reinterpret_cast<const uint32_t*>(&binary[20]);

    if (binary.size() < 32 + code_size + data_size + asset_size) {
        std::cerr << "Binary payload size doesn't match header specifications" << std::endl;
        return false;
    }

    // Load segments
    code_.assign(binary.begin() + 32, binary.begin() + 32 + code_size);
    static_data_.assign(binary.begin() + 32 + code_size, binary.begin() + 32 + code_size + data_size);
    assets_.assign(binary.begin() + 32 + code_size + data_size, binary.end());

    // Copy initial static data into Game RAM (starting at 0x0000)
    size_t copy_size = std::min(static_data_.size(), ram_.size());
    std::memcpy(ram_.data(), static_data_.data(), copy_size);

    // Copy assets directly to VRAM (starting at 0x0000 of VRAM, i.e. 0x10000 address space)
    size_t asset_copy_size = std::min(assets_.size(), vram_.size());
    std::memcpy(vram_.data(), assets_.data(), asset_copy_size);

    reset();
    return true;
}

uint32_t BooeyVM::read_memory_32(uint32_t addr) {
    if (addr + 3 >= 0x20000) {
        trigger_error("Out of bounds 32-bit read: " + std::to_string(addr));
        return 0;
    }
    
    // RAM
    if (addr < 0x10000) {
        return *reinterpret_cast<uint32_t*>(&ram_[addr]);
    }
    // VRAM
    else if (addr < 0x18000) {
        return *reinterpret_cast<uint32_t*>(&vram_[addr - 0x10000]);
    }
    // ARAM
    else if (addr < 0x1C000) {
        return *reinterpret_cast<uint32_t*>(&aram_[addr - 0x18000]);
    }
    // MMIO
    else if (addr < 0x1D000) {
        return *reinterpret_cast<uint32_t*>(&mmio_[addr - 0x1C000]);
    }
    
    trigger_error("Attempted read from reserved memory space: " + std::to_string(addr));
    return 0;
}

void BooeyVM::write_memory_32(uint32_t addr, uint32_t val) {
    if (addr + 3 >= 0x20000) {
        trigger_error("Out of bounds 32-bit write: " + std::to_string(addr));
        return;
    }
    
    // RAM
    if (addr < 0x10000) {
        *reinterpret_cast<uint32_t*>(&ram_[addr]) = val;
    }
    // VRAM
    else if (addr < 0x18000) {
        *reinterpret_cast<uint32_t*>(&vram_[addr - 0x10000]) = val;
    }
    // ARAM
    else if (addr < 0x1C000) {
        *reinterpret_cast<uint32_t*>(&aram_[addr - 0x18000]) = val;
    }
    // MMIO
    else if (addr < 0x1D000) {
        *reinterpret_cast<uint32_t*>(&mmio_[addr - 0x1C000]) = val;
    }
    else {
        trigger_error("Attempted write to reserved memory space: " + std::to_string(addr));
    }
}

uint8_t BooeyVM::read_memory_8(uint32_t addr) {
    if (addr >= 0x20000) {
        trigger_error("Out of bounds 8-bit read: " + std::to_string(addr));
        return 0;
    }
    
    if (addr < 0x10000) {
        return ram_[addr];
    }
    else if (addr < 0x18000) {
        return vram_[addr - 0x10000];
    }
    else if (addr < 0x1C000) {
        return aram_[addr - 0x18000];
    }
    else if (addr < 0x1D000) {
        return mmio_[addr - 0x1C000];
    }
    
    trigger_error("Attempted read from reserved memory space: " + std::to_string(addr));
    return 0;
}

void BooeyVM::write_memory_8(uint32_t addr, uint8_t val) {
    if (addr >= 0x20000) {
        trigger_error("Out of bounds 8-bit write: " + std::to_string(addr));
        return;
    }
    
    if (addr < 0x10000) {
        ram_[addr] = val;
    }
    else if (addr < 0x18000) {
        vram_[addr - 0x10000] = val;
    }
    else if (addr < 0x1C000) {
        aram_[addr - 0x18000] = val;
    }
    else if (addr < 0x1D000) {
        mmio_[addr - 0x1C000] = val;
    }
    else {
        trigger_error("Attempted write to reserved memory space: " + std::to_string(addr));
    }
}

void BooeyVM::trigger_error(const std::string& msg) {
    halted_ = true;
    error_msg_ = msg;
    std::cerr << "VM Error: " << msg << " [PC=" << pc_ << "]" << std::endl;
}

void BooeyVM::run_frame() {
    wait_for_vblank_ = false;
    while (!halted_ && !wait_for_vblank_ && !exited_) {
        execute_instruction();
    }
}

void BooeyVM::execute_instruction() {
    if (pc_ >= code_.size()) {
        trigger_error("PC out of code segment bounds");
        return;
    }

    uint8_t opcode = code_[pc_++];
    
    switch (opcode) {
        case OP_NOP:
            break;
            
        case OP_MOV: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] = registers_[rs];
            break;
        }
        
        case OP_MOVI: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint32_t val = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            registers_[rd] = val;
            break;
        }
        
        case OP_ADD: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            uint64_t res = static_cast<uint64_t>(registers_[rd]) + registers_[rs];
            registers_[rd] = static_cast<uint32_t>(res);
            flag_c_ = (res > 0xFFFFFFFF);
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_SUB: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            uint64_t res = static_cast<uint64_t>(registers_[rd]) - registers_[rs];
            registers_[rd] = static_cast<uint32_t>(res);
            flag_c_ = (res > 0xFFFFFFFF); // Borrow in standard subtraction
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_MUL: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] = registers_[rd] * registers_[rs];
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_DIV: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            if (registers_[rs] == 0) {
                trigger_error("Division by zero");
                return;
            }
            registers_[rd] = registers_[rd] / registers_[rs];
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_MOD: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            if (registers_[rs] == 0) {
                trigger_error("Modulo by zero");
                return;
            }
            registers_[rd] = registers_[rd] % registers_[rs];
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_AND: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] &= registers_[rs];
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_OR: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] |= registers_[rs];
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_XOR: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] ^= registers_[rs];
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_NOT: {
            uint8_t rd = code_[pc_++] & 0xF;
            registers_[rd] = ~registers_[rd];
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_SHL: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] <<= (registers_[rs] & 31);
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_SHR: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] >>= (registers_[rs] & 31);
            flag_z_ = (registers_[rd] == 0);
            flag_n_ = (static_cast<int32_t>(registers_[rd]) < 0);
            break;
        }
        
        case OP_CMP: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            int32_t val1 = static_cast<int32_t>(registers_[rd]);
            int32_t val2 = static_cast<int32_t>(registers_[rs]);
            flag_z_ = (val1 == val2);
            flag_n_ = (val1 < val2);
            break;
        }
        
        case OP_LOAD: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            registers_[rd] = read_memory_32(addr);
            break;
        }
        
        case OP_STORE: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            uint8_t rs = code_[pc_++] & 0xF;
            write_memory_32(addr, registers_[rs]);
            break;
        }
        
        case OP_LOADR: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            registers_[rd] = read_memory_32(registers_[rs]);
            break;
        }
        
        case OP_STORER: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            write_memory_32(registers_[rd], registers_[rs]);
            break;
        }
        
        case OP_LOADB: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            registers_[rd] = read_memory_8(addr);
            break;
        }
        
        case OP_STOREB: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            uint8_t rs = code_[pc_++] & 0xF;
            write_memory_8(addr, static_cast<uint8_t>(registers_[rs] & 0xFF));
            break;
        }
        
        case OP_JMP: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ = addr;
            break;
        }
        
        case OP_JZ: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            if (flag_z_) pc_ = addr;
            break;
        }
        
        case OP_JNZ: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            if (!flag_z_) pc_ = addr;
            break;
        }
        
        case OP_JL: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            if (flag_n_) pc_ = addr;
            break;
        }
        
        case OP_JG: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            if (!flag_n_ && !flag_z_) pc_ = addr;
            break;
        }
        
        case OP_CALL: {
            uint32_t addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            call_stack_.push_back(pc_);
            pc_ = addr;
            break;
        }
        
        case OP_RET: {
            if (call_stack_.empty()) {
                trigger_error("Call stack underflow on RET");
                return;
            }
            pc_ = call_stack_.back();
            call_stack_.pop_back();
            break;
        }
        
        case OP_HALT: {
            halted_ = true;
            break;
        }
        
        case OP_CLS: {
            uint8_t rs = code_[pc_++] & 0xF;
            if (clear_callback_) clear_callback_(registers_[rs]);
            break;
        }
        
        case OP_PIXEL: {
            uint8_t rx = code_[pc_++] & 0xF;
            uint8_t ry = code_[pc_++] & 0xF;
            uint8_t rc = code_[pc_++] & 0xF;
            if (draw_pixel_callback_) draw_pixel_callback_(registers_[rx], registers_[ry], registers_[rc]);
            break;
        }
        
        case OP_LINE: {
            uint8_t rx1 = code_[pc_++] & 0xF;
            uint8_t ry1 = code_[pc_++] & 0xF;
            uint8_t rx2 = code_[pc_++] & 0xF;
            uint8_t ry2 = code_[pc_++] & 0xF;
            uint8_t rc  = code_[pc_++] & 0xF;
            if (draw_line_callback_) draw_line_callback_(registers_[rx1], registers_[ry1], registers_[rx2], registers_[ry2], registers_[rc]);
            break;
        }
        
        case OP_RECT: {
            uint8_t rx = code_[pc_++] & 0xF;
            uint8_t ry = code_[pc_++] & 0xF;
            uint8_t rw = code_[pc_++] & 0xF;
            uint8_t rh = code_[pc_++] & 0xF;
            uint8_t rc = code_[pc_++] & 0xF;
            if (draw_rect_callback_) draw_rect_callback_(registers_[rx], registers_[ry], registers_[rw], registers_[rh], registers_[rc], false);
            break;
        }
        
        case OP_FRECT: {
            uint8_t rx = code_[pc_++] & 0xF;
            uint8_t ry = code_[pc_++] & 0xF;
            uint8_t rw = code_[pc_++] & 0xF;
            uint8_t rh = code_[pc_++] & 0xF;
            uint8_t rc = code_[pc_++] & 0xF;
            if (draw_rect_callback_) draw_rect_callback_(registers_[rx], registers_[ry], registers_[rw], registers_[rh], registers_[rc], true);
            break;
        }
        
        case OP_TEXT: {
            uint8_t rx = code_[pc_++] & 0xF;
            uint8_t ry = code_[pc_++] & 0xF;
            uint32_t str_addr = *reinterpret_cast<uint32_t*>(&code_[pc_]);
            pc_ += 4;
            uint8_t rc = code_[pc_++] & 0xF;
            if (draw_text_callback_) draw_text_callback_(registers_[rx], registers_[ry], str_addr, registers_[rc]);
            break;
        }
        
        case OP_VBLANK: {
            wait_for_vblank_ = true;
            break;
        }
        
        case OP_SPR: {
            uint8_t rid = code_[pc_++] & 0xF;
            uint8_t rx = code_[pc_++] & 0xF;
            uint8_t ry = code_[pc_++] & 0xF;
            if (draw_sprite_callback_) draw_sprite_callback_(registers_[rid], registers_[rx], registers_[ry], 0);
            break;
        }
        
        case OP_SPREX: {
            uint8_t rid = code_[pc_++] & 0xF;
            uint8_t rx = code_[pc_++] & 0xF;
            uint8_t ry = code_[pc_++] & 0xF;
            uint8_t rflags = code_[pc_++] & 0xF;
            if (draw_sprite_callback_) draw_sprite_callback_(registers_[rid], registers_[rx], registers_[ry], registers_[rflags]);
            break;
        }
        
        case OP_BTNP: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rbtn = code_[pc_++] & 0xF;
            // MMIO offset 0x1C004 holds the "pressed" state bitmask
            uint32_t pressed_mask = read_memory_32(0x1C004);
            registers_[rd] = (pressed_mask & (1 << registers_[rbtn])) ? 1 : 0;
            break;
        }
        
        case OP_BTNH: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rbtn = code_[pc_++] & 0xF;
            // MMIO offset 0x1C000 holds the "held" state bitmask
            uint32_t held_mask = read_memory_32(0x1C000);
            registers_[rd] = (held_mask & (1 << registers_[rbtn])) ? 1 : 0;
            break;
        }
        
        case OP_RND: {
            uint8_t rd = code_[pc_++] & 0xF;
            uint8_t rs = code_[pc_++] & 0xF;
            uint32_t max_val = registers_[rs];
            registers_[rd] = (max_val == 0) ? 0 : (std::rand() % max_val);
            break;
        }
        
        case OP_EXIT: {
            exited_ = true;
            break;
        }

        default:
            trigger_error("Unknown opcode: " + std::to_string(opcode));
            break;
    }
}

} // namespace ooey_station::vm
