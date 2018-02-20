#pragma once
#include<cstdint>

class apu {
public:
    apu();
    void write(uint16_t addr, uint8_t val, uint64_t cycle);
    uint8_t read(uint16_t addr, uint64_t cycle);
    void run(uint64_t run_to);
private:
    void apply();
};
