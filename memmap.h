#pragma once
#include<cstdint>
#include "mapper.h"
#include "rom.h"
#include "lcd.h"
#include<string>
#include<vector>

class memmap {
public:
    memmap(const std::string& filename);
    //read: rw==false, write: rw==true
//    void map(int addr, void * val, int size, bool rw);
    void read(int addr, void * val, int size, int cycle);
    void write(int addr, void * val, int size, int cycle);
    //void map(int addr, void * const val, int size, bool rw);
    void  render(int f);
private:
    lcd screen;
    std::vector<uint8_t> rom;   
    std::vector<uint8_t> wram;
    std::vector<uint8_t> hram;
    std::vector<uint8_t> oam;
    static const uint8_t dmg_firmware[256];
    bool use_dmg;
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
