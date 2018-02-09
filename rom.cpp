#include "rom.h"
#include<fstream>
#include<iostream>
#include<cassert>

rom::rom(const std::string& rom_filename, const std::string& firmware_filename = "") : valid(false), filename(rom_filename) {
    cram.resize(0);
    firmware = false;
    //Take input of the actual ROM data

    if(rom_filename.substr(rom_filename.size() - 3, 3) == "zip") {
        int retval = util::unzip(rom_filename, rom_data, 32*1024, 8*1024*1024); //All ROMs are expected to be between size 32KB and 8MB
        if(retval) {
            printf("Got error code %d while trying to extract the zip.\n", retval);
            rom_data.resize(0x8000,0xd3); //invalid opcode, to crash the interpreter
        }
        else {
            std::cout<<"Opened "<<rom_filename<<", found a file of "<<rom_data.size()<<" bytes."<<std::endl;
        }
        h.filesize = rom_data.size();
        rom_backup.resize(256);
        memcpy(&rom_backup[0],&rom_data[0], 256);
    }
    else {
        int retval = util::read(rom_filename, rom_data, 32*1024, 8*1024*1024);
        if(retval) {
            printf("Got error code %d while trying to read the rom.\n", retval);
            rom_data.resize(0x8000, 0xd3); //invalid opcode
        }
        else {
            std::cout<<"Opened "<<rom_filename<<", found a file of "<<rom_data.size()<<" bytes."<<std::endl;
        }
        h.filesize = rom_data.size();
        rom_backup.resize(256);
        memcpy(&rom_backup[0], &rom_data[0], 256);
    }

    //Load the firmware file, if one was provided
    if(firmware_filename != "") {
        int retval = util::read(firmware_filename, firmware_data, 256, 256);
        if(retval) {
            firmware = false;
            std::cout<<"Didn't find a valid firmware file at "<<firmware_filename<<". Continuing without one."<<std::endl;
        }
        else {
            firmware = true;
            memcpy(&rom_data[0], &firmware_data[0], 256);
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

    std::vector<uint8_t> rtc_data;

    if(h.has_bat && cram.size() > 0) {
        uint32_t min_size = cram.size();
        uint32_t max_size = cram.size();
        if(h.mapper == MAP_MBC3) { //Take into account the RTC values
            min_size+=44;
            max_size+=48;
        }

        int ret = util::read(rom_filename.substr(0, rom_filename.find_last_of(".")+1) + "sav", cram, min_size, max_size);
        if(!ret && h.mapper == MAP_MBC3) {
            size_t rtc_size = cram.size() % 8192;
            rtc_data.resize(rtc_size, 0);
            std::copy(cram.end() - rtc_size, cram.end(), rtc_data.begin());
            cram.resize(cram.size() - rtc_size);
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
            map = new mapper(h.rom_size, h.ram_size, h.has_bat);
            break;
        case MAP_MBC1:
            map = new mbc1_rom(h.rom_size, h.ram_size, h.has_bat);
            break;
        case MAP_MBC2:
            map = new mbc2_rom(h.rom_size, h.has_bat);
            break;
        case MAP_MBC3:
            if(rtc_data.size() != 44 && rtc_data.size() != 48) {
                rtc_data.resize(48,0);
            }
            map = new mbc3_rom(h.rom_size, h.ram_size, h.has_bat, h.has_rtc, rtc_data);
            break;
        case MAP_MBC5:
            map = new mbc5_rom(h.rom_size, h.ram_size, h.has_bat, h.has_rumble);
            break;
        default: return;
    }

    valid = true;

}

rom::~rom() {
    if(h.has_bat && cram.size() > 0) {
        std::ofstream outfile(filename.substr(0, filename.find_last_of(".")+1)+"sav");
        if(outfile.is_open()) {
            outfile.write(reinterpret_cast<char *>(&cram[0]), cram.size());
            outfile.close();
        }
    }
    delete map;
}

void rom::read(uint32_t addr, void * val, int size, int cycle) {
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
        if(addr < 0xffffff) { //Messy solution for specifying that an invalid address was requested
            memcpy(val, &(cram[addr]), size);
        }
        else if((addr & 0xffffff00) == 0xffffff00) { //Messy solution for returning a value stored in the mapper itself (like RTC data in MBC3)
            uint8_t mapped_val = addr&0xff;
            memcpy(val, &mapped_val, 1);
        }
        else {
            memset(val, 0xff, size);
        }
    }
}

//TODO: Replace this with mapper implementation
void rom::write(uint32_t addr, void * val, int size, int cycle) {
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
mapper::mapper(int rom_size, int ram_size, bool bat, bool rtc/*=false*/) : has_bat(bat), has_rtc(rtc), romsize(rom_size), ramsize(ram_size) {}
uint32_t mapper::map_rom(uint32_t addr, int cycle) {
    return addr;
}
mapper::~mapper() {}
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
    if(addr<0x4000) {
        return addr;
    }

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
    if(addr<0x2000 && (*((uint8_t *)val) == 0x0a)) ram_enabled = true;
    else if(addr<0x2000) ram_enabled = false;
    else if(addr >= 0x2000 && addr < 0x4000) bank.lower = *(((uint8_t *)val));
    else if(addr >= 0x4000 && addr < 0x6000) bank.upper = *(((uint8_t *)val));
    else mode = *(((uint8_t *)val));

    if(addr>=0x2000 && addr < 0x6000) {
        printf("ROM MBC1: Set ROM bank to %d, rom size: %d\n", bank.rom_bank, romsize);
        bank.rom_bank = bank.rom_bank % (romsize / 16384);
        //printf("ROM MBC1: Set ROM bank to %d after masking\n", bank.rom_bank);
    }
}



//MBC2 mapper
mbc2_rom::mbc2_rom(uint32_t rom_size, bool has_bat) : mapper(rom_size, 512, has_bat), ram_enabled(false), banknum(1) {}
uint32_t mbc2_rom::map_rom(uint32_t addr, int cycle) {
    return  addr + (uint32_t(banknum) * 0x4000) - 0x4000;
}
uint32_t mbc2_rom::map_ram(uint32_t addr, int cycle) {
    addr -= 0xa000;
    if(addr >= ramsize || !ram_enabled) {
        return 0xffffff;
    }
    return addr;
}
void mbc2_rom::write(uint32_t addr, void * val, int size, int cycle) {
    if(addr >= 0x2000 && (addr & 0x100) == 0x100 && addr < 0x4000) banknum = *(((uint8_t *)val));
    else if(addr < 0x2000 && addr < 0x4000 && (addr & 0x100) == 0) ram_enabled = *(((uint8_t *)val));
}



//MBC3 mapper
mbc3_rom::mbc3_rom(int rom_size, int ram_size, bool has_bat, bool has_rtc, std::vector<uint8_t>& rtc_data) : mapper(rom_size, ram_size+5, has_bat), rombank(1), rambank(0), ram_enabled(false), rtc_latch(false), rtc{0,0,0,0,0}, latched_rtc{0,0,0,0,0}, load_timestamp(0) {
    //TODO: Add RTC load code
}
uint32_t mbc3_rom::map_rom(uint32_t addr, int cycle) {
    if(addr < 0x4000) {
        return addr;
    }

    addr -= 0x4000;
    return addr+(uint32_t(rombank) * 0x4000);
}
uint32_t mbc3_rom::map_ram(uint32_t addr, int cycle) {
    if(rambank < 0x04) {
        addr -= 0xa000;
        if(addr >= ramsize || !ram_enabled) {
            return 0xffffff;
        }
        return addr+(uint32_t(rambank) * 0x2000);
    }
    else if(rambank >= 0x08 && rambank <= 0x0c) {
        //TODO: Make this actually return current time if unlatched, and latched data if latched
        return (uint32_t(rtc[rambank-0x08]) | 0xffffff00);
    }
    return 0;
}
void mbc3_rom::write(uint32_t addr, void * val, int size, int cycle) {
    if(addr < 0x2000) {
        if(*((uint8_t *)val) == 0x0a) {
            ram_enabled = true;
        }
        else {
            ram_enabled = false;
        }
    }
    else if(addr < 0x4000) {
        rombank = *(((uint8_t *)val));
    }
    else if(addr < 0x6000) {
        rambank = *(((uint8_t *)val));
    }
    else if(addr < 0x8000) {
        if(*(((uint8_t *)val)) == 1 && !rtc_latch) {
            //TODO: actually latch new data here
            rtc_latch = true;
        }
        else if(*(((uint8_t *)val)) == 0) {
            rtc_latch = false;
        }
    }
}



//MBC5 mapper
mbc5_rom::mbc5_rom(int rom_size, int ram_size, bool has_bat, bool has_rumble) : mapper(rom_size, ram_size, has_bat), rombank{.bank=1}, rambank(0), ram_enabled(false) {}
uint32_t mbc5_rom::map_rom(uint32_t addr, int cycle) {
    if(addr<0x4000) {
        return addr;
    }

    addr -= 0x4000;
    addr+=(uint32_t(rombank.bank) * 0x4000);
    return addr;
}

uint32_t mbc5_rom::map_ram(uint32_t addr, int cycle) {
    if(!ram_enabled) {
        return 0xffffff;
    }
    addr+=(uint32_t(rambank) * 0x2000);
    return 0;
}

void mbc5_rom::write(uint32_t addr, void * val, int size, int cycle) {
    if(addr < 0x2000) {
        ram_enabled = (*((uint8_t *)val) == 0x0a);
    }
    else if(addr < 0x3000) {
        rombank.lower = *((uint8_t *)val);
    }
    else if(addr < 0x4000) {
        rombank.upper = *((uint8_t *)val);
    }
    else if(addr < 0x6000) {
        if(ramsize != 0) {
            rambank = *((uint8_t *)val) % (ramsize / 0x2000);
        }
    }

    if(addr >= 0x2000 && addr < 0x4000) {
        rombank.bank = rombank.bank % (romsize / 0x4000);
    }
}
