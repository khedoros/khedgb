#include "rom.h"
#include<fstream>
#include<iostream>

rom::rom(const std::string& rom_filename, const std::string& firmware_filename = "") : cram(0), valid(false) {
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

    h.has_sensor = false; //not supported by any of the MBCs I plan on implementing
    switch(h.cart_type) {
        case 0:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_NONE;
            break;
        case 1:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC1;
            break;
        case 2:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC1;
            break;
        case 3:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC1;
            break;
        case 5:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC2;
            break;
        case 6:
            h.has_ram = false; h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC2;
            break;
        case 8:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_NONE;
            break;
        case 9:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_NONE;
            break;
        case 0xf:
            h.has_ram = false; h.has_bat = true;  h.has_rtc = true;  h.has_rumble = false; h.mapper = MAP_MBC3;
            break;
        case 0x10:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = true;  h.has_rumble = false; h.mapper = MAP_MBC3;
            break;
        case 0x11:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC3;
            break;
        case 0x12:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC3;
            break;
        case 0x13:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC3;
            break;
        case 0x19:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC5;
            break;
        case 0x1a:
            h.has_ram = true;  h.has_bat = false; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC5;
            break;
        case 0x1b:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_MBC5;
            break;
        case 0x1c:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = true;  h.mapper = MAP_MBC5;
            break;
        case 0x1d:
            h.has_ram = false; h.has_bat = false; h.has_rtc = false; h.has_rumble = true;  h.mapper = MAP_MBC5;
            break;
        case 0x1e:
            h.has_ram = true;  h.has_bat = true;  h.has_rtc = false; h.has_rumble = true;  h.mapper = MAP_MBC5;
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

    if(h.mapper != MAP_MBC2) {
        cram.resize(h.ram_size);
    }
    else {
        cram.resize(512); //512 4-bit values supported in MBC2
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
            map = new mapper(h.rom_size, h.ram_size, h.has_bat);
            break;
        case MAP_MBC1:
            map = new mbc1_rom(h.rom_size, h.ram_size, h.has_bat);
            break;
        case MAP_MBC2:
            map = new mbc2_rom(h.rom_size, h.has_bat);
            break;
        case MAP_MBC3:
            map = new mbc3_rom(h.rom_size, h.ram_size, h.has_bat, h.has_rtc);
            break;
        case MAP_MBC5:
            map = new mbc5_rom(h.rom_size, h.ram_size, h.has_bat, h.has_rumble);
            break;
        default: return;
    }

    valid = true;

}

void rom::read(int addr, void * val, int size, int cycle) {
    if(addr < 0x4000) {
        memcpy(val, &(rom_data[addr]), size);
    }
    else if(addr < 0x8000) {
        addr = map->map_rom(addr, cycle);
        if(addr < 0xffffff) {
            memcpy(val, &(rom_data[addr]), size);
        }
        else {
            memset(val, 0xff, size);
        }
    }
    else if(addr >= 0xa000 && addr < 0xc000) {
        addr = map->map_ram(addr, cycle);
        if(addr < 0xffffff) {
            memcpy(val, &(cram[addr]), size);
        }
        else {
            memset(val, 0xff, size);
        }
    }
}

//TODO: Replace this with mapper implementation
void rom::write(int addr, void * val, int size, int cycle) {
    if(addr == 0xff50) memcpy(reinterpret_cast<char *>(&rom_data[0]), reinterpret_cast<char *>(&rom_backup[0]), 256);
    else if(addr >= 0xa000 && addr < 0xc000) {
        addr = map->map_ram(addr, cycle);
        if(addr < 0xffffff) {
            memcpy(&(cram[addr]), val, size);
        }
    }
    else {
        map->write(addr, val, size, cycle);
    }
}

std::string rom::mapper_names[6] = {"None", "MBC1", "MBC2", "MBC3", "MBC5", "Unsupported"};
uint32_t rom::rom_sizes[9] = {32*1024, 64*1024, 128*1024, 256*1024, 512*1024, 1024*1024, 2048*1024, 4096*1024, 8192*1024};
uint32_t rom::ram_sizes[6] = {0, 2048, 8192, 32768, 131072, 65536};



//NULL mapper
mapper::mapper(int rom_size, int ram_size, bool bat) : romsize(rom_size), ramsize(ram_size), has_bat(bat) {}
uint32_t mapper::map_rom(uint32_t addr, int cycle) {
    return addr;
}
uint32_t mapper::map_ram(uint32_t addr, int cycle) {
    if(ramsize > addr - 0xa000) {
        return addr-0xa000;
    }
    else {
        return 0xffffff;
    }
    return 0;
}
void mapper::write(uint32_t addr, void * val, int size, int cycle) { }



//MBC1 mapper
mbc1_rom::mbc1_rom(int rom_size, int ram_size, bool has_bat) : mapper(rom_size, ram_size, has_bat), ram_enabled(false), mode(false) {
    bank.rom_bank = 1; //also sets RAM bank to 0
}
uint32_t mbc1_rom::map_rom(uint32_t addr, int cycle) {
    addr -= 0x4000;
    if(!mode) { //ROM banking
        addr+=(uint32_t(bank.rom_bank) * 0x4000);
        if(bank.rom_bank % 0x20 == 0) addr+=0x4000;
    }
    else {
        addr+=(uint32_t(bank.lower) * 0x4000);
        if(bank.lower == 0) addr+=0x4000;
    }
    return addr;
}
uint32_t mbc1_rom::map_ram(uint32_t addr, int cycle) {
    addr-=0xa000;
    if(addr >= ramsize || !ram_enabled) {
        return 0xffffff;
    }
    if(mode) { //RAM banking
        addr+=(uint32_t(bank.ram_bank) * 0x2000);
    }
    //else addr=addr (only use first bank)
    return addr;
}
void mbc1_rom::write(uint32_t addr, void * val, int size, int cycle) {
    if(addr<0x2000 && (*((char *)(val)) == 0x0a)) ram_enabled = true;
    else if(addr<0x2000) ram_enabled = false;
    else if(addr >= 0x2000 && addr < 0x4000) bank.lower = *(((char *)(val)));
    else if(addr >= 0x4000 && addr < 0x6000) bank.upper = *(((char *)(val)));
    else mode = *(((char *)(val)));
}



//MBC2 mapper
mbc2_rom::mbc2_rom(uint32_t rom_size, bool has_bat) : mapper(rom_size, 512, has_bat) {}
uint32_t mbc2_rom::map_rom(uint32_t addr, int cycle) {
    return 0;
}
uint32_t mbc2_rom::map_ram(uint32_t addr, int cycle) {
    return 0;
}
void mbc2_rom::write(uint32_t addr, void * val, int size, int cycle) {}



//MBC3 mapper
mbc3_rom::mbc3_rom(int rom_size, int ram_size, bool has_bat, bool has_rtc) : mapper(rom_size, ram_size, has_bat) {}
uint32_t mbc3_rom::map_rom(uint32_t addr, int cycle) {
    return 0;
}
uint32_t mbc3_rom::map_ram(uint32_t addr, int cycle) {
    return 0;
}
void mbc3_rom::write(uint32_t addr, void * val, int size, int cycle) {}



//MBC5 mapper
mbc5_rom::mbc5_rom(int rom_size, int ram_size, bool has_bat, bool has_rumble) : mapper(rom_size, ram_size, has_bat) {}
uint32_t mbc5_rom::map_rom(uint32_t addr, int cycle) {
    return 0;
}
uint32_t mbc5_rom::map_ram(uint32_t addr, int cycle) {
    return 0;
}
void mbc5_rom::write(uint32_t addr, void * val, int size, int cycle) {}

