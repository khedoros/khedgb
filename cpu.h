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

typedef uint64_t (cpu::*cpu_op)(int data);
#define CALL_MEMBER_FN(obj, ptr) ((obj).*(ptr))

class cpu {
public:
    cpu(memmap * bus, bool cgb_mode, bool has_firmware=false);
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
    //static const Arr<cpu_op, 256> cpu_ops;

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

public:
    static const cpu_op cpu_ops[256];
    uint64_t nop(int data);
    uint64_t ld_a16_sp(int data);
    uint64_t stop(int data);
    uint64_t jr_r8(int data);
    uint64_t jr_nz_r8(int data);
    uint64_t jr_z_r8(int data);
    uint64_t jr_nc_r8(int data);
    uint64_t jr_c_r8(int data);
    uint64_t ld_bc_d16(int data);
    uint64_t ld_de_d16(int data);
    uint64_t ld_hl_d16(int data);
    uint64_t ld_sp_d16(int data);
    uint64_t add_hl_bc(int data);
    uint64_t add_hl_de(int data);
    uint64_t add_hl_hl(int data);
    uint64_t add_hl_sp(int data);
    uint64_t ld_bcaddr_a(int data);
    uint64_t ld_a_bcaddr(int data);
    uint64_t ld_deaddr_a(int data);
    uint64_t ld_a_deaddr(int data);
    uint64_t ld_hlpaddr_a(int data);
    uint64_t ld_a_hlpaddr(int data);
    uint64_t ld_hlmaddr_a(int data);
    uint64_t ld_a_hlmaddr(int data);
    uint64_t inc_bc(int data);
    uint64_t dec_bc(int data);
    uint64_t inc_de(int data);
    uint64_t dec_de(int data);
    uint64_t inc_hl(int data);
    uint64_t dec_hl(int data);
    uint64_t inc_sp(int data);
    uint64_t dec_sp(int data);
    uint64_t inc_b(int data);
    uint64_t dec_b(int data);
    uint64_t inc_c(int data);
    uint64_t dec_c(int data);
    uint64_t inc_d(int data);
    uint64_t dec_d(int data);
    uint64_t inc_e(int data);
    uint64_t dec_e(int data);
    uint64_t inc_h(int data);
    uint64_t dec_h(int data);
    uint64_t inc_l(int data);
    uint64_t dec_l(int data);
    uint64_t inc_hladdr(int data);
    uint64_t dec_hladdr(int data);
    uint64_t inc_a(int data);
    uint64_t dec_a(int data);
    uint64_t ld_b_d8(int data);
    uint64_t ld_c_d8(int data);
    uint64_t ld_d_d8(int data);
    uint64_t ld_e_d8(int data);
    uint64_t ld_h_d8(int data);
    uint64_t ld_l_d8(int data);
    uint64_t ld_hladdr_d8(int data);
    uint64_t ld_a_d8(int data);
    uint64_t rlca(int data);
    uint64_t rrca(int data);
    uint64_t rla(int data);
    uint64_t rra(int data);
    uint64_t daa(int data);
    uint64_t cpl(int data);
    uint64_t scf(int data);
    uint64_t ccf(int data);
};
