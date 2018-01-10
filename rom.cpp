#include "rom.h"
#include<fstream>
#include<iostream>

rom::rom(const std::string& rom_filename, const std::string& firmware_filename = "") : cram(0x2000) {
    firmware = false;
    //Take input of the actual ROM data
    std::ifstream in(rom_filename.c_str());
    if(in.is_open()) {
        size_t size = 0;
        in.seekg(0, std::ios::end);
        size = in.tellg();
        in.seekg(0, std::ios::beg);
        std::cout<<"Opened "<<rom_filename<<", found a file of "<<size<<" bytes."<<std::endl;
        rom_data.resize(size);
        h.filesize = size;
        in.read(reinterpret_cast<char *>(&(rom_data[0])), size);
        in.seekg(0, std::ios::beg);
        rom_backup.resize(256);
        in.read(reinterpret_cast<char *>(&(rom_backup[0])), 256);
        in.close();
    }
    else {
        std::cerr<<"Couldn't open "<<rom_filename<<"."<<std::endl;
        rom_data.resize(0x8000, 0x10); //Fill it with "stop" instructions
        return;
    }

    //Load the firmware file, if one was provided
    if(firmware_filename != "") {
        in.open(firmware_filename.c_str());
        if(in.is_open()) {
            size_t size = 0;
            in.seekg(0, std::ios::end);
            size = in.tellg();
            in.seekg(0, std::ios::beg);
            std::cout<<"Opened firmware "<<firmware_filename<<", found a file of "<<size<<" bytes."<<std::endl;
            firmware = true;
            if(size != 256) {
                std::cerr<<"But...I was really expecting a firmware of 256 bytes. I'm going to proceed without firmware."<<std::endl;
                firmware = false;
                in.close();
            }
            else {
                firmware = true;
                firmware_data.resize(size);
                in.read(reinterpret_cast<char *>(&(rom_data[0])), 256);
                in.close();
            }
        }
        else {
            std::cerr<<"Couldn't open "<<firmware_filename<<"."<<std::endl;
            firmware = false;
        }
    }

    //Read Game title
    memcpy(&(rom_data[0x134]), &(h.title[0]), 16);
    h.title[16] = 0;
    for(int i=0;i<16;i++) {
        if(h.title[i] < 32 || h.title[i] > 126) {
            h.title[i] = 0;
        }
    }

    h.cgb_flag = rom_data[0x143];
    h.support_sgb = (rom_data[0x146] == 3);
    h.cart_type = rom_data[0x147];

    switch(h.cart_type) {
        case 0:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_NONE;
            break;
        case 1:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC1;
            break;
        case 2:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC1;
            break;
        case 3:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC1;
            break;
        case 5:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC2;
            break;
        case 6:
            h.has_ram = false; h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC2;
            break;
        case 8:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_NONE;
            break;
        case 9:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_NONE;
            break;
        case 0xb:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MMM01;
            break;
        case 0xc:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MMM01;
            break;
        case 0xd:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MMM01;
            break;
        case 0xf:
            h.has_ram = false; h.has_bat = true;  h.has_rtc = true;  h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC3;
            break;
        case 0x10:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = true;  h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC3;
            break;
        case 0x11:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC3;
            break;
        case 0x12:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC3;
            break;
        case 0x13:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC3;
            break;
        case 0x19:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC5;
            break;
        case 0x1a:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC5;
            break;
        case 0x1b:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC5;
            break;
        case 0x1c:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = true;  h.has_sensor = false; h.mapper = MAP_MBC5;
            break;
        case 0x1d:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = true;  h.has_sensor = false; h.mapper = MAP_MBC5;
            break;
        case 0x1e:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = true;  h.has_sensor = false; h.mapper = MAP_MBC5;
            break;
        case 0x20:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.has_sensor = false; h.mapper = MAP_MBC6;
            break;
        case 0x22:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = true;  h.has_sensor = true;  h.mapper = MAP_MBC7;
            break;
        default:
            std::cerr<<"Cart type "<<std::hex<<int(h.cart_type)<<" will probably never be supported :-("<<std::endl;
    }
    
    /*
    uint8_t rom_size;     //0148
    uint8_t ram_size;     //0149
    uint8_t old_lic;      //014b old license code. Needs to be 0x33 for SGB support.
    uint8_t rom_ver;      //014c

*/

    h.rom_size = 0; //We'll deal with this later :-)



}

void rom::read(int addr, void * val, int size, int cycle) {
    if(addr < 0x8000) {
        memcpy(val, &(rom_data[addr]), size);
    }
    else if(addr >= 0xa000 && addr < 0xc000) {
        memcpy(val, &(cram[addr-0xa000]), size);
    }
}

void rom::write(int addr, void * val, int size, int cycle) {
    if(addr == 0xff50) memcpy(reinterpret_cast<char *>(&rom_data[0]), reinterpret_cast<char *>(&rom_backup[0]), 256);
    else if(addr >= 0xa000 && addr < 0xc000) {
        memcpy(&(cram[addr-0xa000]), val, size);
    }
    else {
        std::cout<<"Mapper stuff for the future ;-)"<<std::endl;
    }
}
