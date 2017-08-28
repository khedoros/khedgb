#pragma once
#include<cstdint>
//#include<list>
#include<queue>
#include<SDL2/SDL_audio.h>
#include "util.h"
#include<iostream>
#include<fstream>

/* 512 Hz frame sequencer, with 8 phases (2048 cycles between phases)
 * - length is clocked on phases 0, 2, 4, and 6 (256Hz)
 * - volume envelope is clocked on phase 7 (64Hz)
 * - sweep is clocked on phases 2 and 6 (128Hz)
 *
 *  Square 1: Sweep -> Timer -> Duty -> Length Counter -> Envelope -> Mixer
 *  Square 2:          Timer -> Duty -> Length Counter -> Envelope -> Mixer
 *  Wave:              Timer -> Wave -> Length Counter -> Volume   -> Mixer
 *  Noise:             Timer -> LFSR -> Length Counter -> Envelope -> Mixer
 *
 * Length counter:
 * - Channel disables when length hits 0
 * - Writing to length loads an internal counter (length doesn't show the time remaining)
 * 
 * Volume envelope:
 * - Volume stays in range of 0-15
 * - if period is 0, or volume is 0 or 15, it doesn't change
 * 
 * Square wave:
 * - Timer period set by 4*(2048-freq)
 * - 4 duty cycle options:
 *   0 = 00000001  12.5%
 *   1 = 10000001  25%
 *   2 = 10000111  50%
 *   3 = 01111110  75%
 *
 * Freq sweep:
 * - freq is freq +/- freq>>shift
 * - when note is triggered:
 *   - freq copied to sweep shadow register
 *   - sweep timer is reloaded (timer count restarts from 0)
 *   - sweep's internal enabled flag is set if sweep period or shift are non-zero, cleared otherwise
 *   - if sweep shift is non-zero, freq calc and overflow check happen immediately
 *     - freq calc performed as previously noted, using the value in the shadow register
 *     - if resulting value > 2047, channel is disabled.
 * - when sweep is clocked, enabled flag is on, sweep period is non-zero:
 *   - if overflow check passes, the new freq is written to the shadow reg, and NR13+NR14
 *   - overflow check is run AGAIN with the new value (and can disable the channel), but value isn't written back
 * - if NR13+NR14 are changed during operation, the channel will switch back to the old freq when data is copied from shadow, on next sweep
 *
 * Noise channel:
 * - timer period is 0->7, maps to these actual timer counts: {8, 16, 32, 48, 64, 80, 96, 112}
 * - Linear Feedback Shift Register (LFSR) is a 15-bit reg. When asked to produce a value, bits 0+1 are XORed, all are shifted right by 1, and the XOR result goes to bit 14.
 *   If width mode is 1, the result is *also* copied to bit 6 after the shift, producing a 7-bit LFSR. Wave output is bit 0, INVERTED.
 *
 * Wave channel:
 * - Frequency timer is 2*(2048-freq)
 * - Volume control goes 0->3, and produces the following levels: {0%, 100%, 50%, 25%}
 * - Acts weird if wave ram is accessed while channel is active ;-)
 *
 * Note trigger event (write to NRx4 with bit7=1:
 * - Channel enabled
 * - if length is 0, set to 64 (256 for wave)
 * - freq timer is reloaded with period
 * - envelope timer reloaded with period
 * - channel volume reloaded from NRx2
 * - Noise channel's LFSR bits are all set to 1
 * - wave channel's position set to 0, but "buffer is NOT refilled" (think it means that the samples themselves aren't modified)
 * - square 1's sweep does some stuff (described above)
 * - If the channel's DAC is off (upper 5 of NRx2 or upper bit of NR30 for wave are zero), channel is immediately disabled again.
 *
 * DAC:
 * - controlled by top 5 bits of NR12,NR22,NR42 or top bit of NR30.
 * - takes input 0->15, outputs voltage in -1.0 to 1.0 range (using arbitrary voltage units)
 * - DACs output to both mixers. If the channel's active there, the channel's output is added to that mixer.
 * - The mixers output to the master volume control, which multiplies the signal by volume+1.
 * - Single channel enabled by NR51 playing at vol 2 with master 7 is about as loud as that channel at vol 15 and master 0
 *
 * Power:
 * - Cutting power with NR52 clears NR10-NR51, and denies writes to the registers. DMG allows length data to be written, but other models don't.
 * - Doesn't affect wave memory, or the 512Hz timer count.
 * - On boot, wave ram is random-ish, but tends to be the same on a per-unit basis. GBC consistently starts with alternating "00" "FF" values, though.
 *
 *  Extra info here, for after I have the basics coded: http://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
 *
 */

class apu {
public:
    apu();
    ~apu();
    void write(uint16_t addr, uint8_t val, uint64_t cycle);
    uint8_t read(uint16_t addr, uint64_t cycle);
    void run(uint64_t run_to);
    std::ofstream out_wav;
    std::ofstream out_ch3;
    void debug_window(bool);
    void toggle_debug();
private:
    struct samples {
        int8_t l;
        int8_t r;
        int8_t ch1;
        int8_t ch2;
        int8_t ch3;
        int8_t ch4;
    };

    void draw_sample(int sample, samples& s, int accum);
    void display_surface();
    void apply(util::cmd& c);
    void clear(); //Clear CPU-side values
    void clear_regs(); //Clears rendering-side registers
    void init(); //When audio power is turned on
    void render(apu::samples&); //Generate next audio samples
    void clock_sequencer(); //Clock the sequencer
    void clock_freqs(uint64_t c_s = 1); //Clock the frequency counters by c_s cycles
    bool sweep_overflow(); //Check if next sweep iteration should disable the channel due to overflow
    //std::list<util::cmd> cmd_queue;
    std::queue<util::cmd> cmd_queue;

    enum sound_event {
        end_loop, //loop ends before anything but sample rendering happens (based on run_to)
        sequencer, //We hit the 2048-cycle deadline and need to clock the sequencer (based on divisibility by 2048)
        apply_command, //a command needs to be run (based on cycle of next command in queue)
        duty_cycle, //a channel will clock its duty cycle (based on smallest frequency counter variable)
        //render_sample, //This is the variable dependent on the other agents of state change
        output_sample, //use accumulated rendered samples and output an SDL sample (based on calculated next cycle to output on
    };

    //NRx0 Registers
    union sweep_reg { //NR10 0xff10
        struct {
            unsigned shift:3;
            unsigned direction:1; //0=up, 1=down
            unsigned period:3;
            unsigned unused:1;
        } __attribute__((packed));
        uint8_t val;
    };

    union wave_on_reg { //NR30 0xff1a
        struct {
            unsigned unused_wave:7;
            unsigned active:1;
        } __attribute__((packed));
        uint8_t val;
    };
    
    //NRx1 Registers
    union pat_len_reg { //NR11 0xff11, NR21 0xff16, NR41 0xff20
        struct {
            unsigned length:6;
            unsigned duty_cycle:2; //used for ch1+2, but not ch4
        } __attribute__((packed));
        uint8_t val;
    };

    union wave_length { //NR31
        unsigned length:8;
        uint8_t val;
    };

    //NRx2 Registers
    union envelope_reg { //NR12 0xff12, NR22 0xff17, NR42 0xff21
        struct {
            unsigned period:3;
            unsigned direction:1; //0=down, 1=up
            unsigned volume:4; //initial volume
        };
        uint8_t val;
    };

    union wave_level { //NR32
        struct {
            unsigned unused_1:5;
            unsigned level_shift_index:2;
            unsigned unused_2:1;
        } __attribute__((packed));
        uint8_t val;
    };

    //NRx3 + NRx4 registers
    union freq_reg {
        struct {
            unsigned freq:11;
            unsigned freq_unused:3;
            unsigned length_enable:1;
            unsigned initial:1;
        } __attribute__((packed));
        struct {
            unsigned freq_low:8;      //NR13 0xff13, NR23, 0xff18 NR33 0xff1d
            unsigned freq_high:8;     //NR14 0xff14, NR24, 0xff19 NR34 0xff1e
        } __attribute__((packed));
    };

    union noise_freq { //NR43 0xff22
        struct {
            unsigned div_code:3;
            unsigned noise_type:1;
            unsigned clock_shift:4;
        } __attribute__((packed));
        uint8_t val;
    };

    union noise_count_init { //NR44 0xff23
        struct {
            unsigned unused:6;
            unsigned length_enable:1;
            unsigned initial:1;
        } __attribute__((packed));
        uint8_t val;
    };

    //Global status/control registers
    union output_levels { //NR50 0xff24
        struct {
            unsigned so1_level:3;
            unsigned vin_to_so1:1;
            unsigned so2_level:3;
            unsigned vin_to_so2:1;
        } __attribute__((packed));
        uint8_t val;
    };

    union channel_map { //NR51 0xff25
        struct {
            unsigned ch1_to_so1:1;
            unsigned ch2_to_so1:1;
            unsigned ch3_to_so1:1;
            unsigned ch4_to_so1:1;
            unsigned ch1_to_so2:1;
            unsigned ch2_to_so2:1;
            unsigned ch3_to_so2:1;
            unsigned ch4_to_so2:1;
        } __attribute__((packed));
        uint8_t val;
    };

    union channel_status { //NR52 0xff26
        struct {
            unsigned chan1_playing:1;
            unsigned chan2_playing:1;
            unsigned chan3_playing:1;
            unsigned chan4_playing:1;
            unsigned unused:3;
            unsigned enable_sound:1;
        } __attribute__((packed));
        uint8_t val;
    };

    bool writes_enabled;
    uint64_t cycle;
    unsigned int sequencer_phase; //Which of 8 phases are we in, for clocking length, volume, sweep?

    //Definitions of the waveforms for the square waves
    static const int8_t square_wave[4][8];      //The 4 actual square waves
    static const uint8_t square_wave_length = 8; //Number of steps in the wave output

    //Channel 1, rectangle with sweep
    sweep_reg chan1_sweep;    //0xFF10 NR10
    pat_len_reg chan1_patlen; //0xFF11 NR11
    envelope_reg chan1_env;   //0xFF12 NR12
    freq_reg chan1_freq;      //0xFF13+FF14 NR13+NR14
    bool chan1_active;
    bool chan1_sweep_active;
    uint16_t chan1_length_counter; //Counts how many steps until channel is silenced
    int16_t chan1_freq_counter;   //Counts how many steps until waveform is clocked
    uint16_t chan1_env_counter;    //Counts how many steps until envelope is clocked
    uint16_t chan1_sweep_counter;  //Counts how many steps until sweep is clocked
    uint16_t chan1_freq_shadow;    //Frequency shadow register used by sweep
    uint8_t chan1_duty_phase;      //Which waveform sample is it on

    //Channel 2, rectangle
    pat_len_reg chan2_patlen; //0xFF16 NR21
    envelope_reg chan2_env;   //0xFF17 NR22
    freq_reg chan2_freq;      //0xFF18+FF19 NR23+NR24
    bool chan2_active;
    uint16_t chan2_length_counter; //Counts how many steps until channel is silenced
    int16_t chan2_freq_counter;   //Counts how many steps until waveform is clocked
    uint16_t chan2_env_counter;    //Counts how many steps until envelope is clocked
    uint8_t chan2_duty_phase;      //Which waveform sample is it on

    //Channel 3, wave
    wave_on_reg chan3_on;     //0xFF1A NR30
    wave_length chan3_length; //0xFF1B NR31
    wave_level  chan3_level;  //0xFF1C NR32
    freq_reg    chan3_freq;   //0xFF1D+FF1E NR33+NR34
    uint8_t     wave_pattern[32]; //0xFF30-0xFF3F
    static const uint8_t wave_length = 32; //Number of elements in the wave registers
    static const uint8_t wave_shift[4]; //wave volume shift levels
    bool chan3_active;
    bool chan3_dac;
    uint16_t chan3_length_counter;
    int16_t chan3_freq_counter;
    uint8_t     chan3_duty_phase;   //Which sample is the current one
    int8_t chan3_cur_sample;

    //Channel 4 noise
    pat_len_reg chan4_length; //0xFF20 NR41
    envelope_reg  chan4_env;    //0xFF21 NR42
    noise_freq   chan4_freq;   //0xFF22 NR43
    noise_count_init chan4_counter; //0xFF23 NR44 Enables use of length counter, and inits playback
    static const uint8_t noise_divisors[8];
    bool chan4_active;
    int32_t chan4_freq_counter; //How many steps until lfsr is clocked
    uint16_t chan4_env_counter;
    uint16_t chan4_lfsr;         //Linear Feedback Shift Register for noise output
    int8_t lfsr_value;          //Current output value of LFSR
    int16_t chan4_length_counter; //Length to play

    //Sound control registers
    output_levels levels;      //0xFF24 NR50
    channel_map   output_map;  //0xFF25 NR51
    channel_status status;     //0xFF26 NR52

    uint8_t written_values[0x30]; //For reading from CPU. NR52 should be the only more-complicated one.

    //Values to OR onto the written values when read
    static const uint8_t or_values[0x30];

    SDL_AudioDeviceID devid; //Device ID that SDL assigned our output device
    bool audio_open;         //Whether we successfully opened an audio device
    const static int CHANNELS = 2;
    const static int SAMPLE_SIZE = 1;

    bool debug; //Display debug window?
    SDL_Window * screen;
    SDL_Renderer * renderer;
    SDL_Surface * buffer; //Output buffer
    SDL_Texture * texture; //Texture used for output
};
