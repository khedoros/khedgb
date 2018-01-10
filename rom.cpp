#include "rom.h"
#include<fstream>
#include<iostream>

rom::rom(const std::string& rom_filename, const std::string& firmware_filename = "") : cram(0x2000), valid(false) {
    firmware = false;
    //Take input of the actual ROM data
    std::ifstream in(rom_filename.c_str());
    if(in.is_open()) {
        size_t size = 0;
        in.seekg(0, std::ios::end);
        size = in.tellg();
        in.seekg(0, std::ios::beg);
        std::cout<<"Opened "<<rom_filename<<", found a file of "<<size<<" bytes."<<std::endl;
        if(size > 8 * 1024 * 1024) {
            std::cerr<<"That's larger than I'd expect any real Game Boy ROM to be. Exiting."<<std::endl;
            rom_data.resize(0x8000, 0x10);
            return;
        }
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
    memcpy(&(h.title[0]), &(rom_data[0x134]), 16);
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
        default:
            std::cerr<<"Cart type "<<std::hex<<int(h.cart_type)<<" will probably never be supported :-("<<std::endl;
            h.mapper = MAP_UNSUPPORTED;
            return;
    }
    
    uint8_t rom_size = rom_data[0x148];
    uint8_t ram_size = rom_data[0x149];
    if(rom_size >= 0 && rom_size < 9) {
        h.rom_size = rom_sizes[rom_size];
    }
    if(ram_size >=0 && ram_size < 6) {
        h.ram_size = ram_sizes[ram_size];
        if((!h.has_ram && ram_size != 0) || (h.has_ram && ram_size == 0)) {
            std::cerr<<"Specified as having no RAM, but non-zero RAM size. Weird."<<std::endl;
        }
    }

    if(h.rom_size != h.filesize) {
        std::cerr<<"Filesize: "<<std::dec<<h.filesize<<" Claimed rom size: "<<h.rom_size<<" (oopsie!)"<<std::endl;
        return;
    }

    h.rom_ver = rom_data[0x14c];

    std::cout<<"Filename: "<<rom_filename<<" Internal name: "<<h.title<<std::endl;
    std::cout<<"ROM size: "<<h.rom_size<<" bytes RAM size: "<<((h.mapper==MAP_MBC2)?256:h.ram_size)<<" bytes Mapper name: "<<mapper_names[h.mapper]<<std::endl;
    std::cout<<"Features: "<<((h.has_ram)?"[RAM] ":"")<<((h.has_bat)?"[BATTERY] ":"")<<((h.has_rtc)?"[TIMER] ":"")<<((h.has_rumble)?"[RUMBLE] ":"")<<((h.has_sensor)?"[TILT/LIGHT] ":"")<<std::endl;

    switch(h.mapper) {
        case MAP_NONE:
            map = new mapper();
            break;
        case MAP_MBC1:
            map = new mbc1_rom();
            break;
        case MAP_MBC2:
            map = new mbc2_rom();
            break;
        case MAP_MBC3:
            map = new mbc3_rom();
            break;
        case MAP_MBC5:
            map = new mbc5_rom();
            break;
        default: return;
    }

    valid = true;

}

//TODO: Replace this with mapper implementation
void rom::read(int addr, void * val, int size, int cycle) {
    if(addr < 0x8000) {
        memcpy(val, &(rom_data[addr]), size);
    }
    else if(addr >= 0xa000 && addr < 0xc000) {
        memcpy(val, &(cram[addr-0xa000]), size);
    }
}

//TODO: Replace this with mapper implementation
void rom::write(int addr, void * val, int size, int cycle) {
    if(addr == 0xff50) memcpy(reinterpret_cast<char *>(&rom_data[0]), reinterpret_cast<char *>(&rom_backup[0]), 256);
    else if(addr >= 0xa000 && addr < 0xc000) {
        memcpy(&(cram[addr-0xa000]), val, size);
    }
    else {
        std::cout<<"Mapper stuff for the future ;-)"<<std::endl;
    }
}

std::string rom::mapper_names[6] = {"None", "MBC1", "MBC2", "MBC3", "MBC5", "Unsupported"};
uint32_t rom::rom_sizes[9] = {32*1024, 64*1024, 128*1024, 256*1024, 512*1024, 1024*1024, 2048*1024, 4096*1024, 8192*1024};
uint32_t rom::ram_sizes[6] = {0, 2048, 8192, 32768, 131072, 65536};

mapper::mapper() {}
uint32_t mapper::map(int addr, int cycle) {
    return 0;
}
void mapper::write(int addr, void * val, int size, int cycle) {}

mbc1_rom::mbc1_rom() {}
uint32_t mbc1_rom::map(int addr, int cycle) {
    return 0;
}
void mbc1_rom::write(int addr, void * val, int size, int cycle) {}

mbc2_rom::mbc2_rom() {}
uint32_t mbc2_rom::map(int addr, int cycle) {
    return 0;
}
void mbc2_rom::write(int addr, void * val, int size, int cycle) {}

mbc3_rom::mbc3_rom() {}
uint32_t mbc3_rom::map(int addr, int cycle) {
    return 0;
}
void mbc3_rom::write(int addr, void * val, int size, int cycle) {}

mbc5_rom::mbc5_rom() {}
uint32_t mbc5_rom::map(int addr, int cycle) {
    return 0;
}
void mbc5_rom::write(int addr, void * val, int size, int cycle) {}

