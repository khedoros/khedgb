#include<cstdint>
#include "memmap.h"
#include<iostream>
#include<fstream>
#include<cstring>
#include<cassert>

const int memmap::timer_clock_select[4] = {
      256, //4096Hz
        4, //262144Hz
       16, //65536Hz
       64 }; //16384Hz
                            

memmap::memmap(const std::string& rom_filename, const std::string& fw_file) : 
                                                  screen(), sound(),
                                                  cart(rom_filename, fw_file),
                                                  int_enabled{0,0,0,0,0},
                                                  int_requested{0,0,0,0,0},
                                                  last_int_cycle(0), link_data(0), timer(0),timer_reset(0),timer_control(0),
                                                  timer_running(false),timer_deadline(0), div_clock(0), timer_clock(0)
{

    valid = cart.valid;
    if(!valid) return;

    directions.keys = 0x00;
    btns.keys = 0x00;
    wram.resize(0x2000);
    hram.resize(0x7f);

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
                    keyval |= btns.keys;
                }
                if((joypad & 0x10) == 0) {
                    keyval |= directions.keys;
                }
                *((uint8_t *)val) = (0xc0 | joypad | (~keyval));
            }
            //printf("Stubbed out read to gamepad (not implemented yet)\n");
            break;
        case 0xff01:
            std::cout<<"Read from link cable: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            break;
        case 0xff02:
            std::cout<<"Read from link cable: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            break;
        case 0xff04: //DIV register. 16KHz increment, (1024*1024)/16384=64, and the register overflows every 256 increments
            *(uint8_t *)val = ((cycle - div_reset) / 64) % 256;
            break;
        case 0xff05:
            std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            break;
        case 0xff06:
            std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            break;
        case 0xff07:
            std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            break;
        case 0xff0f:
            *((uint8_t *)val) = 0xe0 | int_requested.reg;
            //std::cout<<"Read from interrupt hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            break;
        case 0xff44: //TODO: Move this to PPU, base on global cycle count instead of frame cycle count
            *((uint8_t *)val) = (((cycle - screen.get_active_cycle()) % 17556) / 114);
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
                break;
            case 0xff01: //Fake implementation, for serial output from Blarg roms
                link_data = *((uint8_t *)val);
                //std::cout<<"Write to link cable: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
            case 0xff02: //Fake implementation
                {
                    int cmd = *((uint8_t *)val);
                    if(cmd == 0x81) {
                        std::cout<<"Blarg: "<<link_data<<std::endl;
                        link_data = 0xff;
                    }
                }
                //std::cout<<"Write to link cable: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
            case 0xff04:
                div_reset = cycle;
                //std::cout<<"Write to timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
            case 0xff05:
                //std::cout<<"Write to timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
            case 0xff06:
                //std::cout<<"Write to timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
            case 0xff07:
                //std::cout<<"Write to timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
            case 0xff0f:
                assert(size == 1);
                int_requested.reg = *((uint8_t *)val);
                //printf("Interrupts requested: %02X\n",int_requested.reg);
                //std::cout<<"Write to interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
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

void memmap::keydown(SDL_Scancode k) { //TODO: Implement joypad
    switch(k) {
        case SDL_SCANCODE_W:
            directions.up=1;
            break;
        case SDL_SCANCODE_A:
            directions.left=1;
            break;
        case SDL_SCANCODE_S:
            directions.down=1;
            break;
        case SDL_SCANCODE_D:
            directions.right=1;
            break;
        case SDL_SCANCODE_G:
            btns.select=1;
            break;
        case SDL_SCANCODE_H:
            btns.start=1;
            break;
        case SDL_SCANCODE_K:
            btns.b=1;
            break;
        case SDL_SCANCODE_L:
            btns.a=1;
            break;
        default:
            break;
    }
}

void memmap::keyup(SDL_Scancode k) { //TODO: Implement joypad
    switch(k) {
        case SDL_SCANCODE_W:
            directions.up=0;
            break;
        case SDL_SCANCODE_A:
            directions.left=0;
            break;
        case SDL_SCANCODE_S:
            directions.down=0;
            break;
        case SDL_SCANCODE_D:
            directions.right=0;
            break;
        case SDL_SCANCODE_G:
            btns.select=0;
            break;
        case SDL_SCANCODE_H:
            btns.start=0;
            break;
        case SDL_SCANCODE_K:
            btns.b=0;
            break;
        case SDL_SCANCODE_L:
            btns.a=0;
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

void memmap::update_interrupts(uint32_t frame, uint64_t cycle) {
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

    /*
    //TIMER
    if(int_enabled.timer) { printf("Warning: timer interrupt enabled, but not implemented yet.\n");}

    //SERIAL
    if(int_enabled.serial) { printf("Warning: serial interrupt enabled, but not implemented yet.\n");}

    //JOYPAD
    if(int_enabled.joypad) { printf("Warning: joypad interrupt enabled, but not implemented yet.\n");}
    */
}

bool memmap::has_firmware() {
    return cart.firmware;
}

lcd * memmap::get_lcd() {
    return &screen;
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
