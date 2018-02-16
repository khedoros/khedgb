#include<cstdint>
#include "memmap.h"
#include<iostream>
#include<fstream>
#include<cstring>
#include<cassert>

const unsigned int memmap::timer_clock_select[4] = {
      256, //4096Hz
        4, //262144Hz
       16, //65536Hz
       64 }; //16384Hz
                            

memmap::memmap(const std::string& rom_filename, const std::string& fw_file) : 
                                                  screen(), sound(),
                                                  cart(rom_filename, fw_file),
                                                  int_enabled{0,0,0,0,0},
                                                  int_requested{0,0,0,0,0},
                                                  last_int_cycle(0), link_data(0), div_reset(0), timer(0), timer_modulus(0), timer_control(0),
                                                  timer_running(false), clock_divisor(timer_clock_select[0]), timer_reset(0), timer_deadline(-1), 
                                                  div_clock(0), sgb_active(false),
                                                  bit_ptr(0), sgb_buffered(false), sgb_cur_joypad(0), sgb_joypad_count(1), sgb_cmd_index(0), sgb_cmd_count(0)
{

    valid = cart.valid;
    if(!valid) return;

    for(int i=0;i<4;i++) {
        directions[i].keys = 0x00;
        btns[i].keys = 0x00;
    }
    wram.resize(0x2000);
    hram.resize(0x7f);
    sgb_buffer.resize(16,0);

    if(fw_file == "") {
        uint8_t temp = 0;
        temp = 0x00; write(0xFF05, &temp, 1, 0);    //; TIMA
        temp = 0x00; write(0xFF06, &temp, 1, 0);    //; TMA
        temp = 0x00; write(0xFF07, &temp, 1, 0);    //; TAC
        temp = 0x80; write(0xFF10, &temp, 1, 0);    //; NR10
        temp = 0xBF; write(0xFF11, &temp, 1, 0);    //; NR11
        temp = 0xF3; write(0xFF12, &temp, 1, 0);    //; NR12
        temp = 0xBF; write(0xFF14, &temp, 1, 0);    //; NR14
        temp = 0x3F; write(0xFF16, &temp, 1, 0);    //; NR21
        temp = 0x00; write(0xFF17, &temp, 1, 0);    //; NR22
        temp = 0xBF; write(0xFF19, &temp, 1, 0);    //; NR24
        temp = 0x7F; write(0xFF1A, &temp, 1, 0);    //; NR30
        temp = 0xFF; write(0xFF1B, &temp, 1, 0);    //; NR31
        temp = 0x9F; write(0xFF1C, &temp, 1, 0);    //; NR32
        temp = 0xBF; write(0xFF1E, &temp, 1, 0);    //; NR33
        temp = 0xFF; write(0xFF20, &temp, 1, 0);    //; NR41
        temp = 0x00; write(0xFF21, &temp, 1, 0);    //; NR42
        temp = 0x00; write(0xFF22, &temp, 1, 0);    //; NR43
        temp = 0xBF; write(0xFF23, &temp, 1, 0);    //; NR30
        temp = 0x77; write(0xFF24, &temp, 1, 0);    //; NR50
        temp = 0xF3; write(0xFF25, &temp, 1, 0);    //; NR51
        temp = 0xF1; write(0xFF26, &temp, 1, 0);    //; NR52
        temp = 0x91; write(0xFF40, &temp, 1, 0);    //; LCDC
        temp = 0x00; write(0xFF42, &temp, 1, 0);    //; SCY
        temp = 0x00; write(0xFF43, &temp, 1, 0);    //; SCX
        temp = 0x00; write(0xFF45, &temp, 1, 0);    //; LYC
        temp = 0xFC; write(0xFF47, &temp, 1, 0);    //; BGP
        temp = 0xFF; write(0xFF48, &temp, 1, 0);    //; OBP0
        temp = 0xFF; write(0xFF49, &temp, 1, 0);    //; OBP1
        temp = 0x00; write(0xFF4A, &temp, 1, 0);    //; WY
        temp = 0x00; write(0xFF4B, &temp, 1, 0);    //; WX
        temp = 0x00; write(0xFFFF, &temp, 1, 0);    //; IE
    }
}

void memmap::read(int addr, void * val, int size, uint64_t cycle) {
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";
    if(addr >= 0 && addr < 0x8000) { //Cartridge 0x0000-0x7fff
        cart.read(addr, val, size, cycle);
    }
    else if (addr >= 0x8000 && addr < 0xa000) { //VRAM 0x8000-0x9fff
        screen.read(addr, val, size, cycle);//memcpy(val, &(vram[addr-0x8000]), size);
    }
    else if (addr >= 0xa000 && addr < 0xc000) { //External RAM 0xa000-0xbfff
        cart.read(addr, val, size, cycle);
        //std::cout<<"Read from external RAM: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
    }
    else if (addr >= 0xc000 && addr < 0xfe00) { //8KB of memory, c000-dfff, then a partial mirror of it e000-fe00
        memcpy(val, &(wram[addr & 0x1fff]), size);
    }
    else if (addr >= 0xfe00 && addr < 0xfea0) {
        screen.read(addr, val, size, cycle);
    }
    else if (addr >= 0xfea0 && addr < 0xff00) {
        //"Not useable" RAM area; apparently returns 0 on DMG, 0 and random values on CGB
        memset(val, 0, size);
    }
    else if (addr >= 0xff00 && addr < 0xff80) {
        switch(addr) {
        case 0xff00: 
            {
                uint8_t keyval = 0xf0;
                if((joypad & 0x20) == 0) {
                    keyval |= btns[sgb_cur_joypad].keys;
                }
                if((joypad & 0x10) == 0) {
                    keyval |= directions[sgb_cur_joypad].keys;
                }
                if(joypad == 0x30 && sgb_active) { //SGB output current joypad number
                    sgb_cur_joypad++;
                    sgb_cur_joypad %= sgb_joypad_count;
                    if(sgb_joypad_count > 1) {
                        printf("SGB: current pad: %d/%d\n", sgb_cur_joypad+1, sgb_joypad_count);
                    }
                    keyval |= sgb_cur_joypad;
                }
                *((uint8_t *)val) = (0xc0 | joypad | (~keyval));
                //printf("Joy: returning %02x\n", uint8_t(0xc0|joypad|(~keyval)));
            }
            //printf("Stubbed out read to gamepad (not implemented yet)\n");
            break;
        case 0xff01:
            //std::cout<<"Read from link cable: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            //printf("Read from serial: 0x%04x (got 0x%02x)\n", addr, link_data);
            *(uint8_t *)val = link_data;
            break;
        case 0xff02:
            *(uint8_t *)val = (serial_transfer * 0x80) | internal_clock;
            //printf("Read from serial: 0x%04x (got 0x%02x)\n", addr, (serial_transfer * 0x80) | internal_clock);
            break;
        case 0xff04: //DIV register. 16KHz increment, (1024*1024)/16384=64, and the register overflows every 256 increments
            *(uint8_t *)val = ((cycle - div_reset) / 64) % 256;
            //std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<((cycle - div_reset) / 64) % 256<<std::endl;
            break;
        case 0xff05:
            if(!timer_running) {
                *(uint8_t *)val = timer;
            }
            else {
                *(uint8_t *)val = (uint64_t(timer) + ((cycle - timer_reset) / clock_divisor)) % (256 - timer_modulus) + timer_modulus;
            }
            //std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<(uint64_t(timer) + ((cycle - timer_reset) / clock_divisor)) % (256 - timer_modulus) + timer_modulus<<std::endl;
            break;
        case 0xff06:
            *(uint8_t *)val = timer_modulus;
            //std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(timer_modulus)<<std::endl;
            break;
        case 0xff07:
            *(uint8_t *)val = 0xf8 | timer_control;
            //std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<"= 0x"<<timer_control<<std::endl;
            break;
        case 0xff0f:
            *((uint8_t *)val) = 0xe0 | int_requested.reg;
            //std::cout<<"Read from interrupt hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            break;
        case 0xff44: //TODO: Move this to PPU, base on global cycle count instead of frame cycle count
            *((uint8_t *)val) = (((cycle - screen.get_active_cycle()) % 17556) / 114);
            break;
        case 0xff70: //CGB WRAM size switch
            //TODO: Implement with CGB stuff
            *((uint8_t *)val) = 0;
            break;
        default:
            if(addr > 0xff0f && addr < 0xff3f) {
                //std::cout<<"Read from audio hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
                *((uint8_t *)val) = sound.read(addr, cycle);
            }
            else if(addr > 0xff3f && addr < 0xff4c) {
                screen.read(addr, val, size, cycle);//memcpy(val, &(vram[addr-0x8000]), size);
                //std::cout<<"Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff4e && addr < 0xff6c) {
                std::cout<<"Read from CGB DMA/RAM: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else {
                memset(val, 0xff, size);
                std::cout<<"Read from unknown mem-mapped hardware: 0x"<<std::hex<<addr<<" (assuming it would read 0xff)"<<std::endl;
            }
        }
    }
    else if (addr >= 0xff80 && addr < 0xffff) { //127 bytes High RAM
        memcpy(val, &(hram[addr & 0x7f]), size);
    }
    else if (addr == 0xffff) {
        //std::cerr<<"Attempted read of write-only register?"<<std::endl;
        *((uint8_t *)val) = int_enabled.reg;
    }
    else {
        memset(val, 0xff, size);
        //std::cerr<<"Water fog? Trying to read from 0x"<<std::hex<<addr<<" bzzzzzzt"<<std::endl;
    }

}

void memmap::write(int addr, void * val, int size, uint64_t cycle) {
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";
    if(addr >= 0 && addr < 0x8000) { //Cartridge ROM
        //std::cout<<"Write to ROM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (mappers not implemented yet)"<<std::endl;
        cart.write(addr,val,size,cycle);
    }
    else if (addr >= 0x8000 && addr < 0xa000) { //Video RAM
        screen.write(addr, val, size, cycle);
        //memcpy(&(vram[addr-0x8000]), val, size);
    }
    else if (addr >= 0xa000 && addr < 0xc000) { //Cartridge RAM
        //std::cout<<"Write to external RAM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        cart.write(addr, val, size, cycle);
    }
    else if (addr >= 0xc000 && addr < 0xfe00) { //8KB Work RAM (0xc000-0xdfff) and mirrored area (0xe000-0xfe00)
        memcpy(&(wram[addr&0x1fff]), val, size);
    }
    else if (addr >= 0xfe00 && addr < 0xfea0) { //OAM memory
        screen.write(addr, val, size, cycle);
    }
    else if (addr >= 0xff00 && addr < 0xff80) { //Hardware IO area
        switch(addr) {
            case 0xff00:
                joypad = (*((uint8_t *)val) & 0x30);
                //printf("Joypad received %02x\n", joypad);
                if(joypad == 0 && sgb_active) { //SGB Low Pulse (reset)
                    bit_ptr = -1;
                    sgb_buffered = false;
                    for(int i=0;i<16;++i) sgb_buffer[i]=0;
                    //printf(" reset\n");
                }
                else if(joypad == 0x30 && sgb_active) { //Both channels deselected
                    if(!sgb_buffered) { //SGB High Pulse (value buffer)
                        sgb_buffered = true;
                        bit_ptr++;
                        //printf(" buffer\n");
                    }
                }
                else if(joypad == 0x20 && bit_ptr < 128 && sgb_buffered && sgb_active) { //SGB "0"
                    //int byte = bit_ptr / 8;
                    //int bit = bit_ptr % 8;
                    sgb_buffered = false;
                    //printf(" 0\n");
                }
                else if(joypad == 0x10 && bit_ptr < 128 && sgb_buffered && sgb_active) { //SGB "1"
                    int byte = bit_ptr / 8;
                    int bit = bit_ptr % 8;
                    sgb_buffer[byte] |= 1<<(bit);
                    sgb_buffered = false;
                    //printf(" 1 (byte %d, bit %d)\n",byte,bit);
                }
                else if(joypad == 0x20 && bit_ptr == 128 && sgb_buffered && sgb_active) { //SGB STOP bit
                    sgb_buffered = false;
                    //printf(" end\n");
                    sgb_exec(sgb_buffer);
                }
                else {
                    //printf("\n");
                }

                break;
            case 0xff01: //Fake implementation, for serial output from Blarg roms
                link_data = *((uint8_t *)val);
                //printf("Write to serial: 0x%04x = 0x%02x\n", addr, *(uint8_t *)val);
                break;
            case 0xff02:
                {
                    int cmd = *((uint8_t *)val);
                    if(cmd == 0x81) { //Immediate output for Blarg/debug stuff
                        std::cout<<"Blarg: "<<link_data<<std::endl;
                        //link_data = 0xff;
                    }
                    serial_transfer = ((cmd & 0x80) == 0x80);
                    internal_clock = ((cmd & 0x01) == 0x01);
                    if(serial_transfer) {
                        bits_transferred = 0;
                        transfer_start = cycle;
                    }
                }
                //printf("Write to serial: 0x%04x = 0x%02x\n", addr, *(uint8_t *)val);
                break;
            case 0xff04:
                div_reset = cycle;
                //printf("Write to timer hardware: 0x%04x = 0x%02x, at cycle %lld (DIV set to 0)\n", addr, *(uint8_t *)val, cycle);
                break;
            case 0xff05:
                timer = *(uint8_t *)val;
                if(timer_running) {
                    //timer++;
                    timer_reset = cycle;
                    timer_deadline = cycle + (256 - timer) * clock_divisor + 1; //delay of 1 cycle between overflow and IF flag being set
                }
                //printf("Write to timer hardware: 0x%04x = 0x%02x, at cycle %lld, new deadline %lld\n", addr,timer,cycle, timer_deadline);
                break;
            case 0xff06:
                if(timer_running) { //Calculate new timer baseline (timer+timer_reset values) @ time of change
                    timer = (uint64_t(timer) + ((cycle - timer_reset) / clock_divisor)) % (256 - timer_modulus) + timer_modulus;
                    timer_reset = cycle;
                    timer_deadline = cycle + (256 - timer) * clock_divisor + 1;
                }
                timer_modulus = *(uint8_t *)val;
                //printf("Write to timer hardware: 0x%04x = 0x%02x, at cycle %lld, calc'd timer value: %02x, new deadline %lld\n", addr, timer_modulus, cycle, timer, timer_deadline);
                break;
            case 0xff07:
            {
                bool new_running = ((*(uint8_t *)val & 0x04) == 4);
                unsigned int new_divisor = timer_clock_select[(*(uint8_t *)val & 0x03)];
                if(new_running && timer_running && new_divisor != clock_divisor) { //Divisor changed in still-running timer
                    timer = (uint64_t(timer) + ((cycle - timer_reset) / clock_divisor)) % (256 - timer_modulus) + timer_modulus;
                    timer_reset = cycle;
                    timer_deadline = cycle + (256 - timer) * new_divisor + 1;
                }
                if(new_running && !timer_running) { //Starting a stopped timer
                    //timer hasn't been changing, so we don't need to calculate its current state
                    timer_reset = cycle;
                    timer_deadline = cycle + (256 - timer) * new_divisor + 1;
                }
                if(!new_running && timer_running) { //Stopping a running timer
                    timer = (uint64_t(timer) + ((cycle - timer_reset) / clock_divisor)) % (256 - timer_modulus) + timer_modulus;
                    //timer_reset is only valid for calculating the value when the timer is running
                    timer_deadline = -1; //set interrupt deadline to a time that will "never" occur
                }
                //Don't need to do anything special if previous and current value of ff07 both say that the timer is stopped
                timer_running = new_running;
                clock_divisor = new_divisor;
            }
                //printf("Write to timer hardware: 0x%04x = 0x%02x, at cycle %lld, calc'd timer value: %02x, new deadline: %lld, running: %d, divisor: %d\n",
                //        addr, int(*(uint8_t *)val), cycle, timer, timer_deadline, timer_running, clock_divisor);
                break;
            case 0xff0f:
                assert(size == 1);
                int_requested.reg = *((uint8_t *)val);
                //printf("Interrupts requested: %02X\n",int_requested.reg);
                break;
            case 0xff46: //OAM DMA
                {
                    int dat = uint16_t(*((uint8_t *)val)) * 0x100;
                    if(dat < 0xf100) {
                        //dat is between 0x00 and 0xf1, so that covers: ROM (00 to 7f), vram (80 to 9f), cram (a0-bf), wram (c0-df), wram_echo (e0-f1)
                        uint8_t temp[0xa0];
                        read(dat, (void *)(&temp[0]), 0xa0, cycle);
                        screen.write(0xff46, (void *)(&temp[0]), 0xa0, cycle);
                    }
                }
                break;
            case 0xff50: //disables CPU firmware
                cart.write(addr,val,size,cycle);
                break;
            case 0xff70: //CGB WRAM page switch
                //TODO: Implement as part of CGB support
                break;
            default:
                if(addr > 0xff0f && addr <= 0xff3f) {
                    //std::cout<<"Write to audio hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                    sound.write(addr, *(uint8_t *)val, cycle);
                }
                else if(addr > 0xff3f && addr < 0xff4c) {
                    screen.write(addr, val, size, cycle);
                    //std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                }
                else if(addr > 0xff4e && addr < 0xff6c) { //0xff50 is handled up above
                    std::cout<<"Write to CGB DMA/RAM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                }
                else {
                    std::cout<<"Write to unknown mem-mapped hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                }
                break;
        }
    }
    else if (addr >= 0xff80 && addr < 0xffff) { //127 bytes of HRAM
        memcpy(&(hram[addr - 0xff80]), val, size);
    }
    else if (addr == 0xffff) { //Interrupt enabled register
        assert(size == 1);
        int_enabled.reg = (0xe0 | *((uint8_t *)val));
        //printf("Interrupts enabled: %02X\n",int_enabled.reg);
        //std::cout<<"Write to interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
    }
    else {
        //std::cerr<<"Water fog? Write to 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" bzzzzzzt"<<std::endl;
    }
}

void memmap::keydown(SDL_Scancode k) {
    bool changed = false;

    switch(k) {
        case SDL_SCANCODE_W:
            if(!directions[0].up && !(joypad & 0x10)) changed = true;
            directions[0].up=1;
            break;
        case SDL_SCANCODE_A:
            if(!directions[0].left && !(joypad & 0x10)) changed = true;
            directions[0].left=1;
            break;
        case SDL_SCANCODE_S:
            if(!directions[0].down && !(joypad & 0x10)) changed = true;
            directions[0].down=1;
            break;
        case SDL_SCANCODE_D:
            if(!directions[0].right && !(joypad & 0x10)) changed = true;
            directions[0].right=1;
            break;
        case SDL_SCANCODE_G:
            if(!btns[0].select && !(joypad & 0x20)) changed = true;
            btns[0].select=1;
            break;
        case SDL_SCANCODE_H:
            if(!btns[0].start && !(joypad & 0x20)) changed = true;
            btns[0].start=1;
            break;
        case SDL_SCANCODE_K:
            if(!btns[0].b && !(joypad & 0x20)) changed = true;
            btns[0].b=1;
            break;
        case SDL_SCANCODE_L:
            if(!btns[0].a && !(joypad & 0x20)) changed = true;
            btns[0].a=1;
            break;
        default:
            break;
    }
    if(changed) {
        int_requested.joypad = 1;
    }
}

void memmap::keyup(SDL_Scancode k) {
    switch(k) {
        case SDL_SCANCODE_W:
            directions[0].up=0;
            break;
        case SDL_SCANCODE_A:
            directions[0].left=0;
            break;
        case SDL_SCANCODE_S:
            directions[0].down=0;
            break;
        case SDL_SCANCODE_D:
            directions[0].right=0;
            break;
        case SDL_SCANCODE_G:
            btns[0].select=0;
            break;
        case SDL_SCANCODE_H:
            btns[0].start=0;
            break;
        case SDL_SCANCODE_K:
            btns[0].b=0;
            break;
        case SDL_SCANCODE_L:
            btns[0].a=0;
            break;
        default:
            break;
    }
}

INT_TYPE memmap::get_interrupt() {

    //printf("get_interrupt requested: %02X enabled: %02X to fire: %02X\n", int_requested.reg, int_enabled.reg, (int_enabled.reg & int_requested.reg));
    if(int_enabled.vblank && int_requested.vblank)        {int_requested.vblank = 0;  return VBLANK; }
    else if(int_enabled.lcdstat && int_requested.lcdstat) {int_requested.lcdstat = 0; return LCDSTAT;}
    else if(int_enabled.timer && int_requested.timer)     {int_requested.timer = 0;   return TIMER;  }
    else if(int_enabled.serial && int_requested.serial)   {int_requested.serial = 0;  return SERIAL; }
    else if(int_enabled.joypad && int_requested.joypad)   {int_requested.joypad = 0;  return JOYPAD; }
    else {return NONE;}
}

//Update any interrupt states that are dependent on time
void memmap::update_interrupts(uint64_t cycle) {
    uint8_t enabled = 0;
    screen.read(0xff40, &enabled, 1, cycle);
    //We aren't still in previously-seen vblank, we *are* in vblank, and the screen is enabled
    if(cycle > last_int_cycle + 1140 && screen.get_mode(cycle) == 1 && ((enabled&0x80) > 0)) {
        int_requested.vblank = 1;
        last_int_cycle = cycle; 
        //printf("INT: vbl set active @ cycle %ld\n", cycle);
    }

    //LCDSTAT
    if(screen.interrupt_triggered(cycle)) {
        int_requested.lcdstat = 1;
        //printf("INT: lcdstat set active @ cycle %ld\n", cycle);
    }

    //TODO: Finish implementing timer behavior
    //TIMER
    if(timer_running) {
        if(cycle >= timer_deadline) {
            int_requested.timer = 1;
            uint8_t cur_time = (uint64_t(timer) + ((cycle - timer_reset) / clock_divisor)) % (256 - timer_modulus) + timer_modulus;
            timer_deadline = cycle + (256 - cur_time) * clock_divisor + 1;
            //printf("Triggered timer interrupt at cycle %lld, current time: %02x, next deadline: %lld\n", cycle, cur_time, timer_deadline);
        }
    }
    //if(int_enabled.timer) { printf("Warning: timer interrupt enabled, but not implemented yet.\n");}
    //SERIAL
    if(serial_transfer && internal_clock && cycle >= transfer_start) {
        unsigned int bit = (cycle - transfer_start) / 128;
        if(bit >= 7) {
            transfer_start = -1;
            serial_transfer = false;
            link_data = 0xff;
            bits_transferred = 0;
        }
        else if(bit == bits_transferred + 1) {
            bits_transferred = bit;
            link_data<<=(1);
            link_data |= 1;
        }
        if(bit == 7) {
            bits_transferred = 0;
            serial_transfer = false;
            transfer_start = -1;
            int_requested.serial = 1;
        }
    }


    //if(int_enabled.serial) { printf("Warning: serial interrupt enabled, but not implemented yet.\n");}

    //JOYPAD: Not dependent on time; its state is automatically updated when input events are processed.
    //if(int_enabled.joypad) { printf("Warning: joypad interrupt enabled, but not implemented yet.\n");}
}

bool memmap::has_firmware() {
    return cart.firmware;
}

lcd * memmap::get_lcd() {
    return &screen;
}

bool memmap::set_sgb(bool active) {
    if(cart.supports_sgb()) {
        sgb_active = active;
    }
    return sgb_active;
}

void memmap::sgb_exec(Vect<uint8_t>& s_b) {
    //If we're on the first packet, read the data
    if(sgb_cmd_index == 0) {
        sgb_cmd = s_b[0]>>3; //what's the command in this packet
        sgb_cmd_count = s_b[0] & 0x07; //how many packets are expected
        sgb_cmd_data.resize(sgb_cmd_count*16); //allocate space to store the data
        printf("SGB command %02x, expected %02x packets\n", sgb_cmd, sgb_cmd_count);
    }

    assert(sgb_cmd_data.size() == sgb_cmd_count * 16 && sgb_cmd_data.size() > 0);

    printf("%d / %d: ", sgb_cmd_index+1, sgb_cmd_count);

    int i=0;
    if(sgb_cmd_index == 0) {
        i = 1;
        printf("0x%02x 0x%02x ", sgb_cmd, sgb_cmd_count);
    }

    for(; i < 16; ++i) {
        sgb_cmd_data[sgb_cmd_index * 16 + i] = s_b[i];
        printf("%02x ", s_b[i]);
    }

    printf("\n");

    sgb_cmd_index++;

    if(sgb_cmd_count == sgb_cmd_index) {
        sgb_cmd_index = 0;
        switch(sgb_cmd) {
            case 0x11: //MLT_REQ
                switch(sgb_cmd_data[1] & 0x03) {
                    case 0x00:
                        sgb_joypad_count = 1;
                        break;
                    case 0x01:
                        sgb_joypad_count = 2;
                        break;
                    case 0x02:
                        sgb_joypad_count = 1;
                    case 0x03:
                        sgb_joypad_count = 4;
                        break;
                }
                sgb_cur_joypad = 0;
                break;
            default:
                printf("Unsupported SGB command: %02x\n", sgb_cmd);
                break;
        }
    }
}
/*
0x0000-0x3FFF: Permanently-mapped ROM bank.
0x4000-0x7FFF: Area for switchable ROM banks.
0x8000-0x9FFF: Video RAM.
0xA000-0xBFFF: Area for switchable external RAM banks.
0xC000-0xCFFF: Game Boy’s working RAM bank 0 .
0xD000-0xDFFF: Game Boy’s working RAM bank 1.
0xFE00-0xFEFF: Sprite Attribute Table.
0xFF00-0xFF7F: Devices’ Mappings. Used to access I/O devices.
0xFF80-0xFFFE: High RAM Area.
0xFFFF: Interrupt Enable Register.
*/
