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
        rom_data.resize(0x8000, 0x10);
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
