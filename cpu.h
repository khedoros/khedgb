#pragma once
#include<cstdint>
#include "memmap.h"

//CPU status flags
#define ZERO_FLAG       0x80 //Set when result is 9
#define SUB_FLAG        0x40 //Set when instruction is a subtraction, cleared otherwise
#define HALF_CARRY_FLAG 0x20 //Set upon carry from bit3
#define CARRY_FLAG      0x10 //Set upon carry from bit7

class cpu {
    public:
    cpu(memmap& bus, bool has_firmware=false);
    int run();
    int dec_and_exe(uint32_t opcode);
    struct regpair {
        union {
            struct {
                unsigned low:8;
                unsigned hi:8;
            };
            unsigned pair:16;
        };
    };
    memmap bus;
    regpair af, bc, de, hl;
    uint16_t sp, pc;
    static const uint8_t op_bytes[256];
    static const uint8_t op_times[256];
};
