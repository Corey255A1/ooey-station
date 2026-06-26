#pragma once

#include <cstdint>

namespace ooey_station::vm {

enum Opcode : uint8_t {
    OP_NOP      = 0x00,
    OP_MOV      = 0x01,
    OP_MOVI     = 0x02,
    
    OP_ADD      = 0x10,
    OP_SUB      = 0x11,
    OP_MUL      = 0x12,
    OP_DIV      = 0x13,
    OP_MOD      = 0x14,
    OP_AND      = 0x15,
    OP_OR       = 0x16,
    OP_XOR      = 0x17,
    OP_NOT      = 0x18,
    OP_SHL      = 0x19,
    OP_SHR      = 0x1A,
    OP_CMP      = 0x1B,
    
    OP_LOAD     = 0x20,
    OP_STORE    = 0x21,
    OP_LOADR    = 0x22,
    OP_STORER   = 0x23,
    OP_LOADB    = 0x24,
    OP_STOREB   = 0x25,
    
    OP_JMP      = 0x30,
    OP_JZ       = 0x31,
    OP_JNZ      = 0x32,
    OP_JL       = 0x33,
    OP_JG       = 0x34,
    OP_CALL     = 0x35,
    OP_RET      = 0x36,
    OP_HALT     = 0x37,
    
    OP_CLS      = 0x40,
    OP_PIXEL    = 0x41,
    OP_LINE     = 0x42,
    OP_RECT     = 0x43,
    OP_FRECT    = 0x44,
    OP_TEXT     = 0x45,
    OP_VBLANK   = 0x46,
    
    OP_SPR      = 0x50,
    OP_SPREX    = 0x51,
    OP_SCOL     = 0x52,
    OP_TILE     = 0x53,
    OP_TSCROLL  = 0x54,
    OP_TDRAW    = 0x55,
    
    OP_BTNP     = 0x60,
    OP_BTNH     = 0x61,
    OP_SFX      = 0x62,
    OP_PLAY     = 0x63,
    OP_BTNR     = 0x64,
    
    OP_RND      = 0x70,
    OP_SIN      = 0x71,
    OP_DIST     = 0x72,
    OP_FRAME    = 0x73,
    OP_EXIT     = 0x74,
    OP_COS      = 0x75,
    OP_ATAN2    = 0x76,
    OP_ITOA     = 0x77
};

} // namespace ooey_station::vm
