//SHARP LR35902.
//4MHz (4194304Hz) 

#include<iostream>
#include "cpu.h"

void decode(int pre,int x,int y,int z,int data);

cpu::cpu(memmap& b, bool has_firmware): bus(b),
        r{&bc.hi, &bc.low, &de.hi, &de.low, &hl.hi, &hl.low, &dummy, &af.hi},
        rp{&bc.pair, &de.pair, &hl.pair, &sp},
        rp2{&bc.pair, &de.pair, &hl.pair, &af.pair},
        int_called{false, false, false, false, false}
{
    interrupts = false;
    halted = false;
    stopped = false;
    af.pair=0;
    bc.pair=0;
    de.pair=0;
    hl.pair=0; 
    sp=0;
    if(has_firmware) {
        pc = 0;
    }
    else {
        pc = 0x100;
    }
}

int cpu::run() {
    uint32_t opcode=0;
    bool running=true;
    int cycles=0;
    while(running) {
        bus.read(pc, &opcode, 3);
        cycles = dec_and_exe(opcode);
        if(!cycles) {
            std::cout<<"No idea what to do with opcode "<<std::hex<<int(opcode)<<"."<<std::endl;
            running = false;
        }
        cycle+=cycles;
    }
    return 0;
}

int cpu::dec_and_exe(uint32_t opcode) {
    int bytes = 0;
    int prefix = 0;
    int op = 0;
    int cycles = 0;
    int data = 0;
    //std::cout<<std::hex<<opcode<<"\t";
    op = opcode & 0xff;
    int x = (op&0xc0)>>(6); // 11000000
    int y = (op&0x38)>>(3); // 00111000
    int z = (op&0x7);       // 00000111
    if(op == 0xcb) {
        prefix = 0xcb;
        op = ((opcode&0xFF00)>>(8));
        x = (op&0xc0)>>(6); // 11000000
        y = (op&0x38)>>(3); // 00111000
        z = (op&0x7);       // 00000111
        bytes = 2;
        cycles = (z == 6) ? 16 : 8;
        bytes = 2;
    }
    else {
        bytes = op_bytes[op];
        cycles = op_times[op];
    }
    
    if(bytes == 2 && !prefix) {
        data = (opcode & 0xff00)>>(8);
    }
    else if(bytes == 3) {
        data = (opcode & 0xffff00)>>(8);
    }

    printf("%04x ", pc);
    registers();
    printf("\t");
    decode(prefix,x,y,z,data);
    printf("\n");

    if(!halted && !stopped) {
        pc += bytes;
    }
    cycles += execute(prefix,x,y,z,data);


    return cycles;
}

int cpu::execute(int pre,int x,int y,int z,int data) {
    /*
    char r[][5]  =    {     "B",      "C",   "D",      "E",   "H",   "L","(HL)",  "A"};
    char rp[][3] =    {    "BC",     "DE",  "HL",     "SP"};
    char rp2[][3]=    {    "BC",     "DE",  "HL",     "AF"};
    */
    char cc[][3] =    {    "NZ",      "Z",  "NC",      "C",  "PO",  "PE",   "P",  "M"};
    char alu[][7]=    {"ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR",  "OR", "CP"};
    char rot[][5]=    {   "RLC",    "RRC",  "RL",     "RR", "SLA", "SRA", "SWAP","SRL"};
    int p,q;
    p=y/2;
    q=y%2;
    int extra_cycles = 0;
    bool condition=false;
    if(!pre) {
        if(x==0) {
            switch(z) {
            case 0x0:
                switch(y) {
                case 0x0: //NOP
                    //printf("NOP\n");
                    break;
                case 0x1:
                    //Different than Z80
                    bus.write(data, &sp, 2);
                    //printf("LD ($%04x), SP (diff)\n",data);
                    break;
                case 0x2:
                    //Different than Z80
                    halted = true;
                    stopped = true;
                    printf("STOP 0 (diff)\n");
                    break;
                case 0x3:
                    data = extend(data);
                    pc += data;
                    //printf("JR $%02x\n",data);
                    break;
                default: /* 4..7 */
                    data = extend(data);
                    switch(y-4) {
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
                    }
                    if(condition) {
                        pc = pc + data;
                        extra_cycles = op_times_extra[(x<<(6))+(y<<(3))+z];
                    }
                    //printf("JR %s, $%02x\n", cc[y-4], data);
                    break;
                }
                break;
            case 0x1:
                if(!q) {
                    *rp[p] = data;
                    //printf("LD %s, $%04x\n",rp[p],data);
                }
                else {
                    if((hl.pair & 0xfff) + (*(rp[p]) & 0xfff) >= 0x1000) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint32_t(hl.pair) + uint32_t(*(rp[p])) >= 0x10000) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    hl.pair += *(rp[p]);

                    clear(SUB_FLAG);

                    //printf("ADD HL, %s\n",rp[p]);
                }
                break;
            case 0x2:
                switch(p) {
                case 0x0:
                    if(!q) {
                        bus.write(bc.pair, &af.hi, 1);
                        //printf("LD (BC), A\n");
                    }
                    else {
                        bus.read(bc.pair, &af.hi, 1);
                        //printf("LD A, (BC)\n");
                    }
                    break;
                case 0x1:
                    if(!q) {
                        bus.write(de.pair, &af.hi, 1);
                        //printf("LD (DE), A\n");
                    }
                    else {
                        bus.read(de.pair, &af.hi, 1);
                        //printf("LD A, (DE)\n");
                    }
                    break;
                case 0x2:
                    if(!q) {
                        //Different than z80
                        //printf("LD (nn), HL\n");
                        bus.write(hl.pair, &af.hi, 1);
                        hl.pair++;
                        //printf("LDI (HL), A (diff)\n");
                    }
                    else {
                        //Different than z80
                        //printf("LD HL, (nn)\n");
                        bus.read(hl.pair, &af.hi, 1);
                        hl.pair++;
                        //printf("LDI A, (HL) (diff)\n");
                    }
                    break;
                case 0x3:
                    if(!q) {
                        //Different than z80
                        //printf("LD (nn), A\n");
                        bus.write(hl.pair, &af.hi, 1);
                        hl.pair--;
                        //printf("LDD (HL), A (diff)\n");
                    }
                    else {
                        //Different than z80
                        //printf("LD A, (nn)\n");
                        bus.read(hl.pair, &af.hi, 1);
                        hl.pair--;
                        //printf("LDD A, (HL) (diff)\n");
                    }
                    break;
                }
                break;
            case 0x3:
                if(!q) {
                    (*rp[p])++;
                    //printf("INC %s\n", rp[p]);
                }
                else {
                    (*rp[p])--;
                    //printf("DEC %s\n", rp[p]);
                }
                break;
            case 0x4:
                if(y==6) bus.read(hl.pair, &dummy, 1);
                (*r[y])++;
                if(*r[y] == 0) set(ZERO_FLAG);
                else           clear(ZERO_FLAG);
                if((*r[y] & 0xf) == 0) set(HALF_CARRY_FLAG);
                else                   clear(HALF_CARRY_FLAG);
                clear(SUB_FLAG);
                if(y==6) bus.write(hl.pair, &dummy, 1);
                //printf("INC %s\n", r[y]);
                break;
            case 0x5:
                if(y==6) bus.read(hl.pair, &dummy, 1);
                (*r[y])--;
                if(*r[y] == 0) set(ZERO_FLAG);
                else           clear(ZERO_FLAG);
                if((*r[y] & 0xf) == 0xf) set(HALF_CARRY_FLAG);
                else                     clear(HALF_CARRY_FLAG);
                set(SUB_FLAG);
                if(y==6) bus.write(hl.pair, &dummy, 1);
                //printf("DEC %s\n", r[y]);
                break;
            case 0x6:
                *r[y] = data;
                if(y==6) {
                    bus.write(hl.pair, &dummy, 1);
                }
                //printf("LD %s, $%02x\n", r[y],data);
                break;
            case 0x7:
                switch(y) {
                case 0x0:
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
                    //printf("RLCA\n");
                    break;
                case 0x1:
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
                    //printf("RRCA\n");
                    break;
                case 0x2:
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
                    //printf("RLA\n");
                    break;
                case 0x3:
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
                    //printf("RRA\n");
                    break;
                case 0x4:
                    {
                    uint8_t hi=(af.hi>>(4));
                    uint8_t lo=(af.hi & 0xf);
                    bool valid = true;
                    if(!sub()) { //Addition operation
                        if(!carry()) {
                            if(!hc()) {
                                if     (hi < 0xa && lo < 0xa) {af.hi += 0x00; clear(CARRY_FLAG);}
                                else if(hi <   9 && lo >   9) {af.hi += 0x06; clear(CARRY_FLAG);}
                                else if(hi >   9 && lo < 0xa) {af.hi += 0x60;   set(CARRY_FLAG);}
                                else if(hi >   8 && lo >   9) {af.hi += 0x66;   set(CARRY_FLAG);}
                                else valid = false;
                            }
                            else {
                                if     (hi < 0xa && lo < 4)   {af.hi += 0x06; clear(CARRY_FLAG);}
                                else if(hi >   9 && lo < 4)   {af.hi += 0x66;   set(CARRY_FLAG);}
                                else valid = false;
                            }
                        }
                        else {
                            if(!hc()) {
                                if     (hi < 3 && lo < 0xa) {af.hi += 0x60; set(CARRY_FLAG);}
                                else if(hi < 3 && lo >   9) {af.hi += 0x66; set(CARRY_FLAG);}
                                else valid = false;
                            }
                            else {
                                if(hi < 4 && lo < 4) {af.hi += 0x60; set(CARRY_FLAG);}
                                else valid = false;
                            }
                        }
                    }
                    else { //Subtraction operation
                        if(!carry()) {
                            if(!hc()) {
                                if(hi < 0xa && lo < 0xa) {af.hi += 0x00; clear(CARRY_FLAG);}
                                else valid = false;
                            }
                            else {
                                if(hi <   9 && lo >   5) {af.hi += 0xfa; clear(CARRY_FLAG);}
                                else valid = false;
                            }
                        }
                        else {
                            if(!hc()) {
                                if(hi > 0x6 && lo < 0xa) {af.hi += 0xa0; set(CARRY_FLAG);}
                                else valid = false;
                            }
                            else {
                                if(hi > 0x5 && lo > 0x5) {af.hi += 0x9a; set(CARRY_FLAG);}
                                else valid = false;
                            }
                        }
                    }
                    if(!valid) printf("DAA is confused.\n");
                    }
                    //printf("DAA\n");
                    break;
                case 0x5:
                    set(HALF_CARRY_FLAG);
                    set(SUB_FLAG);
                    af.hi= ~(af.hi);
                    //printf("CPL\n");
                    break;
                case 0x6:
                    clear(HALF_CARRY_FLAG);
                    clear(SUB_FLAG);
                    set(CARRY_FLAG);
                    //printf("SCF\n");
                    break;
                case 0x7:
                    clear(HALF_CARRY_FLAG);
                    clear(SUB_FLAG);
                    if(carry()) {
                        clear(CARRY_FLAG);
                    }
                    else {
                        set(CARRY_FLAG);
                    }
                    //printf("CCF\n");
                    break;
                }
                break;
            }
        }
        else if(x==1) { //HALT and LD r,r' operations
            if(y==6 && z==6) { //Copy from memory location to (same) memory location is replaced by HALT
                halted = true;
                //printf("HALT\n");
            }
            else { //Fairly regular LD ops
                if(z == 6) { //read from memory to register
                    bus.read(hl.pair, &dummy, 1);
                }
                else if(y==6) { //write from register into memory
                    bus.write(hl.pair, r[z], 1);
                }

                *r[y] = *r[z];
                //printf("LD %s, %s\n", r[y], r[z]);
            }
        }
        else if(x==2) { //ALU operations
            if(z==6) { //These all have A as their destination, so there's no matching "write" call for memory operands here
                bus.read(hl.pair,&dummy,1);
            }
            switch(y) {
            case 0: //ADD
                if((af.hi & 0xf) + (*(r[z]) & 0xf) >= 0x10) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(uint16_t(af.hi) + uint16_t(*(r[z])) >= 0x100) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                af.hi += *(r[z]);

                clear(SUB_FLAG);
                break;
            case 1: //ADC
                if((af.hi & 0xf) + (*(r[z]) & 0xf)  + carry() >= 0x10) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(uint16_t(af.hi) + uint16_t(*(r[z])) + carry() >= 0x100) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                af.hi += *(r[z]) + carry();
                clear(SUB_FLAG);
                break;
            case 2: //SUB
                if((af.hi & 0xf) - (*(r[z]) & 0xf) >= 0x10) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(uint16_t(af.hi) - uint16_t(*(r[z])) >= 0x100) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                af.hi -= *(r[z]);
                set(SUB_FLAG);
                break;
            case 3: //SBC
                if((af.hi & 0xf) - (*(r[z]) & 0xf)  - carry() >= 0x10) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(uint16_t(af.hi) - uint16_t(*(r[z])) - carry() >= 0x100) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                af.hi -= *(r[z]) - carry();
                set(SUB_FLAG);
                break;
            case 4: //AND
                af.hi &= *(r[z]);
                clear(SUB_FLAG);
                set(HALF_CARRY_FLAG);
                clear(CARRY_FLAG);
                break;
            case 5: //XOR
                af.hi ^= *(r[z]);
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                clear(CARRY_FLAG);
                break;
            case 6: //OR
                af.hi |= *(r[z]);
                clear(SUB_FLAG);
                clear(HALF_CARRY_FLAG);
                clear(CARRY_FLAG);
                break;
            case 7: //CP
                if((af.hi & 0xf) - (*(r[z]) & 0xf) >= 0x10) {
                    set(HALF_CARRY_FLAG);
                }
                else {
                    clear(HALF_CARRY_FLAG);
                }
                if(uint16_t(af.hi) - uint16_t(*(r[z])) >= 0x100) {
                    set(CARRY_FLAG);
                }
                else {
                    clear(CARRY_FLAG);
                }
                if(af.hi - (*r[z]) == 0) {
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
        else if(x==3) {
            switch(z) {
            case 0x0:
                switch(y) {
                case 4:
                    bus.write(0xff00 + data, &af.hi, 1);
                    //printf("LD (FF00+$%02x), A (diff)\n",data);
                    break;
                case 5:
                    data = extend(data);
                    sp += data;
                    //printf("ADD SP, $%02x (diff)\n",data);
                    break;
                case 6:
                    bus.read(0xff00 + data, &af.hi, 1);
                    //printf("LD A, (FF00+$%02x) (diff)\n",data);
                    break;
                case 7:
                    data = extend(data);
                    hl.pair = sp + data;
                    //printf("LD HL, SP+$%02x (diff)\n",data);
                    break;
                default:
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
                    }
                    if(condition) {
                        bus.read(sp, &pc, 2);
                        sp += 2;
                        extra_cycles = op_times_extra[(x<<(6))+(y<<(3))+z];
                    }
                    //printf("RET %s\n", cc[y]);
                    break;
                }
                break;
            case 0x1:
                if(!q) {
                    bus.read(sp, rp2[p], 2);
                    sp += 2;
                    //printf("POP %s\n", rp2[p]);
                }
                else {
                    switch(p) {
                    case 0x0:
                        bus.read(sp, &pc, 2);
                        sp += 2;
                        //printf("RET\n");
                        break;
                    case 0x1: //Different from z80
                        //printf("EXX\n");
                        bus.read(sp, &pc, 2);
                        sp+=2;
                        interrupts = saved_interrupts;
                        //printf("RETI (diff)\n");
                        break;
                    case 0x2:
                        pc = hl.pair;
                        //printf("JP HL\n");
                        break;
                    case 0x3:
                        sp = hl.pair;
                        //printf("LD SP, HL\n");
                        break;
                    }
                }
                break;
            case 0x2:
                switch(y) {
                case 0x4:
                    bus.write(0xff00 + bc.low, &af.hi,1);
                    //printf("LD (FF00+C), A (diff)\n");
                    break;
                case 0x5:
                    bus.write(data,&af.hi,1);
                    //printf("LD ($%04x), A (diff)\n",data);
                    break;
                case 0x6:
                    bus.read(0xff00 + bc.low, &af.hi,1);
                    //printf("LD A, (FF00+C) (diff)\n");
                    break;
                case 0x7:
                    bus.read(data,&af.hi,1);
                    //printf("LD A, ($%04x) (diff)\n",data);
                    break;
                default:
                    switch(y-4) {
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
                    }
                    if(condition) {
                        pc = data;
                        extra_cycles = op_times_extra[(x<<(6))+(y<<(3))+z];
                    }
                    //printf("JP %s, $%04x\n", cc[y],data);
                    break;
                }
                break;
            case 0x3:
                switch(y) {
                case 0x0:
                    pc = data;
                    //printf("JP $%04x\n",data);
                    break;
                case 0x1:
                    printf("CB Prefix\n");
                    break;
                case 0x2: //Different from x80
                    //printf("OUT (n), A\n");
                    printf("---- (diff)\n");
                    break;
                case 0x3:
                    //printf("IN A, (n)\n");
                    printf("---- (diff)\n");
                    break;
                case 0x4: //Different from z80
                    //printf("EX (SP), HL\n");
                    printf("---- (diff)\n");
                    break;
                case 0x5: //Different from z80
                    //printf("EX DE, HL\n");
                    printf("---- (diff)\n");
                    break;
                case 0x6:
                    //printf("X: %d Y: %d Z: %d ",x,y,z);
                    interrupts = false;
                    //printf("DI\n");
                    break;
                case 0x7:
                    interrupts = true;
                    //printf("EI\n");
                    break;
                }
                break;
            case 0x4:
                if(y<4) {
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
                    }
                    if(condition) {
                        bus.write(sp-2, &pc, 2);
                        sp -= 2;
                        pc = data;
                        extra_cycles = op_times_extra[(x<<(6))+(y<<(3))+z];
                    }
                    //printf("CALL %s, $%04x\n", cc[y],data);
                }
                else {
                    printf("---- (diff)\n");
                }
                break;
            case 0x5:
                if(!q) {
                    bus.write(sp-2, rp2[p], 2);
                    sp -= 2;
                    //printf("PUSH %s\n", rp2[p]);
                }
                else {
                    switch(p) {
                    case 0x0:
                        bus.write(sp-2, &pc, 2);
                        sp -= 2;
                        pc = data;
                        //printf("CALL $%04x\n",data);
                        break;
                    case 0x1: //Different from z80
                        //printf("DD Prefix\n");
                        printf("---- (diff)\n");
                        break;
                    case 0x2: //Different from z80
                        //printf("ED Prefix\n");
                        printf("---- (diff)\n");
                        break;
                    case 0x3: //Different from z80
                        //printf("FD Prefix\n");
                        printf("---- (diff)\n");
                        break;
                    }
                }
                break;
            case 0x6:
                switch(y) {
                case 0: //ADD
                    if((af.hi & 0xf) + (data & 0xf) >= 0x10) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(af.hi) + uint16_t(data) >= 0x100) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    af.hi += data;
                    clear(SUB_FLAG);
                    break;
                case 1: //ADC
                    if((af.hi & 0xf) + (data & 0xf)  + carry() >= 0x10) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(af.hi) + uint16_t(data) + carry() >= 0x100) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    af.hi += data + carry();
                    clear(SUB_FLAG);
                    break;
                case 2: //SUB
                    if((af.hi & 0xf) - (data & 0xf) >= 0x10) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(af.hi) - uint16_t(data) >= 0x100) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    af.hi -= data;
                    set(SUB_FLAG);
                    break;
                case 3: //SBC
                    if((af.hi & 0xf) - (data & 0xf)  - carry() >= 0x10) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(af.hi) - uint16_t(data) - carry() >= 0x100) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    af.hi -= (data + carry());
                    set(SUB_FLAG);
                    break;
                case 4: //AND
                    af.hi &= data;
                    clear(SUB_FLAG);
                    set(HALF_CARRY_FLAG);
                    clear(CARRY_FLAG);
                    break;
                case 5: //XOR
                    af.hi ^= data;
                    clear(SUB_FLAG);
                    clear(HALF_CARRY_FLAG);
                    clear(CARRY_FLAG);
                    break;
                case 6: //OR
                    af.hi |= data;
                    clear(SUB_FLAG);
                    clear(HALF_CARRY_FLAG);
                    clear(CARRY_FLAG);
                    break;
                case 7: //CP
                    if((af.hi & 0xf) - (data & 0xf) >= 0x10) {
                        set(HALF_CARRY_FLAG);
                    }
                    else {
                        clear(HALF_CARRY_FLAG);
                    }
                    if(uint16_t(af.hi) - uint16_t(data) >= 0x100) {
                        set(CARRY_FLAG);
                    }
                    else {
                        clear(CARRY_FLAG);
                    }
                    if(af.hi - (data == 0)) {
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
            case 0x7:
                bus.write(sp-2, &pc, 2);
                sp -= 2;
                pc = data * 8;
                //printf("RST %02X\n", y*8);
                break;
            }
        }
        //printf("\n");
    }
    else if(pre==0xCB) {
        if(x==0) {
            if(z==6) {
                bus.read(hl.pair, &dummy, 1);
            }
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
            if(z==6) {
                bus.write(hl.pair, &dummy, 1);
            }
            if(y>=4)
                printf("%s %s %s\n", rot[y], r[z], (y==6)?"(diff)":"");
        }
        else if(x==1) {
            if(z==6) {
                bus.read(hl.pair, &dummy, 1);
            }
            if((1<<(y)) & (*r[z])) {
                clear(ZERO_FLAG);
            }
            else {
                set(ZERO_FLAG);
            }

            //printf("BIT %02x, %s\n", y, r[z]);
        }
        else if(x==2) {
            if(z==6) {
                bus.read(hl.pair, &dummy, 1);
            }
            (*r[z]) &= (~(1<<(y)));
            if(z==6) {
                bus.write(hl.pair, &dummy, 1);
            }
            //printf("RES %02x, %s\n", y, r[z]);
        }
        else if(x==3) {
            if(z==6) {
                bus.read(hl.pair, &dummy, 1);
            }
            (*r[z]) |= (1<<(y));
            if(z==6) {
                bus.write(hl.pair, &dummy, 1);
            }
            //printf("SET %02x, %s\n", y, r[z]);
        }
    }
    else {
        printf("Prefix \"0x%02x\" doesn't exist in the Game Boy's CPU.\n", pre);
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
     2,1,2,0,0,1,2,1,2,1,3,0,0,0,2,1,
     2,1,2,1,0,1,2,1,2,1,3,1,0,0,2,1};


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
    printf("PC: %04x SP: %04x A: %02x F: %02x B: %02x C: %02x D: %02x E: %02x H: %02x L: %02x",
            pc,sp,af.hi,af.low,bc.hi,bc.low,de.hi,de.low,hl.hi,hl.low);
}

