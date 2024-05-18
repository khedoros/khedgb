#include<cstdint>
#include "memmap.h"
#include<iostream>
#include<fstream>
#include<cstring>
#include<cassert>

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT //assert
#endif

const unsigned int memmap::timer_clock_select[4] = {
      256, //4096Hz
        4, //262144Hz
       16, //65536Hz
       64 }; //16384Hz
                            

memmap::memmap(const std::string& rom_filename, const std::string& fw_file) : 
/*hardware connected to the memmap*/              screen(), sound(), cart(rom_filename, fw_file), p(),
/*interrupt hardware              */              int_enabled{0,0,0,0,0}, int_requested{0,0,0,0,0}, last_int_cycle(0),  joypad(0xc0),
/*Super GameBoy values            */              sgb_active(false), sgb_bit_ptr(0), sgb_buffered(false), sgb_cur_joypad(0), sgb_joypad_count(1), sgb_cmd_count(0), sgb_cmd_index(0),
/*serial/link cable               */              link_data(0), serial_transfer(false), internal_clock(false), transfer_start(-1), bits_transferred(0),
/*div register + timer            */              div(0), div_period(0),last_int_check(0), timer(0), timer_modulus(0), timer_control{.val=0},
/*                                */              clock_divisor_reset(timer_clock_select[0]), clock_divisor(0), screen_status(0), be_speedy(false),
                                                  hdma_hblank(false), hdma_general(false), hdma_running(false), hdma_first(false), next_apu_cycle(0) {

    valid = cart.valid;
    if(!valid) return;

    for(int i=0;i<4;i++) {
        directions[i].keys = 0x00;
        btns[i].keys = 0x00;
    }

    wram_bank = 1;
    wram.resize(0x2000);
    hram.resize(0x7f);
    sgb_buffer.resize(16,0);

    if(fw_file == "") {
        uint8_t temp = 0;
        temp = 0x00; write(0xFF05, temp, 0);    //; TIMA
        temp = 0x00; write(0xFF06, temp, 0);    //; TMA
        temp = 0x00; write(0xFF07, temp, 0);    //; TAC
        temp = 0x80; write(0xFF10, temp, 0);    //; NR10
        temp = 0xBF; write(0xFF11, temp, 0);    //; NR11
        temp = 0xF3; write(0xFF12, temp, 0);    //; NR12
        temp = 0xBF; write(0xFF14, temp, 0);    //; NR14
        temp = 0x3F; write(0xFF16, temp, 0);    //; NR21
        temp = 0x00; write(0xFF17, temp, 0);    //; NR22
        temp = 0xBF; write(0xFF19, temp, 0);    //; NR24
        temp = 0x7F; write(0xFF1A, temp, 0);    //; NR30
        temp = 0xFF; write(0xFF1B, temp, 0);    //; NR31
        temp = 0x9F; write(0xFF1C, temp, 0);    //; NR32
        temp = 0xBF; write(0xFF1E, temp, 0);    //; NR33
        temp = 0xFF; write(0xFF20, temp, 0);    //; NR41
        temp = 0x00; write(0xFF21, temp, 0);    //; NR42
        temp = 0x00; write(0xFF22, temp, 0);    //; NR43
        temp = 0xBF; write(0xFF23, temp, 0);    //; NR30
        temp = 0x77; write(0xFF24, temp, 0);    //; NR50
        temp = 0xF3; write(0xFF25, temp, 0);    //; NR51
        temp = 0xF1; write(0xFF26, temp, 0);    //; NR52
        temp = 0x91; write(0xFF40, temp, 0);    //; LCDC
        temp = 0x00; write(0xFF42, temp, 0);    //; SCY
        temp = 0x00; write(0xFF43, temp, 0);    //; SCX
        temp = 0x00; write(0xFF45, temp, 0);    //; LYC
        temp = 0xFC; write(0xFF47, temp, 0);    //; BGP
        temp = 0xFF; write(0xFF48, temp, 0);    //; OBP0
        temp = 0xFF; write(0xFF49, temp, 0);    //; OBP1
        temp = 0x00; write(0xFF4A, temp, 0);    //; WY
        temp = 0x00; write(0xFF4B, temp, 0);    //; WX
        temp = 0x00; write(0xFFFF, temp, 0);    //; IE
    }
}

uint32_t memmap::readmore(int addr, int size, uint64_t cycle) {
    uint32_t retval = 0;
    //printf("readmore: ");
    for(int i=0;i<size;i++) {
        uint32_t temp = read(addr+i, cycle + i);
        //printf("read %02x from %04x, ", temp, addr+i);
        temp<<=(8*i);
        retval|=temp;
    }
    //printf("Returning %08x\n", retval);
    return retval;
}

uint8_t memmap::read(int addr, uint64_t cycle) {
    uint64_t cycle2=(cycle>>1);
    uint8_t retval = 0;
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";

    //CARTRIDGE AND RAM
    if(addr < 0x8000 || (addr >= 0xa000 && addr < 0xc000)) { //Cartridge ROM 0x0000-0x7fff, External RAM 0xa000-0xbfff
        retval = cart.read(addr, cycle);
    }
    else if ((addr >= 0xc000 && addr < 0xd000) || (addr >= 0xe000 && addr < 0xf000)) { //4KB of working memory (WRAM), c000-cfff, then a mirror of it e000-efff
        retval = wram[addr & 0xfff];
        //printf("read wram0:%04x (%02x)\n", addr, wram[addr & 0xfff]);
    }
    else if ((addr >= 0xd000 && addr < 0xe000) || (addr >= 0xf000 && addr < 0xfe00)) { //28KB of memory in 7 banks, d000-dfff, then a partial mirror of it f000-fdff
        int tmpbank = wram_bank; if(!tmpbank) tmpbank = 1;
        retval = wram[(addr & 0xfff) + tmpbank * 0x1000];
        //printf("read wram%d:%04x (%02x)\n", tmpbank, addr, wram[(addr & 0xfff) + tmpbank * 0x1000]);
    }
    else if ((addr >= 0x8000 && addr < 0xa000) || (addr >= 0xfe00 && addr < 0xfea0)) { //VRAM 0x8000-0x9fff, OAM 0xfe00-0xfe9f
        retval = screen.read(addr, cycle2);//memcpy(val, &(vram[addr-0x8000]), size);
    }
    else if (addr >= 0xfea0 && addr < 0xff00) {
        //"Not useable" RAM area; apparently returns 0 on DMG, 0 and random values on CGB
        retval = 0;
    }
    else if (addr >= 0xff00 && addr < 0xff80 || addr == 0xffff) { //I/O registers
        switch(addr) {
        case 0xff00: //Joypad register
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
                retval = (0xc0 | joypad | (~keyval));
                //printf("Joy: returning %02x\n", uint8_t(0xc0|joypad|(~keyval)));
            }
            //printf("Stubbed out read to gamepad (not implemented yet)\n");
            break;
        case 0xff01: //Serial data
            //printf("Read from serial data: 0x%04x (got 0x%02x, and link_in is 0x%02x)\n", addr, link_data, link_in_data);
            retval = link_data;
            break;
        case 0xff02: //Serial control
            retval = (serial_transfer * 0x80) | 0x7e | internal_clock;
            //printf("Read from serial control: 0x%04x (got 0x%02x)\n", addr, (serial_transfer * 0x80) | internal_clock);
            break;
        case 0xff04: //DIV register. 16KHz increment, (1024*1024)/16384=64, and the register overflows every 256 increments
            retval = div;
            //std::cout<<"Read from div timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<uint64_t(div)<<std::endl;
            break;
        case 0xff05: //Timer counter
            retval = timer;
            //std::cout<<"Read from timer hardware (time): 0x"<<std::hex<<addr<<" = 0x"<<uint64_t(timer)<<std::endl;
            break;
        case 0xff06: //Timer modulus
            retval = timer_modulus;
            //std::cout<<"Read from timer hardware (modulus): 0x"<<std::hex<<addr<<" = 0x"<<uint64_t(timer_modulus)<<std::endl;
            break;
        case 0xff07: //Timer control
            retval = timer_control.val;
            //std::cout<<"Read from timer hardware (control): 0x"<<std::hex<<addr<<"= 0x"<<timer_control.val<<std::endl;
            break;
        case 0xff0f: //Active interrupts
            retval = int_requested.reg;
            //std::cout<<"Read from interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(int_requested.reg)<<" at cycle "<<std::dec<<cycle<<std::endl;
            break;
        case 0xff4d: //CGB KEY1 speed switch register
            retval = (0x7f | (be_speedy * 0x80));
            //printf("Read from unimplemented KEY1 speed switch register\n");
            break;
        case 0xff4f: //VBK (CGB VRAM bank)
            //printf("Read from CGB VRAM bank register\n");
            retval = screen.read(addr, cycle2);
            break;
        case 0xff51:
            //printf("Read from CGB HDMA1 (DMA source, high byte)\n");
            retval = 0xff;
            break;
        case 0xff52:
            //printf("Read from CGB HDMA2 (DMA source, low byte)\n");
            retval = 0xff;
            break;
        case 0xff53:
            //printf("Read from CGB HDMA3 (DMA destination, high byte)\n");
            retval = 0xff;
            break;
        case 0xff54:
            //printf("Read from CGB HDMA4 (DMA destination, low byte)\n");
            retval = 0xff;
            break;
        case 0xff55:
            if(!cgb) retval = 0xff;

            if(!hdma_running && !hdma_hblank) { //HDMA is stopped, not paused
                retval = 0xff;
            }
            else if(!hdma_running && hdma_hblank) { //HDMA is paused in HBlank mode
                retval = (0x80 | (hdma_chunks - 1));
            }
            else { //HDMA is currently running every HBlank
                retval = hdma_chunks - 1;
            }
            //printf("Read from CGB HDMA5 (DMA length/mode/start): %02x\n", *(uint8_t *)val);
            break;
        case 0xff56: //CGB infrared communication port
            //TODO: Write it, after CGB, and probably along with serial support.
            //bit0: write data (RW), bit1: read data (RO), bit6+7 data read enable (3=enable)
            retval = 0;
            printf("Read from unimplemented IR communication register\n");
            break;
        case 0xff68:
                //BIT0-5 are the byte index. byte 0 is pal0/col0/lsb, byte 63 is pal7/col3/msb (8 pals, 4 colors, 2 bytes = 64 bytes)
                //BIT7 says whether to increment address on write.
                //printf("Read from CGB BG palette index\n");
                retval = screen.read(addr, cycle2);
                break;
        case 0xff69:
                //Writes data to specified palette byte
                //printf("Read from CGB BG palette data port\n");
                retval = screen.read(addr, cycle2);
                break;
        case 0xff6a:
                //OBJ palettes work the same as BG palettes.
                //printf("Write to CGB OBJ palette index\n");
                retval = screen.read(addr, cycle2);
                break;
        case 0xff6b:
                //printf("Read from CGB OBJ palette data port\n");
                retval = screen.read(addr, cycle2);
                break;
        case 0xff70: //CGB WRAM size switch
            if(!cgb) {
                retval = 0xff;
            }
            else {
                retval = wram_bank;
            }
            //printf("Read of CGB WRAM bank select register\n");
            break;
        case 0xffff:
            retval = int_enabled.reg;
            break;
        default:
            if(addr > 0xff0f && addr <= 0xff3f) {
                //std::cout<<"Read from audio hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
                retval = sound.read(addr, cycle2);
            }
            else if(addr > 0xff3f && addr < 0xff4c) {
                retval = screen.read(addr, cycle2);
                //std::cout<<"Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff4e && addr < 0xff6c) { //Covers addresses in that range that aren't handled above
                //std::cout<<"Read from CGB DMA/RAM: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
                retval = 0xff;
            }
            else {
                retval = 0xff;
                //std::cout<<"Read from unknown mem-mapped hardware: 0x"<<std::hex<<addr<<" (assuming it would read 0xff)"<<std::endl;
            }
        }
    }
    else if (addr >= 0xff80 && addr < 0xffff) { //127 bytes High RAM
        retval = hram[addr & 0x7f];
    }
    else {
        retval = 0xff;
        //std::cerr<<"Water fog? Trying to read from 0x"<<std::hex<<addr<<" bzzzzzzt"<<std::endl;
    }
    return retval;
}

void memmap::writemore(int addr, uint32_t val, int size, uint64_t cycle) {
    //printf("writemore got %04x = %08x (%d), ", addr, val, size);
    for(int i=0;i<size;i++) {
        //printf("writing %02x to %04x, ", (val&0xff), addr + i);
        write(addr+i, uint8_t(val&0xff), cycle + i);
        val>>=8;
    }
    //printf("\n");
}

void memmap::write(int addr, uint8_t val, uint64_t cycle) {
    uint64_t cycle2 = cycle / 2;
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";
    if((addr >= 0 && addr < 0x8000) || (addr >= 0xa000 && addr < 0xc000)) { //Cartridge ROM, cartridge RAM
        //std::cout<<"Write to ROM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (mappers not implemented yet)"<<std::endl;
        cart.write(addr,val,cycle);
    }
    else if ((addr >= 0x8000 && addr < 0xa000) || addr >= 0xfe00 && addr < 0xfea0) { //Video RAM, OAM RAM
        screen.write(addr, val, cycle2);
    }
    else if ((addr >= 0xc000 && addr < 0xd000) || (addr >= 0xe000 && addr < 0xf000)) { //4KB of memory, c000-cfff, then a mirror of it e000-efff
        wram[addr&0xfff] = val;
        //printf("write wram%d:%04x = %02x\n", 0, addr, wram[(addr & 0xfff)]);
    }
    else if ((addr >= 0xd000 && addr < 0xe000) || (addr >= 0xf000 && addr < 0xfe00)) { //28KB of memory in 7 banks, d000-dfff, then a partial mirror of it f000-fdff
        int tmpbank = wram_bank; if(!tmpbank) tmpbank = 1;
        wram[(addr&0xfff) + tmpbank * 0x1000] = val;
        //printf("write wram%d:%04x = %02x\n", tmpbank, addr, wram[(addr & 0xfff) + tmpbank * 0x1000]);
    }
    else if (addr >= 0xff00 && addr < 0xff80 || addr == 0xffff) { //Hardware IO area
        switch(addr) {
            case 0xff00:
                joypad = (val & 0x30);
                //printf("Joypad received %02x\n", joypad);
                if(joypad == 0 && sgb_active) { //SGB Low Pulse (reset)
                    sgb_bit_ptr = -1;
                    sgb_buffered = false;
                    for(int i=0;i<16;++i) sgb_buffer[i]=0;
                    //printf(" reset\n");
                }
                else if(joypad == 0x30 && sgb_active) { //Both channels deselected
                    if(!sgb_buffered) { //SGB High Pulse (value buffer)
                        sgb_buffered = true;
                        sgb_bit_ptr++;
                        //printf(" buffer\n");
                    }
                }
                else if(joypad == 0x20 && sgb_bit_ptr < 128 && sgb_buffered && sgb_active) { //SGB "0"
                    //Array is initialized to 0, so I don't need to actually *set* 0.
                    sgb_buffered = false;
                    //printf(" 0\n");
                }
                else if(joypad == 0x10 && sgb_bit_ptr < 128 && sgb_buffered && sgb_active) { //SGB "1"
                    int byte = sgb_bit_ptr / 8;
                    int bit = sgb_bit_ptr % 8;
                    sgb_buffer[byte] |= 1<<(bit);
                    sgb_buffered = false;
                    //printf(" 1 (byte %d, bit %d)\n",byte,bit);
                }
                else if(joypad == 0x20 && sgb_bit_ptr == 128 && sgb_buffered && sgb_active) { //SGB STOP bit
                    sgb_buffered = false;
                    //printf(" end\n");
                    sgb_exec(sgb_buffer, cycle);
                }
                else {
                    //printf("\n");
                }
                break;
            case 0xff01:
                link_data = val;
                //printf("Write to serial data: 0x%04x = 0x%02x\n", addr, *(uint8_t *)val);
                break;
            case 0xff02:
                {
                    int cmd = val;
                    if(cmd == 0x81) { //Immediate output for Blarg/debug stuff
                        if(link_data >= 32 && link_data <= 126) {
                            //NOTE: Turn this on for serial output from the 
                            //std::cout<<"Blarg: "<<link_data<<std::endl;
                        }
                        //link_data = 0xff;
                        link_in_data = p.send(link_data);
                    }
                    serial_transfer = ((cmd & 0x80) == 0x80);
                    internal_clock = ((cmd & 0x01) == 0x01);
                    //high_speed = ((cmd & 0x02) == 0x02); //TODO: High speed mode for CGB
                    if(serial_transfer) {
                        bits_transferred = 0;
                        transfer_start = cycle;
                    }
                }
                //printf("Write to serial: 0x%04x = 0x%02x\n", addr, *(uint8_t *)val);
                break;
            case 0xff04:
                div = 0;
                //printf("Write to div hardware: 0x%04x = 0x%02x, at cycle %ld (DIV set to 0)\n", addr, *(uint8_t *)val, cycle);
                break;
            case 0xff05:
                timer = val;
                //printf("Write to timer hardware (timer): 0x%04x = 0x%02x, at cycle %lld\n", addr,timer,cycle);
                break;
            case 0xff06:
                timer_modulus = val;
                //printf("Write to timer hardware (modulus): 0x%04x = 0x%02x, at cycle %lld\n", addr, timer_modulus, cycle);
                break;
            case 0xff07: 
                {
                    timer_control.low = (val & 0x07);
                    clock_divisor_reset = ((be_speedy)?1:2) * timer_clock_select[timer_control.clock];
                    clock_divisor = 0;
                }
                //printf("Write to timer hardware (control): 0x%04x = 0x%02x, at cycle %lld, running: %d, divisor: %d\n",
                //        addr, int(*(uint8_t *)val), cycle, timer_control.running, timer_clock_select[timer_control.clock]);
                break;
            case 0xff0f:
                int_requested.reg = val;
                //printf("Interrupts requested: %02X\n",int_requested.reg);
                break;
            case 0xff46: //OAM DMA, decomposed into a set of individual writes
                {
                    uint16_t src_addr = uint16_t(val) * 0x100;
                    //printf("DMA from 0x%04x\n", src_addr);
                    if(src_addr < 0xf100) {
                        //dat is between 0x00 and 0xf1, so that covers: ROM (00 to 7f), vram (80 to 9f), cram (a0-bf), wram (c0-df), wram_echo (e0-f1)
                        screen.dma(true, cycle2, val);
                        for(uint16_t dest_index = 0; dest_index < 0xa0; dest_index++) {
                            uint8_t data = read(src_addr+dest_index, cycle2 + dest_index);
                            screen.write(0xfe00 + dest_index, data, cycle2 + dest_index);
                        }
                        screen.dma(false, cycle2 + 0xa0, val);
                    }
                }
                break;
            case 0xff4d: //KEY1 Prepare speed switch
                //printf("Write to CGB speed-switching register: %02x\n", val);
                if(cgb && val & 0x01) feel_the_need = true;
                break;
            case 0xff4f: //VBK (CGB VRAM bank)
                //printf("Write to CGB VRAM bank register: %02x\n", val);
                screen.write(addr, val, cycle2);
                break;
            case 0xff50: //disables CPU firmware
                cart.write(addr, val, cycle);
                if(cart.color_firmware && !cart.supports_color()) {
                    screen.cgb_enable(false);
                }
                break;
            case 0xff51: //Allowable sources: 0-7fff, a000-bfff, c000-dfff. e000-ffff reads from a000-bfff, 8000-9fff reads incorrect crap
                //printf("HDMA1 (DMA source, high byte): %02x\n", *(uint8_t *)val);
                hdma_src = (256 * uint16_t(val)) | (hdma_src & 0xff);
                break;
            case 0xff52:
                //printf("HDMA2 (DMA source, low byte): %02x\n", *(uint8_t *)val);
                hdma_src = (hdma_src & 0xff00) | (val & 0xf0);
                break;
            case 0xff53:
                //printf("HDMA3 (DMA destination, high byte): %02x\n", *(uint8_t *)val);
                hdma_dest = ((256 * (uint16_t(val) & 0x1f)) | (hdma_dest & 0xff)) + 0x8000;
                break;
            case 0xff54:
                //printf("HDMA4 (DMA destination, low byte): %02x\n", *(uint8_t *)val);
                hdma_dest = (hdma_dest & 0xff00) | (val & 0xf0);
                break;
            case 0xff55:
                if(!cgb) break;
                //printf("HDMA5 (DMA length/mode/start): ");
                if(hdma_hblank && hdma_running && (val & 0x80) == 0) { //Stop HDMA copy
                    hdma_running = false;
                    hdma_chunks = (0x7f & val) + 1;
                    //printf("Pause HDMA\n");
                }
                else if(hdma_hblank && (val & 0x80) == 0x80) { //Restart HDMA copy
                    hdma_running = true;
                    hdma_chunks = (val & 0x7f) + 1;
                    //hdma_first = true;
                    //printf("Restart HDMA\n");
                }
                else {
                    if((0x80 & val) == 0x80) {
                        hdma_hblank = true;
                        //hdma_first = true;
                        hdma_last_mode = 1;//screen.get_mode(cycle2); (just need a non-zero value, so the copy will start if begun in HBlank
                        //printf("start HDMA, ");
                    }
                    else {
                        hdma_general = true;
                        hdma_hblank = false;
                        //printf("start GDMA, ");
                    }
                    hdma_running = true;
                    hdma_chunks = (val & 0x7f) + 1;
                    //printf("copy %d blocks from %04x to %04x (wrote %02x)\n", hdma_chunks, hdma_src, hdma_dest, *(uint8_t *)val);
                }
                break;
            case 0xff56: //CGB IR Comm register
                if(!cgb) break;
                //bit0: write data (RW), bit1: read data (RO), bit6+7 data read enable (3=enable)
                printf("Write to CGB IR port: %02x\n", val);
                break;
            case 0xff68:
                if(!cgb) break;
                //BIT0-5 are the byte index. byte 0 is pal0/col0/lsb, byte 63 is pal7/col3/msb (8 pals, 4 colors, 2 bytes = 64 bytes)
                //BIT7 says whether to increment address on write.
                //printf("Write to CGB BG palette index: %02x\n", *(uint8_t *)val);
                screen.write(addr, val, cycle2);
                break;
            case 0xff69:
                if(!cgb) break;
                //Writes data to specified palette byte
                //printf("Write to CGB BG palette data port: %02x\n", *(uint8_t *)val);
                screen.write(addr, val, cycle2);
                break;
            case 0xff6a:
                if(!cgb) break;
                //OBJ palettes work the same as BG palettes.
                //printf("Write to CGB OBJ palette index: %02x\n", *(uint8_t *)val);
                screen.write(addr, val, cycle2);
                break;
            case 0xff6b:
                if(!cgb) break;
                //printf("Write to CGB OBJ palette data port: %02x\n", *(uint8_t *)val);
                screen.write(addr, val, cycle2);
                break;
            case 0xff70: //CGB WRAM page switch
                if(!cgb) break;
                wram_bank = (val & 7);
                //printf("Write to CGB WRAM page setting: %02x\n", *(uint8_t *)val);
                break;
            case 0xffff: //Interrupt enabled register
                int_enabled.reg = (0xe0 | val);
                //printf("Interrupts enabled: %02X\n",int_enabled.reg);
                //std::cout<<"Write to interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
            default:
                if(addr > 0xff0f && addr <= 0xff3f) {
                    //std::cout<<"Write to audio hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                    sound.write(addr, val, cycle2);
                    //std::cout<<std::dec<<cycle2<<" "<<std::hex<<addr<<" "<<uint16_t(val)<<'\n';
                }
                else if(addr > 0xff3f && addr < 0xff4c) {
                    screen.write(addr, val, cycle2);
                    if(addr == 0xff40) screen_status = val;
                    //std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                }
                else if(addr > 0xff4e && addr < 0xff6c) { //0xff50 is handled up above
                    std::cout<<"Write to CGB DMA/RAM: 0x"<<std::hex<<addr<<" = 0x"<<int(val)<<" (not implemented yet)"<<std::endl;
                }
                else {
                    std::cout<<"Write to unknown mem-mapped hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(val)<<" (not implemented yet)"<<std::endl;
                }
                break;
        }
    }
    else if (addr >= 0xff80 && addr < 0xffff) { //127 bytes of HRAM
        hram[addr - 0xff80] = val;
    }
    else {
        //std::cerr<<"Water fog? Write to 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" bzzzzzzt"<<std::endl;
    }
}

uint8_t memmap::ienab() {
    return int_enabled.reg;
}

uint8_t memmap::iflag() {
    return int_requested.reg;
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
        case SDL_SCANCODE_E:
            screen.toggle_debug();
            break;
        case SDL_SCANCODE_R:
            sound.toggle_debug();
            break;
        case SDL_SCANCODE_O:
            screen.sgb_trigger_dump(std::string("vram_dump.dat"));
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
        case SDL_SCANCODE_T:
            screen.toggle_trace();
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
    uint64_t time_diff = cycle - last_int_check;
    uint64_t cycle2 = (cycle>>1);
    //uint8_t enabled = 0;
    //screen.read(0xff40, &enabled, 1, cycle);
    //We aren't still in previously-seen vblank, we *are* in vblank, and the screen is enabled
    if(!int_requested.vblank && cycle2 > last_int_cycle + 1140 && screen.get_mode(cycle2) == 1 && ((screen_status&0x80) > 0)) {
        int_requested.vblank = 1;
        last_int_cycle = cycle2; 
        //printf("INT: vbl set active @ cycle %ld\n", cycle);
    }

    //LCDSTAT
    if(!int_requested.lcdstat && screen.interrupt_triggered(cycle2)) {
        int_requested.lcdstat = 1;
        //printf("INT: lcdstat set active @ cycle %ld\n", cycle);
    }

    //TIMER
    if(timer_control.running) {
        //printf("Timer: %d Clock divisor: %d Clock divisor reset: %d Timer modulus: %d\n", timer, clock_divisor, clock_divisor_reset, timer_modulus);
        clock_divisor += time_diff;
        if(clock_divisor >= clock_divisor_reset) {
           uint16_t num_of_steps = clock_divisor / clock_divisor_reset;
           uint8_t old_timer = timer;
           timer+=num_of_steps;
           clock_divisor -= (clock_divisor_reset * num_of_steps);
           if(timer == 0 || timer < old_timer || clock_divisor_reset <= num_of_steps) {
               int_requested.timer = 1;
               timer = timer_modulus;
               //printf("Triggered timer interrupt at cycle %lld, reloading with modulus: %d\n", cycle, timer_modulus);
           }
       }

    }
    //if(int_enabled.timer) { printf("Warning: timer interrupt enabled, but not implemented yet.\n");}

    //SERIAL
    if(!int_requested.serial && serial_transfer && internal_clock && cycle >= transfer_start) {
        int bit = (cycle - transfer_start) / 128;
        //std::cout<<"Transfer start: "<<transfer_start<<" "<<cycle<<std::endl;
        //ASSERT(bit < 256);
        if(bit == (bits_transferred + 1)) { //Passed the time to transfer a new bit
            //printf("Serial shift, current values (before shift): out: %02x, in: %02x\n", link_data, link_in_data);
            bits_transferred = bit;
            link_data<<=(1);
            link_data |= ((link_in_data&0x80)==0x80)?1:0;
            link_in_data<<=1;
        }
        if(bit == 8) { //Bit has completed transfer
            //printf("Serial shift should be done, out: %02x, in: %02x\n", link_data, link_in_data);
            //link_data = p.send(link_data);
            bits_transferred = 0;
            serial_transfer = false;
            transfer_start = -1;
            int_requested.serial = 1;
        }
    }
    //if(int_enabled.serial) { printf("Warning: serial interrupt enabled, but not implemented yet.\n");}


    //JOYPAD: Not dependent on time; its state is automatically updated when input events are processed.
    //if(int_enabled.joypad) { printf("Warning: joypad interrupt enabled, but not implemented yet.\n");}
    //
    div_period += time_diff;
    if(div_period >= 64) {
        div++;
        div_period -= 64;
    }
    last_int_check = cycle;

    //Clock the APU/generate audio every 1/64th of a second. Generates 689 samples (per channel) for 15 calls, then 690 for the 16th.
    if(cycle2 >= next_apu_cycle) {
        /*
        if(cycle2 - next_apu_cycle > 20) {
            printf("how late I am: %d\n", cycle2 - next_apu_cycle);
        }
        */
        sound.run(cycle2);
        next_apu_cycle += 16384;
    }
}

bool memmap::has_firmware() {
    return cart.firmware;
}

lcd * memmap::get_lcd() {
    return &screen;
}

apu * memmap::get_apu() {
    return &sound;
}

void memmap::win_resize(unsigned int newx, unsigned int newy) {
    screen.win_resize(newx, newy);
}

bool memmap::set_sgb(bool active) {
    if(cart.supports_sgb()) {
        sgb_active = active;
        screen.sgb_enable(active);
    }
    return sgb_active;
}

bool memmap::supports_sgb() {
    return cart.supports_sgb();
}

void memmap::sgb_exec(Vect<uint8_t>& s_b, uint64_t cycle) {
    //If we're on the first packet, read the data
    if(sgb_cmd_index == 0) {
        sgb_cmd = s_b[0]>>3; //what's the command in this packet
        sgb_cmd_count = s_b[0] & 0x07; //how many packets are expected
        if(sgb_cmd_count == 0) {
            return;
        }
        sgb_cmd_data.resize(sgb_cmd_count*16); //allocate space to store the data
        //printf("SGB command %02x, expected %02x packets\n", sgb_cmd, sgb_cmd_count);
    }

    //printf("Command data size: %d, expected: %d\n", sgb_cmd_data.size(), sgb_cmd_count * 16);
    ASSERT(sgb_cmd_data.size() == sgb_cmd_count * 16);

    //printf("%d / %d: ", sgb_cmd_index+1, sgb_cmd_count);

    if(sgb_cmd_index == 0) {
        //printf("0x%02x 0x%02x ", sgb_cmd, sgb_cmd_count);
    }
    else {
        //printf("          ");
    }

    for(int i=0; i < 16; ++i) {
        sgb_cmd_data[sgb_cmd_index * 16 + i] = s_b[i];
        //printf("%02x ", s_b[i]);
    }

    sgb_cmd_index++;

    if(sgb_cmd_count == sgb_cmd_index) {
        sgb_cmd_index = 0;
        //printf("SGB_cmd");
        switch(sgb_cmd) {
            case 0x00: //PAL01
            case 0x01: //PAL23
            case 0x02: //PAL03
            case 0x03: //PAL12
                {
                uint8_t pal1 = 0;
                uint8_t pal2 = 0;
                switch(sgb_cmd) {
                    case 0: pal1=0; pal2=1; break;
                    case 1: pal1=2; pal2=3; break;
                    case 2: pal1=0; pal2=3; break;
                    case 3: pal1=1; pal2=2; break;
                }
                //printf(": PAL%d%d    Set palettes %d and %d\n", pal1,pal2,pal1,pal2);
                Arr<uint16_t, 7> pals;
                for(int col=0;col<14;col+=2) {
                    pals[col/2] = 256*sgb_cmd_data[col+2] + sgb_cmd_data[col+1];
                }
                screen.sgb_set_pals(pal1,pal2,pals);
                }
                break;
            case 0x04: //ATTR_BLK
                {
                    Arr<uint8_t, 360> attrs;
                    for(auto& i:attrs) i=10;
                    int data_groups = sgb_cmd_data[1] & 0x1f;
                    //printf(": ATTR_BLK Apply color palette attributes to %d areas\n", data_groups);
                    for(int group=0;group<data_groups;++group) {
                        int offset = 2 + group * 6;
                        int control = sgb_cmd_data[offset];
                        int pal_in = (sgb_cmd_data[1 + offset] & 0x03);
                        int pal_on = ((sgb_cmd_data[1 + offset]>>2) & 0x03);
                        int pal_out = ((sgb_cmd_data[1 + offset]>>4) & 0x03);
                        int x0 = sgb_cmd_data[2+offset] & 0x1f;
                        int y0 = sgb_cmd_data[3+offset] & 0x1f;
                        int x1 = sgb_cmd_data[4+offset] & 0x1f;
                        int y1 = sgb_cmd_data[5+offset] & 0x1f;
                        //printf("\tFor (%02x, %02x) - (%02x, %02x), ", x0, y0, x1, y1);
                        bool change_inside = ((control & 1) == 1);
                        bool change_border = ((control & 2) == 2);
                        bool change_outside = ((control & 4) == 4);
                        if(!(change_inside||change_border||change_outside)) {
                            //printf("nothing");
                        }
                        else {
                            if(change_inside) {
                                printf("%d to inside ", pal_in);
                                for(int i=y0+1;i<y1;i++) {
                                    for(int j=x0+1;j<x1;j++) {
                                        attrs[i*20+j] = pal_in;
                                    }
                                }
                            }
                            if(change_border) {
                                //printf("%d to border ", pal_on);
                                for(int i=y0;i<=y1;i++) {
                                    attrs[i*20+x0] = pal_on;
                                    attrs[i*20+x1] = pal_on;
                                }

                                for(int i=x0+1;i<x1;i++) {
                                    attrs[y0*20+i] = pal_on;
                                    attrs[y1*20+i] = pal_on;
                                }
                            }
                            if(change_outside) {
                                //printf("%d to outside ", pal_out);
                                for(int i=0;i<18;i++) {
                                    for(int j=0;j<20;j++) {
                                        if(i >= y0 && i <= y1 && j >= x0 && j <= x1) continue;
                                        attrs[i*20+j] = pal_out;
                                    }
                                }
                            }
                        }
                        //printf("\n");
                    }
                    screen.sgb_set_attrs(attrs);
                }
                break;
            case 0x05: //ATTR_LIN
                {
                    //printf(": ATTR_LIN Set individual lines of palette attributes:\n");
                    Arr<uint8_t, 360> attrs;
                    for(auto& i:attrs) i=10;
                    int count = sgb_cmd_data[1];
                    for(int line = 0; line < count; line++) {
                        int line_num = (sgb_cmd_data[line+2] & 0x1f);
                        int pal_num  = ((sgb_cmd_data[line+2]>>5)&3);
                        bool hv = ((sgb_cmd_data[line+2] & 0x80) == 0x80);
                        //printf("\tLine %d: num: %d set to pal: %d h/v: %d\n", line, line_num, pal_num, hv);
                        if(hv) for(int i = 0; i < 20; i++) attrs[line_num * 20 + i] = pal_num;
                        else   for(int i = 0; i < 18; i++) attrs[i * 20 + line_num] = pal_num;
                    }
                    screen.sgb_set_attrs(attrs);
                }
                break;  
            case 0x06: //ATTR_DIV
                {
                    //printf(": ATTR_DIV Divide palette attributes by line: ");
                    Arr<uint8_t, 360> attrs;
                    for(auto& i:attrs) i=10;
                    int br = (sgb_cmd_data[1] & 0x03);
                    int tl = ((sgb_cmd_data[1]>>2) & 0x03);
                    int on = ((sgb_cmd_data[1]>>4) & 0x03);
                    bool hv = ((sgb_cmd_data[1] & 0x40) == 0x40);
                    int line = sgb_cmd_data[2] & 0x1f;
                    if(!hv) { //vertical
                        //printf("Divide at x=%d, left to %d, line to %d, right to %d\n", line, tl, on, br);
                        for(int j = 0; j < 18; j++) {
                            for(int i = 0; i < line; i++) attrs[j*20+i] = tl;
                            attrs[j*20+line] = on;
                            for(int i=line+1;i<20;i++) attrs[j*20 + i] = br;
                        }
                    }
                    else { //Horizontal
                        //printf("Divide at y=%d, top to %d, line to %d, bottom to %d\n", line, tl, on, br);
                        for(int i = 0; i < 20; i++) {
                            for(int j = 0; j < line; j++) attrs[j*20+i] = tl;
                            attrs[line*20+i] = on;
                            for(int j=line+1;j<18;j++) attrs[j*20 + i] = br;
                        }
                    }
                    screen.sgb_set_attrs(attrs);
                }
                break;
            case 0x07: //ATTRIBUTE_CHR
                {
                    //printf(": ATTR_CHR explicitly specify palette attributes:\n");
                    Arr<uint8_t, 360> pal_attrs;
                    for(auto& i:pal_attrs) i=0;
                    int x0 = sgb_cmd_data[1];
                    int y0 = sgb_cmd_data[2];
                    int items = sgb_cmd_data[4] * 256 + sgb_cmd_data[3];
                    if(items > 360) items = 360;
                    //printf("x0: %d y0: %d items: %d\n", x0,y0,items);
                    bool hv = ((sgb_cmd_data[5] & 1) == 1);
                    for(int i=0; i<items; i++) {
                        int byte = i / 4;
                        int bit = i % 4;
                        int col = ((sgb_cmd_data[6+byte] & (0xc0>>(bit*2)))>>((3-bit)*2));
                        pal_attrs[y0*20+x0] = col;

                        if(hv) {
                            y0++;
                            if(y0 == 18) {
                                x0++;
                                y0=0;
                            }
                        }
                        else {
                            x0++;
                            if(x0 == 20) {
                                y0++;
                                x0=0;
                            }
                        }
                    }
                    screen.sgb_set_attrs(pal_attrs);
                }
                break;
            case 0x0a: //PAL_SET, use after PAL_TRN and/or ATTR_TRN
                {
                    uint16_t pal0 = sgb_cmd_data[2] * 256 + sgb_cmd_data[1];
                    uint16_t pal1 = sgb_cmd_data[4] * 256 + sgb_cmd_data[3];
                    uint16_t pal2 = sgb_cmd_data[6] * 256 + sgb_cmd_data[5];
                    uint16_t pal3 = sgb_cmd_data[8] * 256 + sgb_cmd_data[7];
                    uint8_t attr_file = sgb_cmd_data[9] & 0x3f;
                    bool cancel = ((sgb_cmd_data[9] & 0x40) == 0x40);
                    bool use_attr = ((sgb_cmd_data[9] & 0x80) == 0x80);
                    //printf(": PAL_SET  apply previous system palette and/or attr data : pal0: %d pal1: %d pal2: %d pal3: %d use attr: %d attr file: %d cancel: %d\n", pal0, pal1, pal2, pal3, use_attr, attr_file, cancel);
                    screen.sgb_pal_transfer(pal0, pal1, pal2, pal3, attr_file, use_attr, cancel);
                }
                break;
            case 0x0b: //PAL_TRN
                //printf(": PAL_TRN  Transfer 4k of VRAM to SNES memory as palette data\n");
                //screen.sgb_trigger_dump(std::string("pal_trn_dat-") + std::to_string(cycle) + ".dat");
                screen.sgb_vram_transfer(1);
                //sends 0x1000 bytes from GB VRAM 000-fff to SNES 3000-3fff
                //each palette is 4 16-bit colors (8 bytes), so this is 512 palettes
                break;
            case 0x0e: //ICON_EN
                {
                    //bool palette  = ((sgb_cmd_data[1] & 1) == 1);
                    //bool settings = ((sgb_cmd_data[1] & 2) == 2);
                    //bool reg_file = ((sgb_cmd_data[1] & 4) == 4);
                    //printf(": ICON_EN  palette: %d settings: %d reg file transfer: %d\n", palette,settings,reg_file);
                }
                break;
            case 0x0f: //DATA_SND
                {
                    //uint16_t addr = (sgb_cmd_data[2]<<8) + sgb_cmd_data[1];
                    //uint8_t bank = sgb_cmd_data[3];
                    //uint8_t count = sgb_cmd_data[4];
                    //printf(": DATA_SND Send %d bytes to SNES WRAM %02x:%04x (not supported)\n", count, bank, addr);
                }
                break;
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
                        break;
                    case 0x03:
                        sgb_joypad_count = 4;
                        break;
                }
                //printf(": MLT_REQ  Set joypad count to %d\n", sgb_joypad_count);
                sgb_cur_joypad = 0;
                break;
            case 0x13: //CHR_TRN
                //printf(": CHR_TRN  Character data transfer from VRAM, chars %02x - %02x\n", (sgb_cmd_data[1] & 1) * 0x80, (sgb_cmd_data[1] & 1) * 0x80 + 0x7f);
                //screen.sgb_trigger_dump(std::string("chr_trn_dat-") + std::to_string(cycle) + ".dat");
                screen.sgb_vram_transfer(2+(sgb_cmd_data[1]&1));
                //bit0: tile numbers
                //bit1: officially, tile type (0=bg, 1=obj), but might not work?
                //4K transfer, 128 16-byte tiles (8x8, 16 colors) (wait? not 32-byte tiles??)
                break;
            case 0x14: //PCT_TRN
                //printf(": PCT_TRN  Screen+Color data transfer from VRAM\n");
                //screen.sgb_trigger_dump(std::string("pct_trn_dat-") + std::to_string(cycle) + ".dat");
                screen.sgb_vram_transfer(4);
                //4K transfer of map and palette data
                //000-7FF: 32x32 16-bit values for bg map
                //800-87f: palettes 4-7, 16 colors of 16bits each
                //880-fff: don't care
                //BG map: bit  0 - 9: character number
                //        bit 10 -12: palette number
                //        bit     13: BG priority
                //        bit     14: x-flip
                //        bit     15: y-flip
                break;
            case 0x15: //ATTR_TRN
                //printf(": ATTR_TRN Transfer attribute files to SNES\n");
                //screen.sgb_trigger_dump(std::string("attr_trn_dat-") + std::to_string(cycle) + ".dat");
                screen.sgb_vram_transfer(5);
                //ATF (attribute file) is a 20x18 array of 1-byte color attributes.
                //This transfers ATF0-ATF44 (4050 bytes) from GB VRAM 000-FD1 to SNES ATF banks.
                //Each ATF has the same data format as command 0x07: 90 bytes, 4 2-bit attributes per byte, arranged in 20x18
                break;
            case 0x16: //ATTR_SET
                //printf(": ATTR_SET file: %d cancel mask: %d\n", (sgb_cmd_data[1] & 0x3f), ((sgb_cmd_data[1] & 0x40) == 0x40));
                screen.sgb_set_attrib_from_file((sgb_cmd_data[1] & 0x3f), ((sgb_cmd_data[1] & 0x40) == 0x40));
                break;
            case 0x17: //MASK_EN
                //printf(": MASK_EN  ");
                switch(sgb_cmd_data[1] & 0x03) {
                    case 0:
                        //printf("Cancel masking\n");
                        break;
                    case 1:
                        //printf("Freeze screen\n");
                        break;
                    case 2:
                        //printf("Screen to black\n");
                        break;
                    case 3:
                        //printf("Screen to color 00\n");
                        break;
                }
                screen.sgb_set_mask_mode(sgb_cmd_data[1]);
                break;
            case 0x19: //PAL_PRI
                //printf(": PAL_PRI  Player/Application pal priority: %d\n", sgb_cmd_data[1]);
                break;
            default:
                //printf(": Unsupported SGB command (usually something that needs SNES emulation): %02x\n", sgb_cmd);
                break;
        }
    }
}

void memmap::speed_switch() {
    if(be_speedy) {
        //printf("Switch from fast to slow\n");
    }
    else {
        //printf("Switch from slow to fast\n");
    }
    be_speedy = !be_speedy;
    feel_the_need = false; //Don't feel the need for speed
}

bool memmap::needs_color() {
    return cart.needs_color();
}

bool memmap::set_color() {
    if(cart.color_firmware || !cart.firmware) {
        cgb = true;
        wram.resize(8 * 0x1000);
        if(cart.color_firmware) {
            screen.cgb_enable(true);
        }
        else {
            screen.cgb_enable(cart.supports_color());
        }
        return true;
    }

    //Return false if a DMG or SGB firmware was supplied
    return false;
}

uint64_t memmap::handle_hdma(uint64_t cycle) {
    if(!hdma_running) return 0;
    uint64_t cycle2 = (cycle>>1);
    uint8_t mode = screen.get_mode(cycle2);
    //uint64_t scrline = ((cycle2 - screen.get_active_cycle()) % 17556) / 114;
    uint8_t val = 0;
    if(hdma_general) {
        //printf("\t GDMA: transfer %d from %04x to %04x @ %ld (mode %d)\n", hdma_chunks, hdma_src, hdma_dest, cycle2, mode);
        for(int c = 0; c < hdma_chunks; c++) {
            for(int byte=0;byte<16;byte++) {
                val = read(hdma_src, cycle + byte + c*16);
                write(hdma_dest, val, cycle + byte + c*16);
                hdma_src++;
                hdma_dest++;
            }
            mode = screen.get_mode(cycle2);
            //scrline = ((cycle2 - screen.get_active_cycle()) % 17556) / 114;
            //printf("\t\tChunk %d, mode %d, line %d @ %ld\n", c, mode, scrline, cycle + c*16);
        }
        hdma_general = false;
        hdma_running = false;
        return 16 * hdma_chunks;
    }
    else if(hdma_hblank) {
        bool transferred = false;
        if(hdma_first || (hdma_last_mode != 0 && mode == 0)) {
            //printf("\t HDMA: transfer 16 from %04x to %04x @ %ld (%d chunks left), mode %d\n", hdma_src, hdma_dest, cycle2, hdma_chunks-1, mode);
            for(int byte=0;byte<16;byte++) {
                val = read(hdma_src + byte, cycle + byte);
                write(hdma_dest + byte, val, cycle + byte);
            }
            hdma_src += 0x10;
            hdma_dest += 0x10;
            hdma_chunks--;
            if(hdma_chunks == 0) {
                hdma_hblank = false;
                hdma_running = false;
            }
            transferred = true;
            hdma_first = false;
        }
        //printf("Setting last mode from %d to %d\n", hdma_last_mode, mode);
        hdma_last_mode = mode;
        if(transferred) {
            return 16;
        }
    }
    return 0;
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
