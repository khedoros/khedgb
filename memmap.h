#pragma once
#include<cstdint>
#include "rom.h"
#include "lcd.h"
#include<string>
#include<vector>

enum INT_TYPE {
    VBLANK,
    LCDSTAT,
    TIMER,
    SERIAL,
    JOYPAD,
    NONE
};

class memmap {
public:
    memmap(const std::string& filename, const std::string& fw_file);
    //read: rw==false, write: rw==true
//    void map(int addr, void * val, int size, bool rw);
    void read(int addr, void * val, int size, int cycle);
    void write(int addr, void * val, int size, int cycle);
    INT_TYPE get_interrupt();
    //void map(int addr, void * const val, int size, bool rw);
    void  render(int f);
    bool has_firmware();
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
private:
    lcd screen;
    rom cart;
    std::vector<uint8_t> wram;
    std::vector<uint8_t> hram;
    std::vector<uint8_t> oam;
    static const uint8_t dmg_firmware[256];

    int_flags int_enabled; //0xffff interrupt enable/disable flags
    int_flags int_requested; //0xff0f interrupt requested flags
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
