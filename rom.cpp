#include "rom.h"
#include<fstream>
#include<iostream>
#include<cassert>

rom::rom(const std::string& rom_filename, const std::string& firmware_filename = "") : valid(false), filename(rom_filename) {
    cram.resize(0);
    firmware = false;
    color_firmware = false;
    //Take input of the actual ROM data

    if(rom_filename.substr(rom_filename.size() - 3, 3) == "zip") {
#ifndef __CYGWIN__
        int retval = util::unzip(rom_filename, rom_data, 32*1024, 8*1024*1024); //All ROMs are expected to be between size 32KB and 8MB
#else
	int retval = 1;
#endif
	if(retval) {
            printf("Got error code %d while trying to extract the zip.\n", retval);
            rom_data.resize(0x8000,0xd3); //invalid opcode, to crash the interpreter
            return;
        }
        else {
            std::cout<<"Opened "<<rom_filename<<", found a file of "<<rom_data.size()<<" bytes."<<std::endl;
        }
    }
    else {
        int retval = util::read(rom_filename, rom_data, 32*1024, 8*1024*1024);
        if(retval) {
            printf("Got error code %d while trying to read the rom.\n", retval);
            rom_data.resize(0x8000, 0xd3); //invalid opcode
            return;
        }
        else {
            std::cout<<"Opened "<<rom_filename<<", found a file of "<<rom_data.size()<<" bytes."<<std::endl;
        }
    }

    //Load the firmware file, if one was provided, and matches the size of the DMG/SGB or CGB firmware files
    if(firmware_filename != "") {
        int retval = util::read(firmware_filename, firmware_data, 256, 256);
        if(!retval) { //Found a 256 byte file at that location; assuming it's valid firmware.
            firmware = true;
        }
        else { //Didn't find DMG/SGB firmware, trying for color
            retval = util::read(firmware_filename, firmware_data, 2304, 2304);
            if(!retval) { //Found valid 2304-byte file at that location; assuming it's firmware
                firmware = true;
                color_firmware = true;
            }
        }

        if(retval) {
            std::cout<<"Didn't find a valid firmware file at "<<firmware_filename<<". Continuing without one."<<std::endl;
        }
    }

    if(firmware) {
        if(color_firmware) { //GBC firmware is in 2 sections, with a "window" to the  original file's header
            rom_backup.resize(2304);
            memcpy(&rom_backup[0],&rom_data[0], 2304);
            memcpy(&rom_data[0],&firmware_data[0],256);
            memcpy(&rom_data[0x200],&firmware_data[0x200],0x700);
        }
        else {
            rom_backup.resize(256);
            memcpy(&rom_backup[0],&rom_data[0], 256);
            memcpy(&rom_data[0],&firmware_data[0],256);
        }
    }

    h.filesize = rom_data.size();
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
        case 0xfc:
            h.has_ram = true; h.has_bat = true; h.has_rtc = false; h.has_rumble = false; h.mapper = MAP_CAMERA;
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

    Vect<uint8_t> rtc_data;

    if(h.has_bat && cram.size() > 0) {
        uint32_t min_size = cram.size();
        uint32_t max_size = cram.size();
        if(h.mapper == MAP_MBC3) { //Take into account the RTC values
            //min_size+=44;
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

    if(h.rom_size > h.filesize) {
        std::cerr<<"Filesize: "<<std::dec<<h.filesize<<" Claimed rom size: "<<h.rom_size<<" (don't be surprised if it crashes!)"<<std::endl;
    }
    else if(h.rom_size < h.filesize) {
        std::cerr<<"Filesize: "<<std::dec<<h.filesize<<" Claimed rom size: "<<h.rom_size<<" (oopsie!)"<<std::endl;
        return;
    }

    h.rom_ver = rom_data[0x14c];

    std::cout<<"Filename: "<<rom_filename<<" Internal name: "<<h.title<<std::endl;
    std::cout<<"ROM size: "<<h.rom_size<<" bytes RAM size: "<<((h.mapper==MAP_MBC2)?256:h.ram_size)<<" bytes Mapper name: "<<mapper_names[h.mapper]<<std::endl;
    std::cout<<"Features: "<<((h.has_ram)?"[RAM] ":"")<<((h.has_bat)?"[BATTERY] ":"")<<((h.has_rtc)?"[TIMER] ":"")<<((h.has_rumble)?"[RUMBLE] ":"")<<((h.has_sensor)?"[TILT/LIGHT] ":"")<<((h.support_sgb)?"[SGB] ":"")<<((h.cgb_flag >= 0x80)?"[COLOR]":"")<<std::endl;

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
            map = new mbc3_rom(h.rom_size, h.ram_size, h.has_bat, h.has_rtc, rtc_data);
            break;
        case MAP_MBC5:
            map = new mbc5_rom(h.rom_size, h.ram_size, h.has_bat, h.has_rumble);
            break;
        case MAP_CAMERA:
            map = new camera_rom(h.rom_size, h.ram_size, h.has_bat, &(cram[0]));
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
            if(h.mapper == MAP_MBC3) {
                Vect<uint8_t> rtc_data = map->read_extra();
                outfile.write(reinterpret_cast<char *>(&rtc_data[0]), rtc_data.size());
            }
            outfile.close();
        }
    }
    delete map;
}

uint8_t rom::read(uint32_t addr, uint64_t cycle) {
    uint8_t retval = 0;
    if(addr < 0x4000) {
        retval = rom_data[addr];
    }
    else if(addr < 0x8000) {
        addr = map->map_rom(addr, cycle);
        if(addr < 0xffffff) {
            retval = rom_data[addr];
        }
        else {
            retval = 0xff;
        }
    }
    else if(addr >= 0xa000 && addr < 0xc000) {
        addr = map->map_ram(addr, cycle);
        if(addr < 0xffffff) { //Messy solution for specifying that an invalid address was requested
            retval = cram[addr];
        }
        else if(addr >= 0xffffff00) { //Messy solution for returning a value stored in the mapper itself (like RTC data in MBC3)
            //printf("Read %02x from camera?\n", addr&0xff);
            uint8_t mapped_val = addr&0xff;
            retval = mapped_val;
        }
        else {
            retval = 0xff;
        }
    }
    return retval;
}

void rom::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    uint32_t orig_addr = addr;
    if(addr >= 0xa000 && addr < 0xc000) {
        addr = map->map_ram(addr, cycle);
        if(addr < 0xffffff) { //in-range RAM mapping, and writes enabled
            cram[addr] = val;
            if(h.mapper == MAP_MBC2) {
                val |= 0xf0;
            }
        }
        //else if(addr == 0xffffff) {} //Address out of range, or RAM disabled
        else if(addr >= 0xffffff00) { //RAM disabled, but mapper 
            //printf("Wrote %02x to mapper register at %04x\n", *((uint8_t *)val),orig_addr);
            map->write(orig_addr, val, cycle);
        }
    }
    else if(addr == 0xff50) memcpy(&rom_data[0], &rom_backup[0], rom_backup.size());
    else {
        map->write(addr, val, cycle);
    }
}

bool rom::supports_sgb() {
    return h.support_sgb;
}

bool rom::needs_color() {
    return h.cgb_flag == 0xc0;
}

bool rom::supports_color() {
    return h.cgb_flag >= 0x80;
}

std::string rom::mapper_names[7] = {"None", "MBC1", "MBC2", "MBC3", "MBC5", "GameBoy Camera", "Unsupported"};
uint32_t rom::rom_sizes[9] = {32*1024, 64*1024, 128*1024, 256*1024, 512*1024, 1024*1024, 2048*1024, 4096*1024, 8192*1024};
uint32_t rom::ram_sizes[6] = {0, 2048, 8192, 32768, 131072, 65536};



//NULL mapper
mapper::mapper(int rom_size, int ram_size, bool bat, bool rtc/*=false*/) : has_bat(bat), has_rtc(rtc), romsize(rom_size), ramsize(ram_size) {}
uint32_t mapper::map_rom(uint32_t addr, uint64_t cycle) {
    return addr;
}
mapper::~mapper() {}
uint32_t mapper::map_ram(uint32_t addr, uint64_t cycle) {
    if(ramsize > addr - 0xa000) {
        return addr-0xa000;
    }
    else {
        return 0xffffff;
    }
    return 0;
}
void mapper::write(uint32_t addr, uint8_t val, uint64_t cycle) { }
Vect<uint8_t> mapper::read_extra() {
    return Vect<uint8_t>(0);
}



//MBC1 mapper
mbc1_rom::mbc1_rom(int rom_size, int ram_size, bool has_bat) : mapper(rom_size, ram_size, has_bat), ram_enabled(false), mode(false) {
    bank.rom_bank = 1; //also sets RAM bank to 0
}
uint32_t mbc1_rom::map_rom(uint32_t addr, uint64_t cycle) {
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
uint32_t mbc1_rom::map_ram(uint32_t addr, uint64_t cycle) {
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
void mbc1_rom::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    if(addr<0x2000 && ((val & 0x0f) == 0x0a)) ram_enabled = true;
    else if(addr<0x2000) ram_enabled = false;
    else if(addr >= 0x2000 && addr < 0x4000) bank.lower = val;
    else if(addr >= 0x4000 && addr < 0x6000) bank.upper = val;
    else mode = val;

    if(addr>=0x2000 && addr < 0x6000) {
        //printf("ROM MBC1: Set ROM bank to %d, rom size: %d\n", bank.rom_bank, romsize);
        bank.rom_bank = bank.rom_bank % (romsize / 16384);
        //printf("ROM MBC1: Set ROM bank to %d after masking\n", bank.rom_bank);
    }
}



//MBC2 mapper
mbc2_rom::mbc2_rom(uint32_t rom_size, bool has_bat) : mapper(rom_size, 512, has_bat), ram_enabled(false), banknum(1) {}
uint32_t mbc2_rom::map_rom(uint32_t addr, uint64_t cycle) {
    return  addr + (uint32_t(banknum) * 0x4000) - 0x4000;
}
uint32_t mbc2_rom::map_ram(uint32_t addr, uint64_t cycle) {
    addr -= 0xa000;
    if(addr >= ramsize || !ram_enabled) {
        return 0xffffff;
    }
    return addr;
}
void mbc2_rom::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    if(addr >= 0x2000 && (addr & 0x100) == 0x100 && addr < 0x4000) banknum = val;
    else if(addr < 0x2000 && addr < 0x4000 && (addr & 0x100) == 0) ram_enabled = val;
}



//MBC3 mapper
mbc3_rom::mbc3_rom(int rom_size, int ram_size, bool has_bat, bool has_rtc, Vect<uint8_t>& rtc_data) : mapper(rom_size, ram_size+5, has_bat), rombank(1), rambank(0), ram_enabled(false), rtc_latch(false), rtc{0,0,0,0,0}, latched_rtc{0,0,0,0,0}, cur_time(0) {
    if(rtc_data.size() == 0) {
        printf("No RTC data found appended to the save data itself.\n");
    }
    else if(rtc_data.size() == 44 || rtc_data.size() == 48) {
        printf("Found RTC data with a %d-bit timestamp.\n", (rtc_data.size()==44)?32:64);
        for(int i=0; i<20;i+=4) {
            rtc[i/4] = rtc_data[i];
        }
        for(int i=20; i<40;i+=4) {
            latched_rtc[(i-20)/4] = rtc_data[i];
        }
        cur_time = uint32_t(rtc_data[40]) + (uint32_t(rtc_data[41])<<8) + (uint32_t(rtc_data[42])<<16) + (uint32_t(rtc_data[43])<<24);
        if(rtc_data.size() == 48) {
            cur_time += (uint64_t(uint32_t(rtc_data[44]) + (uint32_t(rtc_data[45])<<8) + (uint32_t(rtc_data[46])<<16) + (uint32_t(rtc_data[47]<<24)))<<32);
        }
        forward_time();
    }
    else {
        printf("Unexpected RTC data size: %ld", rtc_data.size());
    }
}
uint32_t mbc3_rom::map_rom(uint32_t addr, uint64_t cycle) {
    if(addr < 0x4000) {
        return addr;
    }

    addr -= 0x4000;
    return addr+(uint32_t(rombank) * 0x4000);
}
uint32_t mbc3_rom::map_ram(uint32_t addr, uint64_t cycle) {
    if(rambank < 0x04) {
        addr -= 0xa000;
        if(addr >= ramsize || !ram_enabled) {
            return 0xffffff;
        }
        return addr+(uint32_t(rambank) * 0x2000);
    }
    else if(rambank >= 0x08 && rambank <= 0x0c) {
        //TODO: Make this actually return current time if unlatched, and latched data if latched
        switch(rambank) {
            case 0x08:
                //printf("MBC3 R/W to rtc seconds\n");
                break;
            case 0x09:
                //printf("MBC3 R/W to rtc minutes\n");
                break;
            case 0x0a:
                //printf("MBC3 R/W to rtc hours\n");
                break;
            case 0x0b:
                //printf("MBC3 R/W to rtc days(low)\n");
                break;
            case 0x0c:
                //printf("MBC3 R/W to rtc days(high)\n");
                break;
            default:
                //printf("MBC R/W to RAM bank %x\n", rambank);
                break;
        }

        if(rtc_latch && ram_enabled) {
            return (uint32_t(latched_rtc[rambank-0x08]) | 0xffffff00);
        }
        else {
            0xffffff;
            //return (uint32_t(rtc[rambank-0x08]) | 0xffffff00);
        }
    }
    return 0;
}
void mbc3_rom::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    if(addr < 0x2000) {
        if(val == 0x0a) {
            ram_enabled = true;
        }
        else {
            ram_enabled = false;
        }
    }
    else if(addr < 0x4000) {
        rombank = val;
    }
    else if(addr < 0x6000) {
        rambank = val;
        switch(rambank) {
            case 0x08:
                //printf("MBC3 map to rtc seconds\n");
                break;
            case 0x09:
                //printf("MBC3 map to rtc minutes\n");
                break;
            case 0x0a:
                //printf("MBC3 map to rtc hours\n");
                break;
            case 0x0b:
                //printf("MBC3 map to rtc days(low)\n");
                break;
            case 0x0c:
                //printf("MBC3 map to rtc days(high)\n");
                break;
            default:
                //printf("MBC map to RAM bank %x\n", rambank);
                break;
        }
    }
    else if(addr < 0x8000) {
        if(val == 1 && !rtc_latch) {
            //TODO: actually latch new data here
            //printf("MBC3 latched registers\n");
            rtc_latch = true;
            forward_time();
            for(int i=0;i<5;i++) {
                latched_rtc[i] = rtc[i];
            }
        }
        else if(val == 0) {
            //printf("MBC3 unlatched registers\n");
            rtc_latch = false;
        }
    }
    else if(addr >= 0xa000 && addr < 0xc000) {
        if(rambank > 0x07) {
            //printf("MBC3 write to %s RTC register %02x = 0x%02x\n", ((rtc_latch)?"latched":"unlatched"), rambank, *(uint8_t *)val);
            if(!(rtc[DAY_HIGH] & 0x40)) { //Timer halted before writing to RTC registers
                forward_time();
            }
            rtc[rambank-0x08] = val;
        }
    }
}
Vect<uint8_t> mbc3_rom::read_extra() {
    //TODO: Write RTC out        printf("Found RTC data with a %d-bit timestamp.\n", (rtc_data.size()==44)?32:64);
    Vect<uint8_t> retval(48,0);
    for(int i=0; i<20;i+=4) {
        retval[i] = rtc[i/4]; 
    }
    for(int i=20; i<40;i+=4) {
        retval[i] = rtc[(i-20)/4]; 
    }
    uint64_t mask = 0xff;
    uint64_t shift = 0;
    for(int i=0;i<8;i++) {
        retval[i+40] = ((cur_time & mask)>>shift);
        mask<<=8;
        shift+=8;
    }
    return retval;
}
void mbc3_rom::forward_time() {
    time_t now = time(NULL);
    if(cur_time > now) {
        printf("Warning, save game says it's from the future!\n");
        cur_time = now;
        return;
    }
    double diff = difftime(now, cur_time);
    uint64_t idiff = uint64_t(diff);
    //extract seconds
    lldiv_t convert = lldiv(idiff, 60);
    rtc[SEC] += convert.rem;
    if(rtc[SEC] >= 60) {
        rtc[MIN]++;
        rtc[SEC]%=60;
    }
    //extract minutes
    convert=lldiv(convert.quot, 60);
    rtc[MIN] += convert.rem;
    if(rtc[MIN] >= 60) {
        rtc[HOUR]++;
        rtc[MIN]%=60;
    }
    //extract hours
    convert=lldiv(convert.quot, 24);
    rtc[HOUR] += convert.rem;
    if(rtc[HOUR] >= 24) {
        rtc[DAY_LOW]++;
        rtc[HOUR]%=24;
    }

    convert=lldiv(convert.quot, 256);
    uint64_t temp = uint64_t(rtc[DAY_LOW]) + convert.rem;
    rtc[DAY_LOW] = (temp & 0xff);
    temp/=256;
    rtc[DAY_HIGH] = (rtc[DAY_HIGH] & 0xfe) + (temp & 1);
    if(temp >= 2) {
        rtc[DAY_HIGH] |= 0x80;
    }
    cur_time = now;
}



//GameBoy Pocket Camera mapper
camera_rom::camera_rom(int rom_size, int ram_size, bool has_bat, uint8_t * ramptr) : mapper(rom_size, ram_size, has_bat), rombank(1), rambank(0), ram_enabled(false), cram(ramptr) {
}
uint32_t camera_rom::map_rom(uint32_t addr, uint64_t cycle) {
    if(addr < 0x4000) {
        return addr;
    }

    addr -= 0x4000;
    return addr+(uint32_t(rombank) * 0x4000);
}
uint32_t camera_rom::map_ram(uint32_t addr, uint64_t cycle) {
    addr -= 0xa000;

    if(ram_enabled && rambank < 16) {
        if(rambank == 0) {
            //printf("cam_ram mapped bank %02x:%04x\n", rambank, addr);
        }
        return addr+(uint32_t(rambank) * 0x2000);
    }
    else if(!ram_enabled && rambank < 16) {
        if(rambank == 0) {
            //printf("cam_ram mapped bank %02x:%04x (ram disabled)\n", rambank, addr);
        }
        return 0xffffff00 + cram[addr+uint32_t(rambank) * 0x2000];
    }
    else {
#ifdef CAMERA
        return 0xffffff00 + cam.read(addr,cycle);
#else
        switch(addr%0x80) {
            case 0x00: 
                //printf("camera read: Trigger reg (capture + status)\n"); 
                return 0xffffffaa;
                break;
            case 0x01: 
                //printf("camera read: Reg1 (Output gain+edge ops)\n");
                break;
            case 0x02: 
                //printf("camera read: Reg2 (Exposure time MSB)\n");
                break;
            case 0x03: 
                //printf("camera read: Reg3 (Exposure time LSB)\n");
                break;
            case 0x04: 
                //printf("camera read: Reg7 (Edge, invert, voltage ref)\n");
                break;
            case 0x05: 
                //printf("camera read: Reg0 (out ref volt, zp calc)\n");
                break;
            default:
                if((addr%0x80) >= 0x06 && (addr%0x80) <= 0x35) {
                    //printf("camera read: Dithering register element %d\n", (addr%0x80) - 6);
                }
                else {
                    //printf("camera read: Unknown register at addr %04x\n", addr);
                }
                break;
        }
#endif
        return 0xffffff00;
        //printf("RAM read from out-of-range bank. Reading camera? addr: %d en: %d bank: %d\n", addr+0xa000, ram_enabled, rambank);
        //TODO: Return correct camera status for addr a000
    }

    return 0xffffff;
}
void camera_rom::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    //printf("camera_rom::write addr: %04x val: %02x ", addr, *((uint8_t *)val));
    if(addr < 0x2000) {
        if(val == 0x0a) {
            //printf("(ram on)\n");
            ram_enabled = true;
        }
        else {
            //printf("(ram off)\n");
            ram_enabled = false;
        }
    }
    else if(addr < 0x4000) {
        rombank = (val&0x3f);
        //printf("(rombank to %02x)\n", rombank);
    }
    else if(addr < 0x6000) {
        rambank = (val & 0x1f);
        //printf("(rambank to %02x)\n", rambank);
    }
    else if(addr >= 0xa000 && addr < 0xc000) {
        //printf("(write register: %04x = %02x\n", addr, *((uint8_t *)val));
#ifdef CAMERA
        if(addr == 0xa000 && (val & 0x01) == 1) {
            Vect<uint8_t> output(128*128,0);
            cam.capture(cycle, &(output[0]));

            for(int y=0;y<128;y++) {
                int ty = y / 8;
                int py = y % 8;
                for(int tx=0;tx<16;tx++) {
                    int in_offset = y*128+tx*8;
                    int out_offset = ((ty * 16) + tx) * 16 + py * 2;
                    //printf("Outputting to cram %04x\n", out_offset);
                    cram[out_offset+0] = (output[in_offset+0] & BIT0)<<7;
                    cram[out_offset+1] = (output[in_offset+0] & BIT1)<<6;
                    cram[out_offset+0] |= (output[in_offset+1] & BIT0)<<6;
                    cram[out_offset+1] |= (output[in_offset+1] & BIT1)<<5;
                    cram[out_offset+0] |= (output[in_offset+2] & BIT0)<<5;
                    cram[out_offset+1] |= (output[in_offset+2] & BIT1)<<4;
                    cram[out_offset+0] |= (output[in_offset+3] & BIT0)<<4;
                    cram[out_offset+1] |= (output[in_offset+3] & BIT1)<<3;
                    cram[out_offset+0] |= (output[in_offset+4] & BIT0)<<3;
                    cram[out_offset+1] |= (output[in_offset+4] & BIT1)<<2;
                    cram[out_offset+0] |= (output[in_offset+5] & BIT0)<<2;
                    cram[out_offset+1] |= (output[in_offset+5] & BIT1)<<1;
                    cram[out_offset+0] |= (output[in_offset+6] & BIT0)<<1;
                    cram[out_offset+1] |= (output[in_offset+6] & BIT1)<<0;
                    cram[out_offset+0] |= (output[in_offset+7] & BIT0)>>0;
                    cram[out_offset+1] |= (output[in_offset+7] & BIT1)>>1;
                }
            }
        }
        cam.write(addr, val,cycle);
#else
        const uint8_t image[0x1000] = {
#include "image.hex"
        };
        memcpy(cram, image, 0x1000);
#endif
    }
}



//MBC5 mapper
mbc5_rom::mbc5_rom(int rom_size, int ram_size, bool has_bat, bool has_rumble) : mapper(rom_size, ram_size, has_bat), rombank{.bank=1}, rambank(0), ram_enabled(false) {}
uint32_t mbc5_rom::map_rom(uint32_t addr, uint64_t cycle) {
    if(addr<0x4000) {
        return addr;
    }

    addr -= 0x4000;
    addr+=(uint32_t(rombank.bank) * 0x4000);
    return addr;
}

uint32_t mbc5_rom::map_ram(uint32_t addr, uint64_t cycle) {
    if(!ram_enabled) {
        return 0xffffff;
    }
    addr-=0xa000;
    addr+=(uint32_t(rambank) * 0x2000);
    return addr;
}

void mbc5_rom::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    if(addr < 0x2000) {
        ram_enabled = (val == 0x0a);
    }
    else if(addr < 0x3000) {
        rombank.lower = val;
    }
    else if(addr < 0x4000) {
        rombank.upper = val;
    }
    else if(addr < 0x6000) {
        if(ramsize != 0) {
            rambank = val % (ramsize / 0x2000);
        }
    }

    if(addr >= 0x2000 && addr < 0x4000) {
        rombank.bank = rombank.bank % (romsize / 0x4000);
    }
}

