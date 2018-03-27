//SHARP LR35902.
//4MHz (4194304Hz) 
//The CPU is paired with memory clocked at 1MHz, so the speed is bound by the memory clock.

#include<iostream>
#include<cassert>
#include "cpu.h"


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

//Interrupt addresses
#define VBL_INT_ADDR 0x0040
#define LCD_INT_ADDR 0x0048
#define TIM_INT_ADDR 0x0050
#define SER_INT_ADDR 0x0058
#define JOY_INT_ADDR 0x0060

#ifdef DEBUG
#define APRINTF printf
#define ASSERT assert
#else
#define APRINTF //printf
#define ASSERT //assert
#endif

void decode(int pre,int x,int y,int z,int data);

cpu::cpu(memmap * b, bool cgb_mode, bool has_firmware/*=false*/): bus(b),
        r{&bc.hi, &bc.low, &de.hi, &de.low, &hl.hi, &hl.low, &dummy, &af.hi},
        rp{&bc.pair, &de.pair, &hl.pair, &sp},
        rp2{&bc.pair, &de.pair, &hl.pair, &af.pair},
        cgb(cgb_mode), high_speed(false)
{
    trace = false;
    interrupts = false;
    set_ime = false;
    halted = false;
    halt_bug = false;
    stopped = false;
    speed_mult = 2;
    cycle = 0;//16416; //Start in VBlank
    af.pair=0;
    bc.pair=0;
    de.pair=0;
    hl.pair=0; 
    sp=0;
    if(has_firmware) {
        pc = 0;
    }
    else if(!cgb){
        pc = 0x100;
        sp = 0xfffe;
        af.pair = 0x01b0;
        bc.pair = 0x0013;
        de.pair = 0x00d8;
        hl.pair = 0x014d;
    }
    else {
        pc = 0x100;
        sp = 0xfffe;
        af.pair = 0x11b0;
        bc.pair = 0x0000;
        de.pair = 0xff56;
        hl.pair = 0x000d;
    }
}

uint64_t cpu::run(uint64_t run_to) {
    ASSERT(cycle/2 < run_to);
    uint32_t opcode=0;
    bool running=true;
    uint64_t cycles=0;
    while(running) {
        opcode = bus->readmore(pc, 3, cycle);
        cycles = speed_mult * dec_and_exe(opcode);
        if(!cycles) {
            std::cout<<"No idea what to do with opcode "<<std::hex<<int(opcode)<<"."<<std::endl;
            running = false;
            return 0;
        }
        cycle+=cycles;
        //114 CPU cycles per line, 154 lines per frame. CPU runs at 1024*1024 Hz, gives a framerate around 59.7Hz.
        if(cycle/2 >= run_to) {
            bus->update_interrupts(cycle);
            return cycle/2;
        }
    }
    return 0; //shouldn't reach this, as currently written
}

uint64_t cpu::dec_and_exe(uint32_t opcode) {
    //"cycles" is the time that the CPU spent running this operation
    //"retval" represents cycles too, but means that the function should return before executing any opcodes
    uint64_t cycles = 0;
    uint64_t retval = 0;
    //Poll interrupts
    uint8_t int_flag = 0;
    uint8_t int_enable = 0;

    bus->update_interrupts(cycle);
    int_flag = bus->iflag();
    int_enable = bus->ienab();
    if(interrupts && !stopped) { //IME
        bool was_halted = false; //Exiting halt when jumping to an interrupt takes an extra cycle
        if(halted && (int_flag & int_enable & 0x1f) > 0) {
            halted = false;
            was_halted = true;
            //printf("YO: exiting halt, int_flag: %02x int_enable: %02x\n", int_flag, int_enable);
        }
        bool called = call_interrupts();
        if(called) {
            retval += 5;
            if(was_halted) {
                was_halted = false;
                retval++;
                //printf("YO: Exited halt due to interrupt call, expect to execute: %04x\n", pc);
            }
        }
    }
    //Interrupts disabled, interrupts are *pending*, but HALT was activated
    else if(halted && halt_bug) { //HALT bug: PC is stuck for one instruction after exiting halt mode
        //printf("HALT: opcode: %08x\n", opcode);
        //printf("YO: Halt bug condition.\n");
        halted = false;
        halt_bug = false;
        uint32_t op2 = ((opcode & 0xffff00)>>8);
        opcode &= 0xff; //Grab the next byte after HALT
        opcode = opcode | (opcode<<8) | (op2<<16); //Use that byte as the next instruction and its arguments, since PC is stuck
        uint64_t time = dec_and_exe(opcode); //run the wonky operation
        pc--; //back pc up by one, because it was "stuck"
        retval = time + 1; //CPU takes 4 clocks (1 CPU clock) to exit halt
    } 
    //IME=0, and exiting halt mode due to a new interrupt triggering
    else if(halted && ((int_flag & int_enable) & 0x1f) > 0) {
        //printf("YO: exited halt due to interrupt (if=%02x, ie=%02x, IME=%d), but IME=0\n", int_flag, int_enable, interrupts);
        halted = false;
        cycles++;
    }
    //IME=0, and exiting stop mode due to joypad input
    else if(stopped && (int_flag & JOYPAD) > 0) {
        stopped = false;
    }
    //Staying in halted or stopped mode for another cycle
    else if(halted || stopped) {
        retval = 1;
    }

    //Next instruction after EI is run before possibly jumping into interrupts
    if(set_ime) {
        interrupts = true;
        set_ime = false;
    }

    if(retval) {
        return retval+cycles;
    }
 
    //Sleep for necessary time, if HDMA is in progress
    uint64_t prev = cycles;
    cycles+=bus->handle_hdma(cycle);
    if(cycles - prev > 0) {
        //printf("Reported sleeping for %d cycles\n", cycles - prev);
        return cycles;
    }
   
    int bytes = 0;
    int prefix = 0;
    int data = 0;
    //std::cout<<std::hex<<opcode<<"\t";
    uint32_t op = 0;
    op = opcode & 0xff;
    if(op == 0xcb) {
        prefix = 0xcb;
        op = ((opcode&0xFF00)>>(8));
        cycles += ((op&0x7) == 6) ? 4 : 2;
        if((op&7)==6 && (op&0xc0)==0x40) {
            cycles--;
        }
        bytes = 2;
    }
    else {
        bytes = op_bytes[op];
        cycles += op_times[op];
    }

    int x = (op&0xc0)>>(6); // 11000000
    int y = (op&0x38)>>(3); // 00111000
    int z = (op&0x7);       // 00000111
    
    if(bytes == 2 && !prefix) {
        data = (opcode & 0xff00)>>(8);
    }
    else if(bytes == 3) {
        data = (opcode & 0xffff00)>>(8);
    }

    //Print a CPU trace
    if(trace) {
        registers();
        printf("\t%02x\t",op);
        decode(prefix,x,y,z,data);
        if(halted || stopped) {
                printf(" (not executed)\n");
        }
        else {
                printf("\n");
        }
    }

    if(!halted && !stopped) {//If the CPU hits a HALT or STOP, it needs to stay there.
        pc += bytes;
        //Branches and such need extra time, so execute returns any extra time that was necessary
        cycles += execute(prefix,x,y,z,data);
    }

    return cycles;
}

const cpu_op cpu::cpu_ops[256] = {&cpu::nop,      &cpu::ld_bc_d16,&cpu::ld_bcaddr_a, &cpu::inc_bc,&cpu::inc_b,     &cpu::dec_b,     &cpu::ld_b_d8,     &cpu::rlca,
                                  &cpu::ld_a16_sp,&cpu::add_hl_bc,&cpu::ld_a_bcaddr, &cpu::dec_bc,&cpu::inc_c,     &cpu::dec_c,     &cpu::ld_c_d8,     &cpu::rrca,
                                  &cpu::stop,     &cpu::ld_de_d16,&cpu::ld_deaddr_a, &cpu::inc_de,&cpu::inc_d,     &cpu::dec_d,     &cpu::ld_d_d8,     &cpu::rla,
                                  &cpu::jr_r8,    &cpu::add_hl_de,&cpu::ld_a_deaddr, &cpu::dec_de,&cpu::inc_e,     &cpu::dec_e,     &cpu::ld_e_d8,     &cpu::rra,
                                  &cpu::jr_nz_r8, &cpu::ld_hl_d16,&cpu::ld_hlpaddr_a,&cpu::inc_hl,&cpu::inc_h,     &cpu::dec_h,     &cpu::ld_h_d8,     &cpu::daa,
                                  &cpu::jr_z_r8,  &cpu::add_hl_hl,&cpu::ld_a_hlpaddr,&cpu::dec_hl,&cpu::inc_l,     &cpu::dec_l,     &cpu::ld_l_d8,     &cpu::cpl,
                                  &cpu::jr_nc_r8, &cpu::ld_sp_d16,&cpu::ld_hlmaddr_a,&cpu::inc_sp,&cpu::inc_hladdr,&cpu::dec_hladdr,&cpu::ld_hladdr_d8,&cpu::scf,
                                  &cpu::jr_c_r8,  &cpu::add_hl_sp,&cpu::ld_a_hlmaddr,&cpu::dec_sp,&cpu::inc_a,     &cpu::dec_a,     &cpu::ld_a_d8,     &cpu::ccf,

                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop,
                                  &cpu::nop,      &cpu::nop,      &cpu::nop,         &cpu::nop,   &cpu::nop,       &cpu::nop,       &cpu::nop,         &cpu::nop};

uint64_t cpu::nop(int data) {return 0;}
uint64_t cpu::ld_a16_sp(int data) {
    bus->writemore(data, sp, 2, cycle+2);
    return 0;
}
uint64_t cpu::stop(int data) {
    if(cgb && bus->feel_the_need) {
        bus->speed_switch();
        if(high_speed) {
            high_speed = false;
            speed_mult = 2;
            return 32;
        }
        else {
            high_speed = true;
            speed_mult = 1;
            return 16;
        }
    }
    else {
        stopped = true;
    }
    return 0;
}
uint64_t cpu::jr_r8(int data) {
    data = extend(data);
    pc += data;
    return 0;
}

uint64_t cpu::jr_nz_r8(int data) {
    data = extend(data);
    if(!zero()) {
        pc += data;
        return 1;
    }
    return 0;
}
uint64_t cpu::jr_z_r8(int data) {
    data = extend(data);
    if(zero()) {
        pc += data;
        return 1;
    }
    return 0;
}
uint64_t cpu::jr_nc_r8(int data) {
    data = extend(data);
    if(!carry()) {
        pc += data;
        return 1;
    }
    return 0;
}
uint64_t cpu::jr_c_r8(int data) {
    data = extend(data);
    if(carry()) {
        pc += data;
        return 1;
    }
    return 0;
}
uint64_t cpu::ld_bc_d16(int data) {
    bc.pair = data;
    return 0;
}
uint64_t cpu::ld_de_d16(int data) {
    de.pair = data;
    return 0;
}
uint64_t cpu::ld_hl_d16(int data) {
    hl.pair = data;
    return 0;
}
uint64_t cpu::ld_sp_d16(int data) {
    sp = data;
    return 0;
}

uint64_t cpu::add_hl_bc(int data) {
    if((hl.pair & 0xfff) + (bc.pair & 0xfff) >= 0x1000) {
        set(HALF_CARRY_FLAG);
    }
    else {
        clear(HALF_CARRY_FLAG);
    }
    if(uint32_t(uint32_t(hl.pair) + uint32_t(bc.pair)) >= 0x10000) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    hl.pair += bc.pair;
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::add_hl_de(int data) {
    if((hl.pair & 0xfff) + (de.pair & 0xfff) >= 0x1000) {
        set(HALF_CARRY_FLAG);
    }
    else {
        clear(HALF_CARRY_FLAG);
    }
    if(uint32_t(uint32_t(hl.pair) + uint32_t(de.pair)) >= 0x10000) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    hl.pair += de.pair;
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::add_hl_hl(int data) {
    if((hl.pair & 0xfff) + (hl.pair & 0xfff) >= 0x1000) {
        set(HALF_CARRY_FLAG);
    }
    else {
        clear(HALF_CARRY_FLAG);
    }
    if(uint32_t(uint32_t(hl.pair) + uint32_t(hl.pair)) >= 0x10000) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    hl.pair += hl.pair;
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::add_hl_sp(int data) {
    if((hl.pair & 0xfff) + (sp & 0xfff) >= 0x1000) {
        set(HALF_CARRY_FLAG);
    }
    else {
        clear(HALF_CARRY_FLAG);
    }
    if(uint32_t(uint32_t(hl.pair) + uint32_t(sp)) >= 0x10000) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    hl.pair += sp;
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::ld_bcaddr_a(int data) {
    bus->write(bc.pair, af.hi, cycle+1);
    return 0;
}
uint64_t cpu::ld_a_bcaddr(int data) {
    af.hi = bus->read(bc.pair, cycle+1);
    return 0;
}
uint64_t cpu::ld_deaddr_a(int data) {
    bus->write(de.pair, af.hi, cycle+1);
    return 0;
}
uint64_t cpu::ld_a_deaddr(int data) {
    af.hi = bus->read(de.pair, cycle+1);
    return 0;
}
uint64_t cpu::ld_hlpaddr_a(int data) {
    bus->write(hl.pair, af.hi, cycle+1);
    hl.pair++;
    return 0;
}
uint64_t cpu::ld_a_hlpaddr(int data) {
    af.hi = bus->read(hl.pair, cycle+1);
    hl.pair++;
    return 0;
}
uint64_t cpu::ld_hlmaddr_a(int data) {
    bus->write(hl.pair, af.hi, cycle+1);
    hl.pair--;
    return 0;
}
uint64_t cpu::ld_a_hlmaddr(int data) {
    af.hi = bus->read(hl.pair, cycle+1);
    hl.pair--;
    return 0;
}
uint64_t cpu::inc_bc(int data) {
    bc.pair++;
    return 0;
}
uint64_t cpu::dec_bc(int data) {
    bc.pair--;
    return 0;
}
uint64_t cpu::inc_de(int data) {
    de.pair++;
    return 0;
}
uint64_t cpu::dec_de(int data) {
    de.pair--;
    return 0;
}
uint64_t cpu::inc_hl(int data) {
    hl.pair++;
    return 0;
}
uint64_t cpu::dec_hl(int data) {
    hl.pair--;
    return 0;
}
uint64_t cpu::inc_sp(int data) {
    sp++;
    return 0;
}
uint64_t cpu::dec_sp(int data) {
    sp--;
    return 0;
}
uint64_t cpu::inc_b(int data) {
    bc.hi++;
    if(bc.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((bc.hi & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::dec_b(int data) {
    bc.hi--;
    if(bc.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((bc.hi & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    return 0;
}
uint64_t cpu::inc_c(int data) {
    bc.low++;
    if(bc.low == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((bc.low & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::dec_c(int data) {
    bc.low--;
    if(bc.low == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((bc.low & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    return 0;
}
uint64_t cpu::inc_d(int data) {
    de.hi++;
    if(de.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((de.hi & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::dec_d(int data) {
    de.hi--;
    if(de.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((de.hi & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    return 0;
}
uint64_t cpu::inc_e(int data) {
    de.low++;
    if(de.low == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((de.low & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::dec_e(int data) {
    de.low--;
    if(de.low == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((de.low & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    return 0;
}
uint64_t cpu::inc_h(int data) {
    hl.hi++;
    if(hl.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((hl.hi & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::dec_h(int data) {
    hl.hi--;
    if(hl.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((hl.hi & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    return 0;
}
uint64_t cpu::inc_l(int data) {
    hl.low++;
    if(hl.low == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((hl.low & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::dec_l(int data) {
    hl.low--;
    if(hl.low == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((hl.low & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    return 0;
}
uint64_t cpu::inc_hladdr(int data) {
    uint8_t dummy = bus->read(hl.pair, cycle+1); //0x34 (x=0,y=6,z=4) is a memory increment
    dummy++;
    if(dummy == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((dummy & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    bus->write(hl.pair, dummy, cycle+2); //0x34 (x=0,y=6,z=4) is a memory increment
    return 0;
}
uint64_t cpu::dec_hladdr(int data) {
    uint8_t dummy = bus->read(hl.pair, cycle+1); //0x34 (x=0,y=6,z=4) is a memory increment
    dummy--;
    if(dummy == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((dummy & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    bus->write(hl.pair, dummy, cycle+2); //0x34 (x=0,y=6,z=4) is a memory increment
    return 0;
}
uint64_t cpu::inc_a(int data) {
    af.hi++;
    if(af.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((af.hi & 0xf) == 0) set(HALF_CARRY_FLAG); //0xf+1=0x10, so the bottom nibble produced a carry during the increment
    else                     clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    return 0;
}
uint64_t cpu::dec_a(int data) {
    af.hi--;
    if(af.hi == 0) set(ZERO_FLAG);
    else             clear(ZERO_FLAG);
    if((af.hi & 0xf) == 0xf) set(HALF_CARRY_FLAG); //0x10-1=0x0f, so the bottom nibble needed to borrow during the decrement
    else                       clear(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    return 0;
}
uint64_t cpu::ld_b_d8(int data) {
    bc.hi = data;
    return 0;
}
uint64_t cpu::ld_c_d8(int data) {
    bc.low = data;
    return 0;
}
uint64_t cpu::ld_d_d8(int data) {
    de.hi = data;
    return 0;
}
uint64_t cpu::ld_e_d8(int data) {
    de.low = data;
    return 0;
}
uint64_t cpu::ld_h_d8(int data) {
    hl.hi = data;
    return 0;
}
uint64_t cpu::ld_l_d8(int data) {
    hl.low = data;
    return 0;
}
uint64_t cpu::ld_hladdr_d8(int data) {
    bus->write(hl.pair, data&0xff, cycle+2);
    return 0;
}
uint64_t cpu::ld_a_d8(int data) {
    af.hi = data;
    return 0;
}
uint64_t cpu::rlca(int data) {
    if(af.hi & BIT7) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    clear(ZERO_FLAG);
    clear(SUB_FLAG);
    clear(HALF_CARRY_FLAG);
    af.hi<<=(1);
    af.hi+=carry();
    return 0;
}
uint64_t cpu::rrca(int data) {
    if(af.hi & BIT0) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    clear(ZERO_FLAG);
    clear(SUB_FLAG);
    clear(HALF_CARRY_FLAG);
    af.hi>>=(1);
    af.hi+=carry()*BIT7;
    return 0;
}
uint64_t cpu::rla(int data) {
    data = carry();
    if(af.hi & BIT7) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    clear(ZERO_FLAG);
    clear(SUB_FLAG);
    clear(HALF_CARRY_FLAG);
    af.hi<<=(1);
    af.hi+=data;
    return 0;
}
uint64_t cpu::rra(int data) {
    data = carry();
    if(af.hi & BIT0) {
        set(CARRY_FLAG);
    }
    else {
        clear(CARRY_FLAG);
    }
    clear(ZERO_FLAG);
    clear(SUB_FLAG);
    clear(HALF_CARRY_FLAG);
    af.hi>>=(1);
    af.hi+=data*BIT7;
    return 0;
}
uint64_t cpu::daa(int data) {
    //Third version of this code, shamelessly grabbed from someone's nesdev.com post. 
    //Didn't pass Blargg until I fixed some other instructions, came back, and realized I'd screwed up my adaptation of the post.
    //This is one of the ones that's well-defined for expected inputs, and deviates from behavior of other CPUs for unexpected inputs.
    //
    // note: assumes a is a uint8_t and wraps from 0xff to 0
    if (!sub()) {  // after an addition, adjust if (half-)carry occurred or if result is out of bounds
        if (carry() || af.hi > 0x99) {
            af.hi += 0x60;
            set(CARRY_FLAG);
        }
        if (hc() || (af.hi & 0x0f) > 0x09) {
            af.hi += 0x6;
        }
    }
    else {  // after a subtraction, only adjust if (half-)carry occurred
        if (carry()) {
            af.hi -= 0x60;
            set(CARRY_FLAG);
        }
        if (hc()) {
            af.hi -= 0x6;
        }
    }
    // these flags are always updated
    if(af.hi) clear(ZERO_FLAG); // the usual z flag
    else set(ZERO_FLAG);
    clear(HALF_CARRY_FLAG); // h flag is always cleared

    //printf("DAA\n");
    return 0;
}
uint64_t cpu::cpl(int data) {
    set(HALF_CARRY_FLAG);
    set(SUB_FLAG);
    af.hi= ~(af.hi);
    //printf("CPL\n");
    return 0;
}
uint64_t cpu::scf(int data) {
    clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    set(CARRY_FLAG);
    //printf("SCF\n");
    return 0;
}
uint64_t cpu::ccf(int data) {
    clear(HALF_CARRY_FLAG);
    clear(SUB_FLAG);
    if(carry()) {
        clear(CARRY_FLAG);
    }
    else {
        set(CARRY_FLAG);
    }
    //printf("CCF\n");
    return 0;
}



uint64_t cpu::execute(int pre,int x,int y,int z,int data) {
    //char r_name[][5]  =    {     "B",      "C",   "D",      "E",   "H",   "L","(HL)",  "A"};
    //char rp_name[][3] =    {    "BC",     "DE",  "HL",     "SP"};
    //char rp2_name[][3]=    {    "BC",     "DE",  "HL",     "AF"};
    //char cc_name[][3] =    {    "NZ",      "Z",  "NC",      "C",  "PO",  "PE",   "P",  "M"};
    //char alu_name[][7]=    {"ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR",  "OR", "CP"};
    //char rot_name[][5]=    {   "RLC",    "RRC",  "RL",     "RR", "SLA", "SRA", "SWAP","SRL"};
    int p,q;
    p=y/2;
    q=y%2;
    uint64_t extra_cycles = 0;
    if(!pre && !x) {
        extra_cycles = CALL_MEMBER_FN(*this, cpu_ops[x<<6|y<<3|z])(data);
    }
    bool condition=false;
    if(!pre) {
        if(x==0) { //0x00 - 0x3f
            switch(z) {
            case 0x0:
                break;
            case 0x1:
                break;
            case 0x2:
                break;
            case 0x3:
                break;
            case 0x4:
                break;
            case 0x5:
                break;
            case 0x6:
                break;
            case 0x7: 
                switch(y) {
                case 0x0:
                    break;
                case 0x1:
                    break;
                case 0x2:
                    break;
                case 0x3:
                    break;
                case 0x4:
                    break;
                case 0x5: //CPL, complement A register, 0x2f
                    break;
                case 0x6: //SCF, set carry flag, 0x37
                    break;
                case 0x7: //CCF, complement carry flag, 0x3f
                    break;
                }
                break;
            }
        }
        else if(x==1) { //HALT and LD r,r' operations  0x40 - 0x7f
            if(y==6 && z==6) { //Copy from memory location to (same) memory location is replaced by HALT, 0x76
                halted = true;
                uint8_t int_flag = 0;
                uint8_t int_enable = 0;
                bus->update_interrupts(cycle);
                int_flag = bus->iflag();
                int_enable = bus->ienab();
                if(!interrupts && ((int_flag & int_enable) & 0x1f) > 0) { //HALT bug: PC is stuck for one instruction after exiting halt mode
                    halt_bug = true;
                }
                //printf("YO: HALT called, if: %02x ie: %02x ime: %d\n", int_flag, int_enable, interrupts);
                //printf("HALT\n");
            }
            else { //Fairly regular LD ops, 0x40 - 0x7f, except for 0x76
                if(z == 6) { //read from memory to register
                    dummy = bus->read(hl.pair, cycle+1);
                }
                else if(y==6) { //write from register into memory
                    bus->write(hl.pair, *r[z], cycle+1);
                }

                *r[y] = *r[z];
                //printf("LD %s, %s\n", r[y], r[z]);
            }
        }
        else if(x==2) { //ALU operations 0x80-0xbf
            if(z==6) { //These all have A as their destination, so there's no matching "write" call for memory operands here
                dummy = bus->read(hl.pair, cycle+1);
            }
            switch(y) {
            case 0: //ADD, no carry, 0x80 - 0x87
                if((af.hi & 0xf) + ((*(r[z])) & 0xf) >= 0x10) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(uint16_t(uint16_t(af.hi) + uint16_t(*(r[z]))) >= 0x100) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                af.hi += *(r[z]);

                clear(SUB_FLAG);

                break;
            case 1: //ADC, with carry, 0x88 - 0x8f
                {
                    bool set_c = false;
                    //printf("dbg Op:%02x ADC A(%02x), %s(%02x)\tC: %d H: %d\t=\t",((x<<6)|(y<<3)|z), af.hi, r_name[z], *r[z], carry(), hc());
                    if((af.hi & 0xf) + ((*(r[z])) & 0xf) + carry() >= 0x10) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(uint16_t(af.hi) + uint16_t(*(r[z])) + uint16_t(carry())) >= 0x100) {
                        set_c = true;
                    }
                    af.hi += ((*(r[z])) + carry());
                    if(set_c) set(CARRY_FLAG);
                    else clear(CARRY_FLAG);
                }
                //printf("A(%02x)\tC: %d H: %d\n", af.hi, carry(), hc());

                clear(SUB_FLAG);
                if(af.hi) clear(ZERO_FLAG);
                else set(ZERO_FLAG);

                break;
            case 2: //SUB, without borrow, 0x90 - 0x97
                if((af.hi & 0xf) < ((*(r[z])) & 0xf)) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(uint16_t(uint16_t(af.hi) < uint16_t(*(r[z])))) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                af.hi -= *(r[z]);
                set(SUB_FLAG);
                if(af.hi) clear(ZERO_FLAG);
                else set(ZERO_FLAG);
                break;
            case 3: //SBC, with borrow, 0x98 - 0x9f
                if((af.hi & 0xf) < (((*(r[z])) & 0xf) + carry())) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                {
                    bool set_c = false;
                    if(uint16_t(af.hi) < uint16_t(uint16_t(*(r[z])) + uint16_t(carry()))) {
                        set_c = true;
                    }
                    af.hi -= *(r[z]);
                    af.hi -= carry();
                    if(set_c) set(CARRY_FLAG);
                    else clear(CARRY_FLAG);
                }
                if(af.hi) clear(ZERO_FLAG);
                else set(ZERO_FLAG);
                set(SUB_FLAG);
                break;
            case 4: //AND, logical and, 0xa0 - 0xa7
                af.hi &= *(r[z]);
                clear(SUB_FLAG);
                set(HALF_CARRY_FLAG);
                clear(CARRY_FLAG);
                break;
            case 5: //XOR, logical xor, 0xa8 - 0xaf
                af.hi ^= *(r[z]);
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                clear(CARRY_FLAG);
                break;
            case 6: //OR, logical or, 0xb0 - 0xb7
                af.hi |= *(r[z]);
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                clear(CARRY_FLAG);
                break;
            case 7: //CP, comparison by subtraction, 0xb8 - 0xbf
                if((af.hi & 0xf) < ((*(r[z])) & 0xf)) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(af.hi < (*(r[z]))) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                if(af.hi == (*r[z])) {
                    set(ZERO_FLAG);
                }
                else {
                    clear(ZERO_FLAG);
                }
                set(SUB_FLAG);
                break;
            }
            if(y!=7) {
                if(af.hi == 0) {
                    set(ZERO_FLAG);
                }
                else {
                    clear(ZERO_FLAG);
                }
            }
            //printf("%s %s\n", alu[y], r[z]);
        }
        else if(x==3) { //0xc0 - 0xff
            switch(z) {
            case 0x0: //3 weird LD commands, 1 weird ADD, 4 conditional returns
                switch(y) {
                case 4: //Write A to high memory address, 0xe0
                    bus->write(0xff00 + uint16_t(data), af.hi, cycle+2);
                    //printf("LD (FF00+$%02x), A\n",data);
                    break;
                case 5://add 8-bit signed to SP, 0xe8
                    data = extend(data);
                    //printf("dbg: SP: %d(0x%04x) sum: %d(0x%04x) ",sp,sp,sp+data,uint16_t(sp+data));

                    if(data >=0) {
                        if((sp&0xff) + data > 0xff) set(CARRY_FLAG);
                        else clear(CARRY_FLAG);
                        if((sp&0xf)  + (data&0xf) > 0xf ) set(HALF_CARRY_FLAG);
                        else clear(HALF_CARRY_FLAG);
                    }
                    else {
                        if((sp&0xff) > ((sp + data)&0xff)) set(CARRY_FLAG);
                        else clear(CARRY_FLAG);
                        if((sp&0xf) > ((sp + data)&0xf))   set(HALF_CARRY_FLAG);
                        else clear(HALF_CARRY_FLAG);
                    }

                    //printf("C:%d H:%d\n",carry(),hc());
                    sp += data;
                    clear(ZERO_FLAG);
                    clear(SUB_FLAG);
                    //printf("ADD SP, $%02x\n",data);
                    break;
                case 6: //Read from high memory address to A, 0xf0
                    af.hi = bus->read(0xff00 + uint16_t(data), cycle+2);
                    //printf("LD A, (FF00+$%02x)\n",data);
                    break;
                case 7: //Transfer SP+8-bit signed to HL, 0xf8
                    data = extend(data);
                    ASSERT(data>-129 && data < 128);

                    if(data >=0) {
                        if((sp&0xff) + data > 0xff) set(CARRY_FLAG);
                        else clear(CARRY_FLAG);
                        if((sp&0xf)  + (data&0xf) > 0xf ) set(HALF_CARRY_FLAG);
                        else clear(HALF_CARRY_FLAG);
                    }
                    else {
                        if((sp&0xff) > ((sp + data)&0xff)) set(CARRY_FLAG);
                        else clear(CARRY_FLAG);
                        if((sp&0xf) > ((sp + data)&0xf))   set(HALF_CARRY_FLAG);
                        else clear(HALF_CARRY_FLAG);
                    }

                    hl.pair = sp + data;

                    clear(ZERO_FLAG);
                    clear(SUB_FLAG);
                    //printf("LD HL, SP+$%02x\n",data);
                    break;
                default:
                    switch(y) {
                    case 0: //return if not zero, 0xc0
                        if(!zero()) {
                            condition = true;
                        }
                        break;
                    case 1: //return if zero, 0xc8
                        if(zero()) {
                            condition = true;
                        }
                        break;
                    case 2: //return if not carry, 0xd0
                        if(!carry()) {
                            condition = true;
                        }
                        break;
                    case 3: //return if carry, 0xd8
                        if(carry()) {
                            condition = true;
                        }
                        break;
                    }
                    if(condition) {
                        pc = bus->readmore(sp, 2, cycle+4);
                        sp += 2;
                        extra_cycles = op_times_extra[(x<<(6))+(y<<(3))+z];
                    }
                    //printf("RET %s\n", cc[y]);
                    break;
                }
                break;
            case 0x1: //Weird POPs, RETs, and JPs 0xc1, 0xc9, 0xd1, 0xd9, 0xe1, 0xe9
                if(!q) { //16-bit POP, 0xc1, 0xd1, 0xe1, 0xf1
                    *rp2[p] = bus->readmore(sp, 2, cycle+2);
                    sp += 2;
                    if(p==3) {
                        af.low &= 0xf0;
                    }
                    //printf("POP %s\n", rp2[p]);
                }
                else { //Jumps and stack ops, 0xc9, 0xd9, 0xe9, 0xf9
                    switch(p) {
                    case 0x0: //standard RET, 0xc9
                        pc = bus->readmore(sp, 2, cycle+3);
                        sp += 2;
                        //printf("RET\n");
                        break;
                    case 0x1: //RET from interrupt, Different from z80, 0xd9
                        //printf("EXX\n");
                        pc = bus->readmore(sp, 2, cycle+3);
                        sp+=2;
                        interrupts = true;
                        //printf("RETI\n");
                        break;
                    case 0x2: //Jump to HL, 0xe9
                        pc = hl.pair;
                        //printf("JP HL\n");
                        break;
                    case 0x3: //Set SP to HL, 0xf9
                        sp = hl.pair;
                        //printf("LD SP, HL\n");
                        break;
                    }
                }
                break;
            case 0x2: //Some weird LDs, conditional absolute jumps
                switch(y) {
                case 0:
                    if(!zero()) {
                        condition = true;
                    }
                    break;
                case 1:
                    if(zero()) {
                        condition = true;
                    }
                    break;
                case 2:
                    if(!carry()) {
                        condition = true;
                    }
                    break;
                case 3:
                    if(carry()) {
                        condition = true;
                    }
                    break;
                case 0x4: //Write from A to IO port C, 0xe2
                    bus->write(0xff00 + uint16_t(bc.low), af.hi, cycle+1);
                    //printf("LD (FF00+C), A\n");
                    break;
                case 0x5: //Write A to memory location, 0xea
                    bus->write(data,af.hi, cycle+3);
                    //printf("LD ($%04x), A\n",data);
                    break;
                case 0x6: //Read from IO port C to A, 0xf2
                    af.hi = bus->read(0xff00 + uint16_t(bc.low), cycle+1);
                    //printf("LD A, (FF00+C)\n");
                    break;
                case 0x7: //Read from memory location to A, 0xfa
                    af.hi = bus->read(data, cycle+3);
                    //printf("LD A, ($%04x)\n",data);
                    break;
                }
                if(y<4 && condition) {
                    pc = data;
                    extra_cycles = op_times_extra[(x<<(6))+(y<<(3))+z];
                }
                //printf("JP %s, $%04x\n", cc[y],data);
                break;
            case 0x3: //A Jump, a prefix, Interrupt flag control, and 4 dead ops
                switch(y) {
                case 0x0: //0xc3
                    pc = data;
                    //printf("JP $%04x\n",data);
                    break;
                case 0x1: //0xcb
                    //printf("CB Prefix\n");
                    break;
                case 0x2: //Different from z80, 0xd3
                    //printf("OUT (n), A\n");
                    //printf("---- (diff)\n");
                    break;
                case 0x3: //Different from z80, 0xdb
                    //printf("IN A, (n)\n");
                    //printf("---- (diff)\n");
                    break;
                case 0x4: //Different from z80, 0xe3
                    //printf("EX (SP), HL\n");
                    //printf("---- (diff)\n");
                    break;
                case 0x5: //Different from z80, 0xeb
                    //printf("EX DE, HL\n");
                    //printf("---- (diff)\n");
                    break;
                case 0x6: //Disable interrupts, 0xf3
                    //printf("X: %d Y: %d Z: %d ",x,y,z);
                    interrupts = false;
                    //printf("DI\n");
                    break;
                case 0x7: //Enable interrupts, 0xfb
                    if(!interrupts) {
                        set_ime = true;
                    }
                    //printf("EI\n");
                    break;
                }
                break;
            case 0x4: //Conditional calls and 4 dead ops, 0xc4, 0xcc, 0xd4, 0xdc
                if(y<4) {
                    switch(y) {
                    case 0: //0xc4
                        if(!zero()) {
                            condition = true;
                        }
                        break;
                    case 1: //0xcc
                        if(zero()) {
                            condition = true;
                        }
                        break;
                    case 2: //0xd4
                        if(!carry()) {
                            condition = true;
                        }
                        break;
                    case 3: //0xdc
                        if(carry()) {
                            condition = true;
                        }
                        break;
                    }
                    if(condition) {
                        bus->writemore(sp-2, pc, 2, cycle+5);
                        sp -= 2;
                        pc = data;
                        extra_cycles = op_times_extra[(x<<(6))+(y<<(3))+z];
                    }
                    //printf("CALL %s, $%04x\n", cc[y],data);
                }
                else { //Dead ops, which I think were prefixes in Z80
                    //printf("---- (diff)\n");
                }
                break;
            case 0x5: //16-bit PUSHes, unconditional call to immediate address, 3 dead ops
                if(!q) { //16-bit PUSH, 0xc5, 0xd5, 0xe5, 0xf5
                    bus->writemore(sp-2, *rp2[p], 2, cycle+3);
                    sp -= 2;
                    //printf("PUSH %s\n", rp2[p]);
                }
                else {
                    switch(p) {
                    case 0x0: //Unconditional call to immediate address 
                        bus->writemore(sp-2, pc, 2, cycle+5);
                        sp -= 2;
                        pc = data;
                        //printf("CALL $%04x\n",data);
                        break;
                    default: //Different from z80, Dead prefixes, 0xdd, 0xed, 0xfd
                        //printf("---- (diff)\n");
                        break;
                    }
                }
                break;
            case 0x6: //ALU ops with immediate values
                switch(y) {
                case 0: //ADD, 0xc6
                    if((af.hi & 0xf) + (data & 0xf) >= 0x10) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(uint16_t(af.hi) + uint16_t(data)) >= 0x100) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    af.hi += data;
                    clear(SUB_FLAG);
                    break;
                case 1: //ADC, 0xce
                    {
                        bool set_c = false;
                        data &= 0xff; //data is an int, not a uint8_t
                        //printf("dbg Op:%02x ADC A(%02x), %02x\tC: %d H: %d\t=\t",((x<<6)|(y<<3)|z), af.hi, data, carry(), hc());
                        if((af.hi & 0xf) + (data & 0xf) + carry() >= 0x10) {
                            set(HALF_CARRY_FLAG);
                        }
                        else {
                            clear(HALF_CARRY_FLAG);
                        }
                        if(uint16_t(uint16_t(af.hi) + uint16_t(data) + uint16_t(carry())) >= 0x100) {
                            set_c = true;
                        }
                        af.hi += (data + carry());
                        if(set_c) set(CARRY_FLAG);
                        else clear(CARRY_FLAG);
                        //printf("A(%02x)\tC: %d H: %d\n", af.hi, carry(), hc());
                    }
                    clear(SUB_FLAG);
                    if(af.hi) clear(ZERO_FLAG);
                    else set(ZERO_FLAG);
                    break;
                case 2: //SUB, 0xd6
                    if((af.hi & 0xf) < (data & 0xf)) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(af.hi) < uint16_t(data)) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    af.hi -= data;
                    set(SUB_FLAG);
                    break;
                case 3: //SBC, 0xde
                    if((af.hi & 0xf) < ((data & 0xf) + carry())) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    {
                        bool set_c = false;
                        if(uint16_t(af.hi) < uint16_t(uint16_t(data) + uint16_t(carry()))) {
                            set_c = true;
                        }
                        af.hi -= data;
                        af.hi -= carry();
                        if(set_c) set(CARRY_FLAG);
                        else clear(CARRY_FLAG);
                    }
                    if(af.hi) clear(ZERO_FLAG);
                    else set(ZERO_FLAG);
                    set(SUB_FLAG);
                    break;
     
                    /*
                    if((af.hi & 0xf) < ((data + carry()) & 0xf)) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(af.hi) < uint16_t(uint16_t(data) + uint16_t(carry()))) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    af.hi -= data;
                    af.hi -= carry();
                    set(SUB_FLAG);
                    break;
                    */
                case 4: //AND, 0xe6
                    af.hi &= data;
                    clear(SUB_FLAG);
                    set(HALF_CARRY_FLAG);
                    clear(CARRY_FLAG);
                    break;
                case 5: //XOR, 0xee
                    af.hi ^= data;
                    clear(SUB_FLAG);
                    clear(HALF_CARRY_FLAG);
                    clear(CARRY_FLAG);
                    break;
                case 6: //OR, 0xf6
                    af.hi |= data;
                    clear(SUB_FLAG);
                    clear(HALF_CARRY_FLAG);
                    clear(CARRY_FLAG);
                    break;
                case 7: //CP, 0xfe
                    if((af.hi & 0xf) < uint8_t(data & 0xf)) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(af.hi < uint8_t(data)) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    if((af.hi - uint8_t(data)) == 0) {
                        set(ZERO_FLAG);
                    }
                    else {
                        clear(ZERO_FLAG);
                    }
                    set(SUB_FLAG);
                    break;
                }
                if(y!=7) {
                    if(af.hi == 0) {
                        set(ZERO_FLAG);
                    }
                    else {
                        clear(ZERO_FLAG);
                    }
                }
                //printf("%s $%02x\n", alu[y],data);
                break;
            case 0x7: //RST Y*8, where y = [0..7], 0xc7, 0xcf, 0xd7, 0xdf, 0xe7, 0xef, 0xf7, 0xff
                bus->writemore(sp-2, pc, 2, cycle+3);
                sp -= 2;
                pc = y * 8;
                //printf("RST %02X\n", y*8);
                break;
            }
        }
        //printf("\n");
    }
    else if(pre==0xCB) {
        if(z==6) { //Read value from memory to temp location
            dummy = bus->read(hl.pair, cycle+2);
        }
        if(x==0) {
            switch(y) {
            case 0: //RLC
                if((*r[z]) & BIT7) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                (*r[z])<<=(1);
                (*r[z])+=carry();
                if(*r[z]) clear(ZERO_FLAG);
                else      set(ZERO_FLAG);
                break;
            case 1: //RRC
                if((*r[z]) & BIT0) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                (*r[z])>>=(1);
                (*r[z])+=carry()*BIT7;
                if(*r[z]) clear(ZERO_FLAG);
                else      set(ZERO_FLAG);
                break;
            case 2: //RL
                data = carry();
                if((*r[z]) & BIT7) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                (*r[z])<<=(1);
                (*r[z])+=data;
                if(*r[z]) clear(ZERO_FLAG);
                else      set(ZERO_FLAG);
                break;
            case 3: //RR
                data = carry();
                if((*r[z]) & BIT0) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                (*r[z])>>=(1);
                (*r[z])+=data*BIT7;
                if(*r[z]) clear(ZERO_FLAG);
                else      set(ZERO_FLAG);
                break;
            case 4: //SLA
                if((*r[z]) & BIT7) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                (*r[z])<<=(1);
                if(*r[z]) clear(ZERO_FLAG);
                else      set(ZERO_FLAG);
                break;
            case 5: //SRA
                data = *r[z] & BIT7;
                if((*r[z]) & BIT0) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                (*r[z])>>=(1);
                (*r[z])|=data;
                if(*r[z]) clear(ZERO_FLAG);
                else      set(ZERO_FLAG);
                break;
            case 6: //SWAP
                if(!(*r[z])) set(ZERO_FLAG);
                else clear(ZERO_FLAG);
                {
                    uint8_t top=(((*r[z])&0xf0)>>(4));
                    uint8_t bottom=(((*r[z])&0x0f)<<(4));
                    *r[z]=(top|bottom);
                }
                clear(SUB_FLAG);
                clear(CARRY_FLAG);
                clear(HALF_CARRY_FLAG);
                break;
            case 7: //SRL
                if((*r[z]) & BIT0) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                (*r[z])>>=(1);
                if(*r[z]) clear(ZERO_FLAG);
                else      set(ZERO_FLAG);
                break;
            }
            /*
            if(y>=4)
                //printf("%s %s\n", rot[y], r[z]);*/
        }
        else if(x==1) {
            if((1<<(y)) & (*r[z])) {
                clear(ZERO_FLAG);
            }
            else {
                set(ZERO_FLAG);
            }

            clear(SUB_FLAG);
            set(HALF_CARRY_FLAG);

            //printf("BIT %02x, %s\n", y, r[z]);
        }
        else if(x==2) {
            (*r[z]) &= (~(1<<(y)));
            //printf("RES %02x, %s\n", y, r[z]);
        }
        else if(x==3) {
            (*r[z]) |= (1<<(y));
            //printf("SET %02x, %s\n", y, r[z]);
        }

        if(x!=1 && z==6) { //3/4 of the instructions affect their operand; write back to memory if the operand is a memory address
            bus->write(hl.pair, dummy, cycle+3);
        }
    }
    else {
        //printf("Prefix \"0x%02x\" doesn't exist in the Game Boy's CPU.\n", pre);
    }
    return extra_cycles;
}

//All CB-prefix ops are 2 bytes long and either 8 or 16 cycle instructions (memory operations take more time)
const uint8_t cpu::op_bytes[256] = 
    {1,3,1,1,1,1,2,1,3,1,1,1,1,1,2,1,
     2,3,1,1,1,1,2,1,2,1,1,1,1,1,2,1,
     2,3,1,1,1,1,2,1,2,1,1,1,1,1,2,1,
     2,3,1,1,1,1,2,1,2,1,1,1,1,1,2,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,3,3,3,1,2,1,1,1,3,1,3,3,2,1,
     1,1,3,0,3,1,2,1,1,1,3,0,3,0,2,1,
     2,1,1,0,0,1,2,1,2,1,3,0,0,0,2,1,
     2,1,1,1,0,1,2,1,2,1,3,1,0,0,2,1};


//Times are in CPU cycles, not machine cycles. Machine cycles are 4x shorter.
//For conditionals, times this table are for "branch not taken".
const uint8_t cpu::op_times[256] = 
    {1,3,2,2,1,1,2,1,5,2,2,2,1,1,2,1,
     1,3,2,2,1,1,2,1,3,2,2,2,1,1,2,1,
     2,3,2,2,1,1,2,1,2,2,2,2,1,1,2,1,
     2,3,2,2,3,3,3,1,2,2,2,2,1,1,2,1,
     1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
     1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
     1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
     2,2,2,2,2,2,1,2,1,1,1,1,1,1,2,1,
     1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
     1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
     1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
     1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
     2,3,3,4,3,4,2,4,2,4,3,4,3,6,2,4,
     2,3,3,0,3,4,2,4,2,4,3,0,3,0,2,4,
     3,3,2,0,0,4,2,4,4,1,4,0,0,0,2,4,
     3,3,2,1,0,4,2,4,3,2,4,1,0,0,2,4};

//Additional times to add for conditional branches that are taken
const uint8_t cpu::op_times_extra[256] =
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
     1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     3,0,1,0,3,0,0,0,3,0,1,0,3,0,0,0,
     3,0,1,0,3,0,0,0,3,0,1,0,3,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void cpu::registers() {
    printf("PC: %04x A: %02x B: %02x C: %02x D: %02x E: %02x H: %02x L: %02x SP: %04x F: %02x IME: %d IE: %02x IF: %02x",
            pc,af.hi,bc.hi,bc.low,de.hi,de.low,hl.hi,hl.low,sp,af.low, interrupts, (bus->ienab())&0x1f, (bus->iflag())&0x1f);
}

bool cpu::call_interrupts() {
    //const char * names[6] = {"none", "vblank", "lcdstat", "timer", "serial", "joypad"};
    INT_TYPE interrupt = bus->get_interrupt();
    //if(interrupt != NONE) {
        //printf("Bus says interrupt: %s\n", names[interrupt]);
    //}
    bool call = true;
    uint16_t to_run = pc;
    switch(interrupt) {
        case VBLANK: to_run = VBL_INT_ADDR;
            break;
        case LCDSTAT: to_run = LCD_INT_ADDR;
            break;
        case TIMER: to_run = TIM_INT_ADDR;
            break;
        case SERIAL: to_run = SER_INT_ADDR;
            break;
        case JOYPAD: to_run = JOY_INT_ADDR;
            break;
        default:
            call = false;
    }
    if(call) {
        //printf("INT: calling to %d\n", to_run);
        bus->writemore(sp-2, pc, 2, cycle);
        sp -= 2;

        //DI
        interrupts = false;

        //Jump to interrupt handler
        pc = to_run;
    }
    return call;
}

bool cpu::toggle_trace() {
    trace = !trace;
    return trace;
}
