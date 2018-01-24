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

#define VBL_INT_ADDR 0x0040
#define LCD_INT_ADDR 0x0048
#define TIM_INT_ADDR 0x0050
#define SER_INT_ADDR 0x0058
#define JOY_INT_ADDR 0x0060

/*
 * RST instructions: 0,1,2,3,4,5,6,7 go to 0000, 0008, 0010, 0018, 0020, 0028, 0030, and 0038
 * interrupts are at: 0040: VBlank, which begins at line 144
 *                    0048: LCD status interrupts (you can enable several different ones)
 *                    0050: Timer interrupt  (bit 2 of IF register)
 *                    0058: Serial interrupt (bit 3 of IF register)
 *                    0060: Joypad interrupt (bit 4 of IF register)
 */

class cpu {
public:
    cpu(memmap * bus, bool has_firmware=false);
    uint64_t run(uint64_t run_to);
    bool halted;
    bool stopped;
    uint64_t cycle;
    uint64_t frame;


private:
    uint64_t dec_and_exe(uint32_t opcode);
    uint64_t execute(int pre,int x,int y,int z,int data);
    void registers();

    INT_TYPE check_interrupts();
    bool call_interrupts(); //Returns true if an interrupt is called

    struct regpair {
        union {
            struct {
                uint8_t low;
                uint8_t hi;
            };
            uint16_t pair;
        };
    };
    memmap * bus;
    regpair af, bc, de, hl;
    uint8_t dummy;
    uint16_t sp, pc;
    static const uint8_t op_bytes[256];
    static const uint8_t op_times[256];
    static const uint8_t op_times_extra[256];

    uint8_t * const r[8];
    uint16_t * const rp[4];
    uint16_t * const rp2[4];
    bool interrupts;       //Interrupt Master Enable
};
