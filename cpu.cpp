//SHARP LR35902.
//4MHz (4194304Hz) 

#include<iostream>
#include "cpu.h"

void decode(int pre,int x,int y,int z,int data);

cpu::cpu(memmap& b, bool has_firmware): bus(b) {
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
        bus.map(pc, &opcode, 3, false);
        if(!dec_and_exe(opcode)) {
            std::cout<<"No idea what to do with opcode "<<std::hex<<int(opcode)<<"."<<std::endl;
            running = false;
        }
    }
    return 0;
}

int cpu::dec_and_exe(uint32_t opcode) {
    int bytes = 0;
    int prefix = 0;
    int op = 0;
    int cycles = 0;
    int data = 0;
    std::cout<<std::hex<<opcode<<"\t";
    op = opcode & 0xff;
    int x = (op&0xc0)>>(6); // 11000000
    int y = (op&0x38)>>(3); // 00111000
    int z = (op&0x7);       // 00000111
    if(op == 0xcb) {
        prefix = 0xcb;
        op = ((opcode&0xFF00)>>(8));
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

    decode(prefix,x,y,z,data);

    cycles += execute(prefix,x,y,z,data);

    pc += bytes;

    return cycles;
}

int cpu::execute(int pre,int x,int y,int z,int data) {
    return 0;
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
