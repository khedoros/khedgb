#pragma once
#include<cstdint>
#include "memmap.h"

//CPU status flags
#define ZERO_FLAG       0x80 //Set when result is 9
#define SUB_FLAG        0x40 //Set when instruction is a subtraction, cleared otherwise
#define HALF_CARRY_FLAG 0x20 //Set upon carry from bit3
#define CARRY_FLAG      0x10 //Set upon carry from bit7

#define set(f) (af.low |= f)
#define clear(f) (af.low &= (~f))
#define zero() ((af.low & ZERO_FLAG)>>(7))
#define sub() ((af.low & SUB_FLAG)>>(6))
#define hc() ((af.low & HALF_CARRY_FLAG)>>(5))
#define carry() ((af.low & CARRY_FLAG)>>(4))

#define extend(x) ( (x>0x7f) ? 0xffffff00|x : x)

#define BIT0 0x1
#define BIT1 0x2
#define BIT2 0x4
#define BIT3 0x8
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80

class cpu {
    public:
    cpu(memmap& bus, bool has_firmware=false);
    int run();
    int dec_and_exe(uint32_t opcode);
    int execute(int pre,int x,int y,int z,int data);
    void registers();
    struct regpair {
        union {
            struct {
                uint8_t low;
                uint8_t hi;
            };
            uint16_t pair;
        };
    };
    memmap bus;
    regpair af, bc, de, hl;
    uint8_t dummy;
    uint16_t sp, pc;
    static const uint8_t op_bytes[256];
    static const uint8_t op_times[256];
    static const uint8_t op_times_extra[256];

    uint8_t * const r[8];
    uint16_t * const rp[4];
    uint16_t * const rp2[4];
};
