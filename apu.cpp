#include<SDL2/SDL.h>
#include "apu.h"
#include<cstdio>
#include<cassert>

#ifdef DEBUG
#define APRINTF printf
#else
#define APRINTF //printf
#endif

const uint8_t apu::or_values[] = {0x80, 0x3f, 0x00, 0xff, 0xbf, //0x10,0x11,0x12,0x13,0x14
                                  0xff, 0x3f, 0x00, 0xff, 0xbf, //0x15,0x16,0x17,0x18,0x19
                                  0x7f, 0xff, 0x9f, 0xff, 0xbf, //0x1a,0x1b,0x1c,0x1d,0x1e,
                                  0xff, 0xff, 0x00, 0x00, 0xbf, //0x1f,0x20,0x21,0x22,0x23,
                                  0x00, 0x00, 0x70, 0xff, 0xff, //0x24,0x25,0x26,0x27,0x28
                                  0xff, 0xff, 0xff, 0xff, 0xff, //0x29,0x2a,0x2b,0x2c,0x2d
                                  0xff, 0xff,                   //0x2e,0x2f
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //0x30-0x37
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //0x38-0x3f

const int8_t apu::square_wave[4][8] = {{-1,-1,-1,-1,-1,-1,-1, 1},
                                       { 1,-1,-1,-1,-1,-1,-1, 1},
                                       { 1,-1,-1,-1,-1, 1, 1, 1},
                                       {-1, 1, 1, 1, 1, 1, 1,-1}};

const uint8_t apu::noise_divisors[] = {8, 16, 32, 48, 64, 80, 96, 112};

const uint8_t apu::wave_shift[] = {4, 0, 1, 2};

void apu::clear() {
    for(int i=0; i<0x30;i++) {
        written_values[i] = 0;
    }
}

void apu::clear_regs() {
    //Channel 1, rectangle with sweep
    chan1_sweep.val = 0;    //0xFF10 NR10
    chan1_patlen.val = 0; //0xFF11 NR11
    chan1_env.val = 0;   //0xFF12 NR12
    chan1_freq.freq_high = 0;      //0xFF13+FF14 NR13+NR14
    chan1_freq.freq_low = 0;      //0xFF13+FF14 NR13+NR14
    chan1_active = false;

    //Channel 2, rectangle
    chan2_patlen.val = 0; //0xFF16 NR21
    chan2_env.val = 0;   //0xFF17 NR22
    chan2_freq.freq_high = 0;      //0xFF18+FF19 NR23+NR24
    chan2_freq.freq_low = 0;      //0xFF18+FF19 NR23+NR24
    chan2_active = false;

    //Channel 3, wave
    chan3_on.val = 0;     //0xFF1A NR30
    chan3_length.val = 0; //0xFF1B NR31
    chan3_level.val = 0;  //0xFF1C NR32
    chan3_freq.freq_high = 0;   //0xFF1D+FF1E NR33+NR34
    chan3_freq.freq_low = 0;   //0xFF1D+FF1E NR33+NR34
    chan3_active = false;

    //Channel 4 noise
    chan4_length.val = 0; //0xFF20 NR41
    chan4_env.val = 0;    //0xFF21 NR42
    chan4_freq.val = 0;   //0xFF22 NR43
    chan4_counter.val = 0; //0xFF23 NR44 Enables use of length counter, and inits playback
    chan4_active = false;

    //Sound control registers
    levels.val = 0;      //0xFF24 NR50
    output_map.val = 0;  //0xFF25 NR51
}

void apu::init() {
    sequencer_phase = 7; //so that next step is 0
    chan1_duty_phase = 0;
    chan2_duty_phase = 0;
    chan3_duty_phase = 0;
    clear_regs();
}

void null_callback(void * userdata, Uint8* stream, int len) {}

apu::apu() : writes_enabled(false), cycle(0), devid(0), audio_open(false), cur_chunk(0)
{
    assert(sizeof(sweep_reg) == 1);
    assert(sizeof(wave_on_reg) == 1);
    assert(sizeof(pat_len_reg) == 1);
    assert(sizeof(wave_length) == 1);
    assert(sizeof(wave_level) == 1);
    assert(sizeof(freq_reg) == 2);
    assert(sizeof(noise_freq) == 1);
    assert(sizeof(noise_count_init) == 1);
    assert(sizeof(output_levels) == 1);
    assert(sizeof(channel_map) == 1);
    assert(sizeof(channel_status) == 1);
    clear();
    clear_regs();
    int num_drivers = SDL_GetNumAudioDrivers();
    unsigned int chosen_driver = 255;
    unsigned int driver_desirability = 255;
    std::array<std::array<const char, 20>, 6> desired_drivers = {"directsound", "winmm", "pulse", "pulseaudio", "alsa", "dummy"};
    printf("Audio Drivers: \n");
    for(int i=0; i<num_drivers;i++) {
        printf("\tDriver %d: \"%s\"\n", i, SDL_GetAudioDriver(i));
        for(int j=0;j<desired_drivers.size();j++) {
            if(strncmp((desired_drivers[j].data()), SDL_GetAudioDriver(i), 20) == 0 && j < driver_desirability) {
                chosen_driver = i;
                driver_desirability = j;
            }
        }
    }

    if(chosen_driver != 255) {
        printf("Chose driver: #%d (%s)\n", chosen_driver, SDL_GetAudioDriver(chosen_driver));

        int failure_code = SDL_AudioInit(SDL_GetAudioDriver(chosen_driver));
        if(failure_code) {
            fprintf(stderr, "Error init'ing driver: %s", SDL_GetError());
        }

        int num_playback_devices = SDL_GetNumAudioDevices(0);
        printf("Audio Devices: \n\tPlayback:\n");
        for(int i=0; i<num_playback_devices;i++) {
            printf("\t\tDevice %d: %s\n", i, SDL_GetAudioDeviceName(i,0));
            if(!devid) {
                SDL_AudioSpec want;
                    want.freq=44100;
                    want.format=AUDIO_S8;
                    want.channels=CHANNELS;
                    want.silence=0;
                    want.samples=/*8192*/ 689;
                    want.size=0;
                    want.callback=NULL;//null_callback;
                    want.userdata=NULL;

                SDL_AudioSpec got;
                devid = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i,0), 0, &want, &got, 0);
                if(!devid) {
                    fprintf(stderr, "Error opening device %s: %s\n", SDL_GetAudioDeviceName(i,0), SDL_GetError());
                }
                else {
                    printf("Freq: %d format: %d (%d) ch: %d sil: %d samp: %d size: %d\n", got.freq, got.format, want.format, got.channels, got.silence, got.samples, got.size);
                }
            }
        }
        if(!devid) {
            fprintf(stderr, "No audio device opened.\n");
        }
        else {
            audio_open = true;
            SDL_PauseAudioDevice(devid, 0);
        }
    }
    else {
        fprintf(stderr, "No suitable drivers available.\n");
    }
    out.open("audio.out");
}

apu::~apu() {
    out.close();
}

void apu::write(uint16_t addr, uint8_t val, uint64_t cycle) {
    //printf("addr: %04x val: %02x ", addr, val);
    if(writes_enabled || addr >= 0xff30) {
        written_values[addr - 0xff10] = val;
        cmd_queue.emplace_back(util::cmd{cycle, addr, val, 0});
    }
    else if(addr == 0xff26) {
        written_values[addr - 0xff10] = (val & 0x80);
        if(!(val&0x80)) {
            clear();
            writes_enabled = false;
        }
        else {
            writes_enabled = true;
        }
        cmd_queue.emplace_back(util::cmd{cycle, addr, val, 0});
    }
}

uint8_t apu::read(uint16_t addr, uint64_t cycle) {
    if(addr == 0xff26) {
        return 0;
    }
    return written_values[addr - 0xff10] | or_values[addr - 0xff10];
    //TODO: Calculate status for NR52
    //TODO: Block reads from wave RAM when it's in use (or, rather, return current wave value)
    //TODO: Block reads from most registers when power is off
}

//Gets called at 64Hz, every 16384 1MHz-cycles
void apu::run(uint64_t run_to) {
    //printf("apu:: %ld->%ld\n", cycle, run_to);

    //Generate audio between cycle and cmd.cycle
    util::cmd cur_cmd;
    if(!cmd_queue.empty()) {
        cur_cmd = cmd_queue.front();
    }
    int cur_sample = 0;

    Arr<int8_t, 690*2> out_buffer;
    memset(out_buffer.data(),0,690*2);
    int sample_count = 689;
    if(cur_chunk == 15) {
        //sample_count++;
    }
    uint64_t start_cycle = cycle;
    int32_t left = 0;
    int32_t right = 0;
    int32_t sample_accum = 0;
    for(;cycle < run_to; cycle++) {

        //Clock the frame sequencer at 512Hz
        if(cycle%2048 == 0) {
            clock_sequencer();
        }

        //Clock the frequency counters for each channel
        clock_freqs();

        //If it's time, apply the current command, and grab the next one, if it exists
        while(!cmd_queue.empty() && cycle >= cur_cmd.cycle) {
            apply(cur_cmd);
            cmd_queue.pop_front();
            if(!cmd_queue.empty()) {
                cur_cmd = cmd_queue.front();
            }
        }

        apu::samples s;
        render(s);
        left+=s.l;
        right+=s.r;
        sample_accum++;

        //Figure out if it's time to generate a new sample, and put it into the output buffer if it is
        int sample = int(float(sample_count) * (float(cycle - start_cycle) / float(run_to - start_cycle)));
        if(sample > cur_sample) {
            //printf("L: %d R: %d acc: %d\n", left, right, sample_accum);
            out_buffer[cur_sample*2] = left/sample_accum;
            out_buffer[cur_sample*2+1] = right/sample_accum;
            //assert(sample_accum == sample_count);
            cur_sample++;
            left=0;
            right=0;
            sample_accum=0;
        }
    }

    //Push the sample to the output device.
    if(audio_open && SDL_GetQueuedAudioSize(devid) < 4000) {
        //printf("%d ", SDL_GetQueuedAudioSize(devid));
        //out.write(reinterpret_cast<const char *>(out_buffer.data()), sample_count * CHANNELS * SAMPLE_SIZE);
        SDL_QueueAudio(devid, out_buffer.data(), sample_count * CHANNELS * SAMPLE_SIZE);
    }

    //Every 16 chunks, we need to generate 690 samples, instead of 689.
    cur_chunk++;
    cur_chunk &= 0x0f; //mod by 16
}

void apu::render(apu::samples& s) {
    s.l = 0;
    s.r = 0;
    int32_t chan1 = 0;
    int32_t chan2 = 0;
    int32_t chan3 = 0;
    int32_t chan4 = 0;
    if(chan1_active) {
        chan1 = chan1_level * square_wave[chan1_patlen.duty_cycle][chan1_duty_phase];
        assert(chan1 > -129 && chan1 < 128);
        //printf("ch1: %02x ", chan1&0xff);
    }
    //else if(chan2_active || chan3_active || chan4_active) printf("        ");

    if(chan2_active) {
        chan2 = chan2_level * square_wave[chan2_patlen.duty_cycle][chan2_duty_phase];
        assert(chan2 > -129 && chan2 < 128);
        //printf("ch2: %02x ", chan2&0xff);
    }
    //else if(chan1_active || chan3_active || chan4_active) printf("        ");

    if(chan3_active) {
        //chan3 = 2 * (chan3_cur_sample>>(wave_shift[chan3_level.level_shift_index]));// - (8 / (1<<(wave_shift[chan3_level.level_shift_index])));
        chan3 = 2 * ((chan3_cur_sample) / (1<<(wave_shift[chan3_level.level_shift_index]))) - (8>>(wave_shift[chan3_level.level_shift_index]));
        assert(chan3 > -129 && chan3 < 128);
        //printf("ch3: %d \n", chan3);
    }
    //else if(chan2_active || chan1_active || chan4_active) printf("        ");

    if(chan4_active) {
        chan4 = chan4_level * lfsr_value;
        assert(chan4 > -129 && chan4 < 128);
        //printf("ch4: %02x \n", chan4&0xff);
    }
    //else if(chan2_active || chan3_active || chan1_active) printf("        \n");

    if(output_map.ch1_to_so1) s.l += chan1;
    if(output_map.ch2_to_so1) s.l += chan2;
    if(output_map.ch3_to_so1) s.l += chan3;
    if(output_map.ch4_to_so1) s.l += chan4;
    if(output_map.ch1_to_so2) s.r += chan1;
    if(output_map.ch2_to_so2) s.r += chan2;
    if(output_map.ch3_to_so2) s.r += chan3;
    if(output_map.ch4_to_so2) s.r += chan4;

    if(output_map.val == 0) {
        assert(s.l == 0 && s.r == 0);
    }

    //printf("chan1 (l: %d v: %d) chan2 (l: %d v: %d) templ: %d tempr: %d\n", chan1_level, chan1, chan2_level, chan2, templ, tempr);

    //printf("sl: %d sr: %d\n", s.l, s.r);
    //printf("APU Render cycle %ld to %ld\n", cycle, render_to);
    return;
}

bool apu::sweep_overflow() {
    int16_t freq = chan1_freq_shadow;
    int16_t delta = (freq>>chan1_sweep.shift);
    if(chan1_sweep.direction) delta *=-1;
    freq+=delta;
    if(freq>=2048) return true;
    return false;
}

//Advance the frame sequencer. Clocks at 512Hz, with 8 phases.
// Phase 0: Clock Length
// Phase 1:
// Phase 2: Clock Length and Sweep
// Phase 3:
// Phase 4: Clock Length
// Phase 5:
// Phase 6: Clock Length and Sweep
// Phase 7: Clock Volume
void apu::clock_sequencer() {
    sequencer_phase++;
    sequencer_phase %= 8;
    if(sequencer_phase == 0 || sequencer_phase == 2 || sequencer_phase == 4 || sequencer_phase == 6) {
        //clock length for all the channels
        if(chan1_active && chan1_freq.length_enable) {
            chan1_length_counter--;
            if(chan1_length_counter == 0) chan1_active = false;
        }

        if(chan2_active && chan2_freq.length_enable) {
            chan2_length_counter--;
            if(chan2_length_counter == 0) chan2_active = false;
        }

        if(chan3_active && chan3_freq.length_enable) {
            chan3_length_counter--;
            if(chan3_length_counter == 0) chan3_active = false;
        }

        if(chan4_active && chan4_counter.length_enable) {
            chan4_length_counter--;
            if(chan4_length_counter == 0) chan4_active = false;
        }
    }
    if((sequencer_phase == 2 || sequencer_phase == 6) && chan1_sweep_active) {
        //clock sweep for channel 1 (only one that supports it)
        if(chan1_active && chan1_sweep_active && chan1_sweep.period) {
            chan1_sweep_counter--;
            if(chan1_sweep_counter == 0) {
                chan1_sweep_counter = chan1_sweep.period;
                int16_t freq = chan1_freq_shadow;
                int16_t delta = (freq>>chan1_sweep.shift);
                if(!chan1_sweep.direction) delta *=-1;
                freq+=delta;
                if(freq > 2047) {
                    chan1_active = false;
                    chan1_sweep_active = false;
                }
                else {
                    chan1_freq_shadow = util::clamp(0,freq,2047);
                    freq = 2048 - chan1_freq_shadow;
                    chan1_freq.freq = freq;
                }
                if(sweep_overflow()) {
                    chan1_active = false;
                    chan1_sweep_active = false;
                }
            }
        }
    }
    if(sequencer_phase == 7) {
        //clock volume for 1, 2, and 4? I think chan3 has level, but not envelope.
        if(chan1_active && chan1_level > 0 && chan1_env.period) {
            chan1_env_counter--;
            if(!chan1_env_counter) {
                if(chan1_env.direction && chan1_level < 15) {
                    chan1_level++;
                }
                else if(!chan1_env.direction && chan1_level > 0) {
                    chan1_level--;
                }
                else if(!chan1_env.direction && !chan1_level) {
                    chan1_active = false;
                }
                chan1_env_counter = chan1_env.period;
                if(!chan1_env_counter) chan1_env_counter = 8;
            }
        }
        if(chan2_active && chan2_level > 0 && chan2_env.period) {
            chan2_env_counter--;
            if(!chan2_env_counter) {
                if(chan2_env.direction && chan2_level < 15) {
                    chan2_level++;
                }
                else if(!chan2_env.direction && chan2_level > 0) {
                    chan2_level--;
                }
                else if(!chan2_env.direction && !chan2_level) {
                    chan2_active = false;
                }
                chan2_env_counter = chan2_env.period;
                if(!chan2_env_counter) chan2_env_counter = 8;
            }
        }
        if(chan4_active && chan4_level > 0 && chan4_env.period) {
            chan4_env_counter--;
            if(!chan4_env_counter) {
                if(chan4_env.direction && chan4_level < 15) {
                    chan4_level++;
                }
                else if(!chan4_env.direction && chan4_level > 0) {
                    chan4_level--;
                }
                else if(!chan4_env.direction && !chan4_level) {
                    chan4_active = false;
                }
                chan4_env_counter = chan4_env.period;
                if(!chan4_env_counter) chan4_env_counter = 8;
            }
        }
    }
}

void apu::clock_freqs() {
    if(chan1_active) {
        chan1_freq_counter--;
        if(chan1_freq_counter <= 0) {
            chan1_freq_counter += chan1_freq_shadow;
            chan1_duty_phase++;
            chan1_duty_phase %= square_wave_length;
        }
    }
    if(chan2_active) {
        chan2_freq_counter--;
        if(chan2_freq_counter <= 0) {
            chan2_freq_counter += (2048 - chan2_freq.freq);
            chan2_duty_phase++;
            chan2_duty_phase %= square_wave_length;
        }
    }
    if(chan3_active) {
        chan3_freq_counter--;
        if(chan3_freq_counter <= 0) {
            chan3_freq_counter += (2048 - chan3_freq.freq);
            chan3_duty_phase++;
            chan3_duty_phase %= wave_length;
            if((chan3_duty_phase & 1) == 1) {
                chan3_cur_sample = (wave_pattern[chan3_duty_phase] & 0x0f);
            }
            else {
                chan3_cur_sample = ((wave_pattern[chan3_duty_phase] & 0xf0)>>4);
            }
        }
    }
    if(chan4_active) {
        chan4_freq_counter--;
        if(chan4_freq_counter <= 0) {
            chan4_freq_counter = (noise_divisors[chan4_freq.div_code]<<(chan4_freq.clock_shift))/4;
            uint16_t next = (chan4_lfsr & BIT0) ^ ((chan4_lfsr & BIT1)>>1);
            chan4_lfsr>>=1;
            chan4_lfsr |= (next<<14);
            if(chan4_freq.noise_type) {
                //chan4_lfsr &= 32735;
                chan4_lfsr ^= (next<<5);
            }
            lfsr_value = ((next)?-1:1);
            //printf("lfsr out: %d\n", lfsr_value);
        }
    }
}

void apu::apply(util::cmd& c) {
    APRINTF("apply %04x = %02x @ %ld", c.addr,c.val,c.cycle);
    switch(c.addr) {
        //sound 1: rectangle with sweep + envelope
        case 0xff10: //sound 1 sweep
            APRINTF(" (ch1 sweep)\n");
            chan1_sweep.val = c.val;
            //APRINTF("apu: S1 sweep time: %d increase: %d shift: %d\n", (c.val&0x70)>>4, (c.val&8)>>3, (c.val&7));
            break;
        case 0xff11: //sound 1 length + duty cycle
            APRINTF(" (ch1 duty+length)\n");
            chan1_patlen.val = c.val;
            //APRINTF("apu: S1 duty cycle: %d length: %d\n", c.val>>6, (c.val&0x3f));
            break;
        case 0xff12: //sound 1 envelope
            APRINTF(" (ch1 envelope)\n");
            chan1_env.val = c.val;
            //APRINTF("apu: S1 default envelope: %d up/down: %d step length: %d\n", c.val>>4, (c.val&8)>>3, (c.val&7));
            break;
        case 0xff13: //sound 1 low-order frequency
            APRINTF(" (ch1 low-freq)\n");
            chan1_freq.freq_low = c.val;
            //APRINTF("apu: S1 freq-low: %d\n", c.val);
            break;
        case 0xff14: //sound 1 high-order frequency + start
            chan1_freq.freq_high = c.val;
            if(chan1_freq.initial) {
                APRINTF(" (ch1 trigger)\n");
                chan1_freq_shadow = 2048 - chan1_freq.freq;
                chan1_sweep_counter = chan1_sweep.period;
                if(!chan1_sweep_counter) chan1_sweep_counter = 8;
                chan1_active = true;
                if(chan1_sweep.shift != 0 && sweep_overflow()) {
                    chan1_active = false;
                }
                if(chan1_active && (chan1_sweep.shift || chan1_sweep_counter)) {
                    chan1_sweep_active = true;
                }
                chan1_length_counter = 64 - chan1_patlen.length;
                chan1_level = chan1_env.volume;
                chan1_env_counter = chan1_env.period;
                if(!chan1_env_counter) chan1_env_counter = 8;
                chan1_freq_counter = chan1_freq_shadow;
            }
            else {
                APRINTF(" (ch1 stop)\n");
                //chan1_active = false;
            }
            //APRINTF("apu: S1 start: %d freq-high: %d\n", c.val>>7, (0x03&c.val));
            break;

        //sound 2: rectangle with envelope
        case 0xff16: //sound 2 length + duty cycle
            APRINTF(" (ch2 duty+length)\n");
            chan2_patlen.val = c.val;
            //APRINTF("apu: S2 duty cycle: %d length: %d\n", c.val>>6, (c.val&0x3f));
            break;
        case 0xff17: //sound 2 envelope
            APRINTF(" (ch2 envelope)\n");
            chan2_env.val = c.val;
            //APRINTF("apu: S2 default envelope: %d up/down: %d step length: %d\n", c.val>>4, (c.val&8)>>3, (c.val&7));
            break;
        case 0xff18: //sound 2 low-order frequency
            APRINTF(" (ch2 low-freq)\n");
            chan2_freq.freq_low = c.val;
            //APRINTF("apu: S2 freq-low: %d\n", c.val);
            break;
        case 0xff19: //sound 2 high-order frequency + start
            chan2_freq.freq_high = c.val;
            if(chan2_freq.initial) {
                APRINTF(" (ch2 trigger)\n");
                chan2_active = true;
                chan2_length_counter = 64 - chan2_patlen.length;
                chan2_freq_counter = 2048 - chan2_freq.freq;
                chan2_level = chan2_env.volume;
                chan2_env_counter = chan2_env.period;
                if(!chan2_env_counter) chan2_env_counter = 8;
            }
            else {
                APRINTF(" (ch2 stop)\n");
                //chan2_active = false;
            }
            //APRINTF("apu: S2 start: %d freq-high: %d\n", c.val>>7, (0x03&c.val));
            break;

        //sound 3: arbitrary waveform
        case 0xff1a: //sound 3 start/stop
            if(c.val & BIT7) {
                APRINTF(" (ch3 DAC on)\n");
                //chan3_active = true;
                chan3_dac = true;
            }
            else {
                APRINTF(" (ch3 DAC off)\n");
                //chan3_active = false;
                chan3_dac = false;
            }
            chan3_on.val = c.val;
            //APRINTF("apu: S3 enable: %d\n", c.val>>7);
            break;
        case 0xff1b: //sound 3 length
            APRINTF(" (ch3 length)\n");
            chan3_length.val = c.val;
            //APRINTF("apu: S3 length: %d\n", c.val);
            break;
        case 0xff1c: //sound 3 output level
            APRINTF(" (ch3 level)\n");
            chan3_level.val = c.val;
            //APRINTF("apu: S3 level: %d\n", (c.val&0x60)>>5);
            break;
        case 0xff1d: //sound 3 low-order frequency
            APRINTF(" (ch3 low-freq)\n");
            chan3_freq.freq_low = c.val;
            //APRINTF("apu: S3 freq-low: %d\n", c.val);
            break;
        case 0xff1e: //sound 3 high-order frequency + start
            chan3_freq.freq_high = c.val;
            if(chan3_freq.initial) {
                APRINTF(" (ch3 trigger)\n");
                chan3_active = true;
                chan3_freq_counter = 2048 - chan3_freq.freq;
                chan3_length_counter = 256 - chan3_length.length;
                chan3_duty_phase = 0;
                if(!chan3_on.active) chan3_active = false;
                APRINTF("chan3_trigger level index: %d, level shift: %d, freq: %d, length: %d, DAC: %d, chan is: %s\n", chan3_level.level_shift_index, wave_shift[chan3_level.level_shift_index], chan3_freq_counter, chan3_length_counter, chan3_on.active,(chan3_active?"on":"off"));
            }
            else {
                APRINTF(" (ch3 stop)\n");
                //chan3_active = false;
            }
            //APRINTF("apu: S3 start: %d freq-high: %d\n", c.val>>7, (0x03&c.val));
            break;

        //sound 4: noise
        case 0xff20: //sound 4 length
            //APRINTF("apu: S4 length: %d\n", c.val&0x3f);
            APRINTF(" (ch4 length)\n");
            chan4_length.val = c.val;
            break;
        case 0xff21: //sound 4 envelope
            //APRINTF("apu: S4 default envelope: %d up/down: %d step length: %d\n", c.val>>4, (c.val&0x08)>>3, (c.val&7));
            APRINTF(" (ch4 envelope)\n");
            chan4_env.val = c.val;
            break;
        case 0xff22: //sound 4 frequency settings
            APRINTF(" (ch4 freq)\n");
            chan4_freq.val = c.val;
            //APRINTF("apu: S4 shift freq: %d counter steps: %d freq division ratio: %d\n", c.val>>4, (c.val&0x08)>>3, (c.val&7));
            break;
        case 0xff23: //sound 4 start
            chan4_counter.val = c.val;
            if(chan4_counter.initial) {
                APRINTF(" (ch4 trigger)\n");
                chan4_active = true;
                chan4_length_counter = 64 - chan4_length.length;
                chan4_freq_counter = (noise_divisors[chan4_freq.div_code]<<(chan4_freq.clock_shift ))/4;
                //APRINTF("ch4 freq: %d (divisor %d, ratio %d, shift %d) type: %d\n", chan4_freq_counter, noise_divisors[chan4_freq.div_ratio], chan4_freq.div_ratio, chan4_freq.shift_clock, chan4_freq.noise_type);
                chan4_lfsr = 32767;
                chan4_level = chan4_env.volume;
                chan4_env_counter = chan4_env.period;
                if(!chan4_env_counter) chan4_env_counter = 8;
            }
            else {
                APRINTF(" (ch4 stop)\n");
                //chan4_active = false;
            }
            //APRINTF("apu: S4 start: %d counter/continuous: %d\n", c.val>>7, (c.val&0x40)>>6);
            break;

        //audio control
        case 0xff24: //V-in output levels+channels
            APRINTF(" (output levels)\n");
            levels.val = c.val;
            //APRINTF("apu: Vin->SO2: %d, S02 level: %d, Vin->SO1: %d, S01 level: %d\n", c.val>>7, (c.val&0x70)>>4, (c.val&0x08)>>3, (c.val&0x07));
            break;
        case 0xff25: //Left+right channel selections
            APRINTF(" (output mappings)\n");
            output_map.val = c.val;
            //APRINTF("apu: S1->SO2: %d S2->S02: %d S3->S02: %d S4->S02: %d S1->SO1: %d S2->S01: %d S3->S01: %d S4->S01: %d\n", c.val>>7, (c.val&0x40)>>6, (c.val&0x20)>>5, (c.val&0x10)>>4,
                                                                                                                             //(c.val&0x0f)>>3, (c.val&0x04)>>2, (c.val&0x02)>>1, (c.val&0x01));
            break;
        case 0xff26: //Sound activation
            if(!(c.val & 0x80)) {
                APRINTF(" (sounds off) %02x\n", c.val);
                clear_regs();
                //writes_enabled = false;
            }
            else {
                APRINTF(" (sounds on)\n");
                //init();
                //writes_enabled = true;
            }
            status.val = c.val;
            //APRINTF("apu: master: %d S4-on: %d S3-on: %d S2-on: %d S1-on: %d\n", c.val>>7, (c.val&0x08)>>3, (c.val&0x04)>>2, (c.val&0x02)>>1, (c.val&0x01));
            break;

        default:
            if(c.addr >= 0xff30 && c.addr < 0xff40) {
                APRINTF( " (wave val %d)\n", (c.addr & 0x0f));
                wave_pattern[c.addr & 0x0f] = c.val;
                /*
                if(c.addr == 0xff3f) {
                    for(int i=0;i<16;i++) printf("%01x %01x ", (wave_pattern[i]>>4), (wave_pattern[i]&0xf));
                    printf("level index: %d, level shift: %d, DAC: %d", chan3_level.level_shift_index, wave_shift[chan3_level.level_shift_index], chan3_on.active);
                    printf("\n");
                }
                */
                //APRINTF("apu: wave[%d] = %d, wave[%d] = %d\n", (addr-0xff30)*2, (c.val&0xf0)>>4, (addr-0xff30)*2+1, c.val&0x0f);
                //waveform RAM
            }
            else {
                APRINTF("\n");
                //APRINTF("apu: unknown, addr=0x%04x, c.val=0x%02x\n", addr, c.val);
            }
    }
}
