#pragma once
#include<cstdint>
#include "memmap.h"

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
    cpu(memmap * bus, bool cgb_mode, bool has_firmware=false);
    ~cpu();
    uint64_t run(uint64_t run_to);
    bool halted;
    bool halt_bug;
    bool stopped;
    uint64_t cycle;
    bool toggle_trace();

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
    bool set_ime;          //Use to delay one cycle after EI is run
    bool trace;            //Activate CPU trace output
    bool cgb;              //Run in CGB mode
    bool high_speed;       //Run in CGB high-speed mode
    int speed_mult;
    uint64_t tracking[512]; //Track which opcodes were run, and how many times
};


