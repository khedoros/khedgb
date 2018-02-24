#pragma once
#include<cstdint>
#include "util.h"

class apu {
public:
    apu();
    void write(uint16_t addr, uint8_t val, uint64_t cycle);
    uint8_t read(uint16_t addr, uint64_t cycle);
    void run(uint64_t run_to);
private:
    void apply(util::cmd& c);

    union sweep_reg { //NR10
        struct {
            unsigned sweep_shift:3;
            unsigned sweep_direction:1; //0=up, 1=down
            unsigned sweep_time:3;
            unsigned sweep_unused:1;
        };
        uint8_t val;
    };

    union pat_len_reg { //NR11, NR21
        struct {
            unsigned length:6;
            unsigned duty_cycle:2;
        };
        uint8_t val;
    };

    union envelope_reg { //NR12, NR22, NR42
        struct {
            unsigned sweep_shift:3;
            unsigned sweep_direction:1; //0=down, 1=up
            unsigned sweep_volume:4; //initial volume
        };
        uint8_t val;
    };

    //freq = 131072/(2048-x) Hz
    union freq_reg {
        struct {
            unsigned freq:11;
            unsigned freq_unused:3;
            unsigned freq_counter:1;
            unsigned initial:1;
        };
        struct {
            unsigned freq_low:8;      //NR13, NR23, NR33
            unsigned freq_high:8;     //NR14, NR24, NR34
        };
    };

    union wave_on_reg { //NR30
        struct {
            unsigned unused_wave:7;
            unsigned active:1;
        };
        uint8_t val;
    };

    union wave_length { //NR31
        unsigned length:8;
        uint8_t val;
    };

    union wave_level { //NR32
        struct {
            unsigned unused_1:5;
            unsigned level:2;
            unsigned unused_2:1;
        };
        uint8_t val;
    };

    union noise_length { //NR41
        struct {
            unsigned length:5;
            unsigned lengh_unused:3;
        };
        uint8_t val;
    };

    union noise_freq { //NR43
        struct {
            unsigned div_ratio:3;
            unsigned noise_type:1;
            unsigned shift_clock:4;
        };
        uint8_t val;
    };

    union noise_counter { //NR44
        struct {
            unsigned unused:6;
            unsigned freq_counter:1;
            unsigned initial:1;
        };
        uint8_t val;
    };



    //Channel 1, rectangle with sweep
    sweep_reg chan1_sweep;    //0xFF10 NR10
    pat_len_reg chan1_patlen; //0xFF11 NR11
    envelope_reg chan1_env;   //0xFF12 NR12
    freq_reg chan1_freq;      //0xFF13+FF14 NR13+NR14

    //Channel 2, rectangle
    pat_len_reg chan2_patlen; //0xFF16 NR21
    envelope_reg chan2_env;   //0xFF17 NR22
    freq_reg chan2_freq;      //0xFF18+FF19 NR23+NR24

    //Channel 3, wave
    wave_on_reg chan3_on;     //0xFF1A NR30
    wave_length chan3_length; //0xFF1B NR31
    wave_level  chan3_level;  //0xFF1C NR32
    freq_reg    chan3_freq;   //0xFF1D+FF1E NR33+NR34
    uint8_t     wave_pattern[32]; //0xFF30-0xFF3F

    //Channel 4 noise
    noise_length chan4_length; //0xFF20 NR41
    envelope_reg  chan4_env;    //0xFF21 NR42
    noise_freq   chan4_freq;   //0xFF22 NR43
    noise_counter chan4_counter; //0xFF23 NR44

    //Sound control registers
    //TODO: Write these.

};
