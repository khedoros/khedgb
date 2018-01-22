#include<cstdint>
#include "memmap.h"
#include "rom.h"
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
                                                  screen(), cart(rom_filename, fw_file),
                                                  int_enabled{false,false,false,false,false},
                                                  int_requested{false,false,false,false,false},
                                                  last_int_frame(0),div_clock(0),timer_running(false),
                                                  timer(0),timer_reset(0),timer_control(0),timer_clock(0)
{
    wram.resize(0x2000);
    hram.resize(0x7f);

}

void memmap::dump_tiles() {
    screen.dump_tiles();
}

void memmap::read(int addr, void * val, int size, int cycle) {
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";
    if(addr >= 0 && addr < 0x8000) {
        cart.read(addr, val, size, cycle);
    }
    else if (addr >= 0x8000 && addr < 0xa000) {
        screen.read(addr, val, size, cycle);//memcpy(val, &(vram[addr-0x8000]), size);
    }
    else if (addr >= 0xa000 && addr < 0xc000) {
        cart.read(addr, val, size, cycle);
        //std::cout<<"Read from external RAM: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
    }
    else if (addr >= 0xc000 && addr < 0xfe00) { //8KB of memory, c000-dfff, then a partial mirror of it e000-fe00
        memcpy(val, &(wram[addr & 0x1fff]), size);
    }
    else if (addr >= 0xfe00 && addr < 0xfea0) {
        screen.read(addr, val, size, cycle);
    }
    else if (addr >= 0xff00 && addr < 0xff80) {
        switch(addr) {
        case 0xff00:
            *((uint8_t *)val) = 0xff;
            printf("Stubbed out read to gamepad (not implemented yet)\n");
            break;
        case 0xff44:
            *((uint8_t *)val) = (cycle / 114);
            break;
        default:
            if(addr < 0xff03) {
                std::cout<<"Read from gamepad/link cable: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff03 && addr < 0xff08) {
                std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr == 0xff0f) {
                *((uint8_t *)val) = int_requested.reg;
                //std::cout<<"Read from interrupt hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff0f && addr < 0xff3f) {
                std::cout<<"Read from audio hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
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
    else if (addr >= 0xff80 && addr < 0xffff) {
        memcpy(val, &(hram[addr - 0xff80]), size);
    }
    else if (addr == 0xffff) {
        //std::cerr<<"Attempted read of write-only register?"<<std::endl;
        *((uint8_t *)val) = int_enabled.reg;
    }
    else if (addr >= 0xffa0 && addr < 0xff00) {
        //"Not useable" RAM area; apparently returns 0 on DMG, 0 and random values on CGB
        memset(val, 0, size);
    }
    else {
        //std::cerr<<"Water fog? Trying to read from 0x"<<std::hex<<addr<<" bzzzzzzt"<<std::endl;
    }

}

void memmap::write(int addr, void * val, int size, int cycle) {
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";
    if(addr >= 0 && addr < 0x8000 || addr == 0xff50) {
        //std::cout<<"Write to ROM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (mappers not implemented yet)"<<std::endl;
        cart.write(addr,val,size,cycle);
    }
    else if (addr >= 0x8000 && addr < 0xa000) {
        screen.write(addr, val, size, cycle);
        //memcpy(&(vram[addr-0x8000]), val, size);
    }
    else if (addr >= 0xa000 && addr < 0xc000) {
        //std::cout<<"Write to external RAM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        cart.write(addr, val, size, cycle);
    }
    else if (addr >= 0xc000 && addr < 0xfe00) { //8KB RAM and mirrored area
        memcpy(&(wram[addr&0x1fff]), val, size);
    }
    else if (addr >= 0xfe00 && addr < 0xfea0) {
        screen.write(addr, val, size, cycle);
    }
    else if (addr >= 0xff00 && addr < 0xff80) {
        if(addr < 0xff03) {
            if(addr == 0xff01) {
                link_data = *((uint8_t *)val);
            }
            else if(addr == 0xff02) {
                int cmd = *((uint8_t *)val);
                if(cmd == 0x81) {
                    std::cout<<"Blarg: "<<link_data<<std::endl;
                    link_data = 0xff;
                }
            }
            std::cout<<"Write to gamepad/link cable: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff03 && addr < 0xff08) {
            std::cout<<"Write to timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr == 0xff0f) {
            assert(size == 1);
            int_requested.reg = *((uint8_t *)val);
            //printf("Interrupts requested: %02X\n",int_requested.reg);
            //std::cout<<"Write to interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff0f && addr < 0xff3f) {
            std::cout<<"Write to audio hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff3f && addr < 0xff4c) {
            if(addr == 0xff46) { //OAM DMA
                int dat = 0; //uint16_t(*((uint8_t *)val)) * 0x100;
                if(dat < 0xf100) {
                    //dat is between 0x00 and 0xf1, so that covers: ROM (00 to 7f), vram (80 to 9f), cram (a0-bf), wram (c0-df), wram_echo (e0-f1)
                    uint8_t temp[0xa0];
                    read(dat, (void *)(&temp[0]), 0xa0, cycle);
                    screen.write(0xff46, (void *)(&temp[0]), 0xa0, cycle);
                }
            }
            else {
                screen.write(addr, val, size, cycle);
            }
            //std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff4e && addr < 0xff6c) { //0xff50 is handled up above
                std::cout<<"Write to CGB DMA/RAM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else {
            std::cout<<"Write to unknown mem-mapped hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
    }
    else if (addr >= 0xff80 && addr < 0xffff) {
        memcpy(&(hram[addr - 0xff80]), val, size);
    }
    else if (addr == 0xffff) {
        assert(size == 1);
        int_enabled.reg = *((uint8_t *)val);
        //printf("Interrupts enabled: %02X\n",int_enabled.reg);
        //std::cout<<"Write to interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
    }
    else {
        //std::cerr<<"Water fog? Write to 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" bzzzzzzt"<<std::endl;
    }
}

void memmap::render(int frame,bool output_file) {
    screen.render(frame,output_file);
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

void memmap::update_interrupts(uint32_t frame, uint32_t cycle) {
    //VBLANK
    if(frame > last_int_frame && cycle >= 144*114) {
        int_requested.vblank = 1; 
        last_int_frame = frame; 
        //printf("vbl set active, frame %d, cycle %d\n", frame, cycle);
    }

    //LCDSTAT
    if(screen.interrupt_triggered(frame, cycle)) {
        int_requested.lcdstat = 1;
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
