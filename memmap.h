#pragma once
#include<cstdint>
#include "mapper.h"
#include "rom.h"
#include<string>
#include<vector>

class memmap {
public:
    memmap(const std::string& filename);
    //read: rw==false, write: rw==true
    void map(int addr, void * val, int size, bool rw);
    //void map(int addr, void * const val, int size, bool rw);
private:
    std::vector<uint8_t> rom;   
    static const uint8_t dmg_firmware[256];
    bool use_dmg;
};
