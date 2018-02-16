#pragma once

//#include<vector>
#include<string>
#include<cstring>
#include<cstdint>
#include "util.h"

enum map_type {
    MAP_NONE,
    MAP_MBC1,
    MAP_MBC2,
    MAP_MBC3,
    MAP_MBC5,
    MAP_CAMERA, 
    MAP_UNSUPPORTED
};

class mapper {
    public:
        mapper(int rom_size, int ram_size, bool has_bat, bool has_rtc=false);
        virtual ~mapper();
        virtual uint32_t map_rom(uint32_t addr, int cycle);
        virtual uint32_t map_ram(uint32_t addr, int cycle);
        virtual void write(uint32_t addr, void * val, int size, int cycle);
    protected:
        bool has_bat;
        bool has_rtc;
        uint32_t romsize;
        uint32_t ramsize;
};

class rom {
    public:
        rom(const std::string& rom_filename, const std::string& firmware_filename);
        ~rom();
        virtual void read(uint32_t addr, void * val, int size, int cycle);
        virtual void write(uint32_t addr, void * val, int size, int cycle);
        bool supports_sgb();

        struct header {
            uint8_t title[17]; //16 chars and null, just in case 0134-0143
            uint8_t cgb_flag;     //Bit7: Color, Bit6: *only* color (if present with bit7) 0143
            bool support_sgb;     //0146
            uint8_t cart_type;    //0147
            map_type mapper;       //part of 0147
            bool has_ram;         //part of 0147
            bool has_bat;         //part of 0147
            bool has_rtc;         //part of 0147
            bool has_rumble;      //part of 0147
            bool has_sensor;      //part of 0147
            uint32_t rom_size;     //0148
            uint32_t ram_size;     //0149
            uint8_t old_lic;      //014b old license code. Needs to be 0x33 for SGB support.
            uint8_t rom_ver;      //014c
            size_t filesize;
        };
        bool firmware;
        bool valid;

        static std::string mapper_names[7];
        static uint32_t rom_sizes[9];
        static uint32_t ram_sizes[6];
        
    protected:
        void disable_firmware();
        Vect<uint8_t> firmware_data;
        Vect<uint8_t> rom_data;
        Vect<uint8_t> rom_backup;
        Vect<uint8_t> cram;
        header h;
        mapper * map;
        std::string filename;
};

class mbc1_rom: public mapper {
    public:
        mbc1_rom(int rom_size, int ram_size, bool has_bat);
        virtual uint32_t map_rom(uint32_t addr, int cycle);
        virtual uint32_t map_ram(uint32_t addr, int cycle);
        virtual void write(uint32_t addr, void * val, int size, int cycle);
    private:
        bool ram_enabled;
        bool mode; //0=rom banking, 1=ram banking
        union banknum {
            struct {
                unsigned lower:5;
                unsigned upper:2;
                unsigned unused_rom:1;
            };
            struct {
                unsigned unused_ram:5;
                unsigned ram_bank:2;
                unsigned unused_ram2:1;
            };
            struct {
                unsigned rom_bank:7;
                unsigned unused_rom1:1;
            };
        };
        banknum bank; //
};

class mbc2_rom: public mapper {
    public:
        mbc2_rom(uint32_t rom_size, bool has_bat);
        virtual uint32_t map_rom(uint32_t addr, int cycle);
        virtual uint32_t map_ram(uint32_t addr, int cycle);
        virtual void write(uint32_t addr, void * val, int size, int cycle);
    private:
        bool ram_enabled;
        unsigned banknum:4;
};

class mbc3_rom: public mapper {
    public:
        mbc3_rom(int rom_size, int ram_size, bool has_bat, bool has_rtc, std::vector<uint8_t>& rtc_data);
        virtual uint32_t map_rom(uint32_t addr, int cycle);
        virtual uint32_t map_ram(uint32_t addr, int cycle);
        virtual void write(uint32_t addr, void * val, int size, int cycle);
    private:
        uint8_t rombank;
        uint8_t rambank;
        bool ram_enabled;
        bool rtc_latch;
        uint8_t rtc[5];
        uint8_t latched_rtc[5];
        uint64_t load_timestamp; //64-bit Unix timestamp, representing when the file is loaded (save data stores when the file was saved, so that we can simulate the appropriate passage of time)
};

class camera_rom: public mapper {
    public:
        camera_rom(int rom_size, int ram_size, bool has_bat);
        virtual uint32_t map_rom(uint32_t addr, int cycle);
        virtual uint32_t map_ram(uint32_t addr, int cycle);
        virtual void write(uint32_t addr, void * val, int size, int cycle);
    private:
        uint8_t rombank;
        uint8_t rambank;
        bool ram_enabled;
};

class mbc5_rom: public mapper {
    public:
        mbc5_rom(int rom_size, int ram_size, bool has_bat, bool has_rumble);
        virtual uint32_t map_rom(uint32_t addr, int cycle);
        virtual uint32_t map_ram(uint32_t addr, int cycle);
        virtual void write(uint32_t addr, void * val, int size, int cycle);
    private:
        union rom_bank {
            struct {
                unsigned lower:8;
                unsigned upper:1;
                unsigned unused_rom:7;
            };
            uint16_t bank;
        } rombank;

        uint8_t rambank;
        bool ram_enabled;
};

/*
 *
sgb_flag;    //00=no SGB support, 03=SGB support
0144-0145 - New Licensee Code (use if 14b is 0x33)
Le sigh. Maybe if I want to print the licensee in the future. Seems gratuitous. YAGNI.

0146 - SGB Flag
00h = No SGB functions (Normal Gameboy or CGB only game)
03h = Game supports SGB functions

0147 - Cartridge Type
00h  ROM ONLY                 19h  MBC5
01h  MBC1                     1Ah  MBC5+RAM
02h  MBC1+RAM                 1Bh  MBC5+RAM+BATTERY
03h  MBC1+RAM+BATTERY         1Ch  MBC5+RUMBLE
05h  MBC2                     1Dh  MBC5+RUMBLE+RAM
06h  MBC2+BATTERY             1Eh  MBC5+RUMBLE+RAM+BATTERY
08h  ROM+RAM                  20h  MBC6
09h  ROM+RAM+BATTERY          22h  MBC7+SENSOR+RUMBLE+RAM+BATTERY
0Bh  MMM01
0Ch  MMM01+RAM
0Dh  MMM01+RAM+BATTERY
0Fh  MBC3+TIMER+BATTERY
10h  MBC3+TIMER+RAM+BATTERY   FCh  POCKET CAMERA
11h  MBC3                     FDh  BANDAI TAMA5
12h  MBC3+RAM                 FEh  HuC3
13h  MBC3+RAM+BATTERY         FFh  HuC1+RAM+BATTERY

0148 - ROM Size
Specifies the ROM Size of the cartridge. Typically calculated as "32KB shl N".

00h -  32KByte (no ROM banking)
01h -  64KByte (4 banks)
02h - 128KByte (8 banks)
03h - 256KByte (16 banks)
04h - 512KByte (32 banks)
05h -   1MByte (64 banks)  - only 63 banks used by MBC1
06h -   2MByte (128 banks) - only 125 banks used by MBC1
07h -   4MByte (256 banks)
08h -   8MByte (512 banks)
52h - 1.1MByte (72 banks)
53h - 1.2MByte (80 banks)
54h - 1.5MByte (96 banks)

0149 - RAM Size
Specifies the size of the external RAM in the cartridge (if any).

00h - None
01h - 2 KBytes
02h - 8 Kbytes
03h - 32 KBytes (4 banks of 8KBytes each)
04h - 128 KBytes (16 banks of 8KBytes each)
05h - 64 KBytes (8 banks of 8KBytes each)

When using a MBC2 chip 00h must be specified in this entry, even though the MBC2 includes a built-in RAM of 512 x 4 bits.

014A - Destination Code
00h - Japanese
01h - Non-Japanese

014B - Old Licensee Code
SGB requires 0x33.

14c - Mask ROM version
*/
