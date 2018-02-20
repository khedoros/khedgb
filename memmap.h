#pragma once
#include<cstdint>
#include "rom.h"
#include "lcd.h"
#include "apu.h"
#include<string>
#include<SDL2/SDL_scancode.h>

enum INT_TYPE {
    NONE = 0,
    VBLANK = 1,
    LCDSTAT = 2,
    TIMER = 4,
    SERIAL = 8,
    JOYPAD = 16
};

class memmap {
public:
    memmap(const std::string& filename, const std::string& fw_file);
    //read: rw==false, write: rw==true
//    void map(int addr, void * val, int size, bool rw);
    void read(int addr, void * val, int size, uint64_t cycle);
    void write(int addr, void * val, int size, uint64_t cycle);
    void keydown(SDL_Scancode k);
    void keyup(SDL_Scancode k);
    INT_TYPE get_interrupt();
    void update_interrupts(uint64_t cycle);
    //void map(int addr, void * const val, int size, bool rw);
    bool has_firmware();
    lcd * get_lcd();
    apu * get_apu();
    bool set_sgb(bool active);
    struct int_flags {
        union {
            struct {
                unsigned vblank:1;
                unsigned lcdstat:1;
                unsigned timer:1;
                unsigned serial:1;
                unsigned joypad:1;
                unsigned unused:3;
            };
            uint8_t reg;
        };
    };
    bool valid;
private:
    lcd screen;
    apu sound;
    rom cart;
    Vect<uint8_t> wram;
    Vect<uint8_t> hram;

    //Interrupt registers
    int_flags int_enabled; //0xffff interrupt enable/disable flags
    int_flags int_requested; //0xff0f interrupt requested flags
    uint64_t last_int_cycle;

    //Joypad registers
    struct buttons {
        union {
            struct {
                unsigned right:1;
                unsigned left:1;
                unsigned up:1;
                unsigned down:1;
                unsigned unused_dirs:4;
            };
            struct {
                unsigned a:1;
                unsigned b:1;
                unsigned select:1;
                unsigned start:1;
                unsigned unused_buttons:4;
            };
            unsigned keys:8;
        };
    };
    uint8_t joypad; //0xff00, just stores the flags for which lines are active
    buttons directions[4]; // pads 2-4 are SGB-only (and just dummies, for now)
    buttons btns[4];

    //Super GameBoy registers
    bool sgb_active;
    void sgb_exec(Vect<uint8_t>& s_b, uint64_t cycle);
    Vect<uint8_t> sgb_buffer;
    Vect<uint8_t> sgb_cmd_data;
    uint8_t sgb_bit_ptr;
    bool sgb_buffered;
    uint8_t sgb_cur_joypad;
    uint8_t sgb_joypad_count;
    uint8_t sgb_cmd_count;
    uint8_t sgb_cmd;
    uint8_t sgb_cmd_index;

    //Serial link registers
    uint8_t link_data; //0xff01 (serial data register)
    bool serial_transfer; //0xff02 bit 7 (serial command register, transfer state)
    bool internal_clock;  //0xff02 bit 0 (serial command register, clock selection)
    uint64_t transfer_start; //cycle when transfer was initiated
    uint8_t bits_transferred; //how many bits have been transmitted

    //Timer registers
    //uint8_t div; //0xff04 top 8 bits of clock-divider register, which increments at 16384Hz. Calculated based on current cycle, and last time it was reset.
    uint64_t div_reset; //0xff04 counter cycle reset
    uint8_t timer; //0xff05 timer value
    uint8_t timer_modulus; //0xff06 Modulus value for timer.
    uint8_t timer_control; //0xff07 Controls speed and start/stop of timer. 
    bool timer_running; //based on 0xff07 writes (bit 2)
    unsigned int clock_divisor; //based on 0xff07 writes
    uint64_t timer_reset; //Clock when the timer was started, speed was changed, or timer was written while running
    static const unsigned int timer_clock_select[4]; //0xff07 bit 0+1, table of number of CPU clocks to tick the timer
    uint64_t timer_deadline; //Time the clock will expire
    uint64_t div_clock; //0xff04: Used to calculate value of div. Stores last clock that div was reset at. DIV updates at 16384Hz (every 64CPU cycles)
};
/*
 * 0x0000-0x3FFF: Permanently-mapped ROM bank.
 * 0x4000-0x7FFF: Area for switchable ROM banks.
 * 0x8000-0x9FFF: Video RAM.
 * 0xA000-0xBFFF: Area for switchable external RAM banks.
 * 0xC000-0xCFFF: Game Boy’s working RAM bank 0 .
 * 0xD000-0xDFFF: Game Boy’s working RAM bank 1.
 * 0xFE00-0xFEFF: Sprite Attribute Table.
 * 0xFF00-0xFF7F: Devices’ Mappings. Used to access I/O devices.
 * 0xFF80-0xFFFE: High RAM Area.
 * 0xFFFF: Interrupt Enable Register.
 * */

