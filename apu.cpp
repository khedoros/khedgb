#include<SDL2/SDL.h>
#include "apu.h"
#include<cstdio>
#include<cassert>

#ifdef DEBUG
#define APRINTF printf
#define ASSERT assert
#else
#define APRINTF //printf
#define ASSERT //assert
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
    APRINTF("Clear registers!\n");
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

apu::apu() : writes_enabled(false), cycle(0), devid(0), audio_open(false), debug(false), screen(NULL), renderer(NULL), buffer(NULL), texture(NULL)
{
    ASSERT(sizeof(sweep_reg) == 1);
    ASSERT(sizeof(wave_on_reg) == 1);
    ASSERT(sizeof(pat_len_reg) == 1);
    ASSERT(sizeof(wave_length) == 1);
    ASSERT(sizeof(wave_level) == 1);
    ASSERT(sizeof(freq_reg) == 2);
    ASSERT(sizeof(noise_freq) == 1);
    ASSERT(sizeof(noise_count_init) == 1);
    ASSERT(sizeof(output_levels) == 1);
    ASSERT(sizeof(channel_map) == 1);
    ASSERT(sizeof(channel_status) == 1);
    clear();
    clear_regs();
    init();
    int num_drivers = SDL_GetNumAudioDrivers();
    unsigned int chosen_driver = 255;
    unsigned int driver_desirability = 255;
    std::array<std::array<const char, 20>, 6> desired_drivers = {"directsound", "winmm", "pulse", "pulseaudio", "alsa", "dummy"};
    printf("Audio Drivers: \n");
    for(int i=0; i<num_drivers;i++) {
        printf("\tDriver %d: \"%s\"\n", i, SDL_GetAudioDriver(i));
        for(unsigned int j=0;j<desired_drivers.size();j++) {
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

	SDL_AudioSpec want;
	want.freq=44100;
	want.format=AUDIO_S8;
	want.channels=CHANNELS;
	want.silence=0;
	want.samples=/*8192*/ 690;
	want.size=0;
	want.callback=NULL;
	want.userdata=NULL;

	SDL_AudioSpec got;
	devid = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
	if(!devid) {
		fprintf(stderr, "Error opening audio device: %s\n", SDL_GetError());
	}
	else {
		printf("Freq: %d format: %d (%d) ch: %d sil: %d samp: %d size: %d\n", got.freq, got.format, want.format, got.channels, got.silence, got.samples, got.size);
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

    for(int i=0;i<8;i++) {
        wave_pattern[i*2] = 0;
        wave_pattern[i*2+1]=0xff;
    }
    //out_wav.open("audio.out");
    //out_ch3.open("ch3_audio.out");
    //debug_window(true);
}

apu::~apu() {
    //out_wav.close();
    //out_ch3.close();
}

void apu::write(uint16_t addr, uint8_t val, uint64_t cycle) {
    //printf("addr: %04x val: %02x ", addr, val);
    if(writes_enabled || addr >= 0xff30) {
        written_values[addr - 0xff10] = val;
        cmd_queue.emplace(util::cmd{cycle, addr, val});
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
        cmd_queue.emplace(util::cmd{cycle, addr, val});
    }
}

uint8_t apu::read(uint16_t addr, uint64_t cycle) {
    //printf("Read %04x\n", addr);
    if(addr == 0xff26) {
        //Not super accurate, timing-wise, but should be within 1/64th of a second of correct.
        return 0;//((chan4_active<<3)|(chan3_active<<2)|(chan2_active<<1)|chan1_active);
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
    int sample_count = 690;
    uint64_t start_cycle = cycle;
    int32_t left = 0;
    int32_t right = 0;
    apu::samples s;
    int32_t accum = 0;

    for(;cycle < run_to; cycle+=4) {
        //Clock the frame sequencer at 512Hz
        if(((cycle%2048)&0x7fc) == 0) {
            clock_sequencer();
        }

                //If it's time, apply the current command, and grab the next one, if it exists
        while(!cmd_queue.empty() && cycle >= cur_cmd.cycle) {
            apply(cur_cmd);
            cmd_queue.pop();
            if(!cmd_queue.empty()) {
                cur_cmd = cmd_queue.front();
            }
        }

        if((cycle&3)==0) {
            clock_freqs(4);
            //Clock the frequency counters for each channel
            render(s);
            left+=s.l;
            right+=s.r;
            accum++;
        }

        //Figure out if it's time to generate a new sample, and put it into the output buffer if it is
        int sample = int(float(sample_count) * (float(cycle - start_cycle) / float(run_to - start_cycle)));
        if(sample > cur_sample) {
            //printf("L: %d R: %d acc: %d\n", left, right, sample_accum);
            out_buffer[cur_sample*2] = left/accum;
            out_buffer[cur_sample*2+1] = right/accum;
            if(debug) {
                draw_sample(cur_sample, s, accum);
            }
            cur_sample++;
            left = 0;
            right = 0;
            accum = 0;
            s.ch1 = 0;
            s.ch2 = 0;
            s.ch3 = 0;
            s.ch4 = 0;
            //printf("sample %d accum %d\n", cur_sample, accum);
        }
    }

    //Push the sample to the output device.
    if(audio_open && SDL_GetQueuedAudioSize(devid) < out_buffer.size() * 4) {
        //printf("%d ", SDL_GetQueuedAudioSize(devid));
        //out_wav.write(reinterpret_cast<const char *>(out_buffer.data()), sample_count * CHANNELS * SAMPLE_SIZE);
        SDL_QueueAudio(devid, out_buffer.data(), sample_count * CHANNELS * SAMPLE_SIZE);
        if(debug) {
            display_surface();
        }
            
    }
}

void apu::display_surface() {
    if(texture && buffer && renderer) {
        if(texture) {
            SDL_DestroyTexture(texture);
            texture = NULL;
        }
        texture = SDL_CreateTextureFromSurface(renderer, buffer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_FillRect(buffer, NULL, SDL_MapRGB(buffer->format, 100,100,100));
        SDL_Rect r = {0,128,690,1};
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 0,0,0));
        r = {0,256,690,1};
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 0,0,0));
        r = {0,384,690,1};
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 0,0,0));

        r = {0,64,690,1};
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 75,75,75));
        r = {0,128+64,690,1};
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 75,75,75));
        r = {0,256+64,690,1};
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 75,75,75));
        r = {0,384+64,690,1};
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 75,75,75));
    }
}

void apu::draw_sample(int cur_sample, samples& s, int accum) {
    if(buffer) {
        SDL_Rect r = {cur_sample,0,1,0};
        //SDL_Rect r2 = {0,0,10,10};
        s.ch1/=accum; s.ch2/=accum; s.ch3/=accum; s.ch4/=accum;
        r.h = abs(2*s.ch1);
        r.y = 64;
        if(s.ch1 > 0) r.y -= r.h;
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 255,255,0));
        if(chan1_active) {
            //SDL_FillRect(buffer, &r2, SDL_MapRGB(buffer->format, 0,255,0));
        }

        r.h = abs(2*s.ch2);
        r.y = 192;
        if(s.ch2 > 0) r.y -= r.h;
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 255,0,255));
        if(chan2_active) {
            //r2.y = 128;
            //SDL_FillRect(buffer, &r2, SDL_MapRGB(buffer->format, 255,0,0));
        }
        r.h = abs(2*s.ch3);
        r.y = 320;
        if(s.ch3 > 0) r.y -= r.h;
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 0,255,255));
        if(chan3_active) {
            //r2.y = 256;
            //SDL_FillRect(buffer, &r2, SDL_MapRGB(buffer->format, 0,0,255));
        }
        r.h = abs(2*s.ch4);
        r.y = 448;
        if(s.ch4 > 0) r.y -= r.h;
        SDL_FillRect(buffer, &r, SDL_MapRGB(buffer->format, 255,255,255));
        if(chan4_active) {
            //r2.y = 384;
            //SDL_FillRect(buffer, &r2, SDL_MapRGB(buffer->format, 0,0,0));
        }
    }
}

void apu::render(apu::samples& s) {
    int32_t chan1 = chan1_active * chan1_env.volume * square_wave[chan1_patlen.duty_cycle][chan1_duty_phase];
    ASSERT(chan1 > -129 && chan1 < 128);
    //printf("ch1: %02x ", chan1&0xff);

    int32_t chan2 = chan2_active * chan2_env.volume * square_wave[chan2_patlen.duty_cycle][chan2_duty_phase];
    ASSERT(chan2 > -129 && chan2 < 128);
    //printf("ch2: %02x ", chan2&0xff);

    int32_t chan3 = chan3_active * chan3_dac * ((chan3_cur_sample - 8) / (1<<(wave_shift[chan3_level.level_shift_index])));
    ASSERT(chan3 > -129 && chan3 < 128);
    //printf("ch3: sample: %x shift: %x offset: %x result: %d \n", chan3_cur_sample, wave_shift[chan3_level.level_shift_index], 8>>(wave_shift[chan3_level.level_shift_index]), chan3/4);

    int32_t chan4 = chan4_active * chan4_env.volume * lfsr_value;
    ASSERT(chan4 > -129 && chan4 < 128);
    ASSERT(chan4 > -129 && chan4 < 128);
    //printf("ch4: %02x \n", chan4&0xff);

    s.ch1 += chan1;
    s.ch2 += chan2;
    s.ch3 += chan3;
    s.ch4 += chan4;

    s.l = output_map.ch1_to_so1 * chan1;
    s.l += output_map.ch2_to_so1 * chan2;
    s.l += output_map.ch3_to_so1 * chan3;
    s.l += output_map.ch4_to_so1 * chan4;
    s.r = output_map.ch1_to_so2 * chan1;
    s.r += output_map.ch2_to_so2 * chan2;
    s.r += output_map.ch3_to_so2 * chan3;
    s.r += output_map.ch4_to_so2 * chan4;

    //printf("chan1 (l: %d v: %d) chan2 (l: %d v: %d) templ: %d tempr: %d\n", chan1_level, chan1, chan2_level, chan2, templ, tempr);

    //printf("sl: %d sr: %d\n", s.l, s.r);
    //printf("APU Render cycle %ld to %ld\n", cycle, render_to);
    return;
}

bool apu::sweep_overflow() {
    int16_t freq = chan1_freq_shadow;
    int16_t delta = (freq>>chan1_sweep.shift);
    if(!chan1_sweep.direction) delta *=-1;
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
            if(chan1_length_counter == 0) {
                chan1_active = false;
                APRINTF("ch1 length expired\n");
            }
        }

        if(chan2_active && chan2_freq.length_enable) {
            chan2_length_counter--;
            if(chan2_length_counter == 0) {
                chan2_active = false;
                APRINTF("ch2 length expired\n");
            }
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
                int16_t delta = (chan1_freq.freq>>chan1_sweep.shift);
                if(!chan1_sweep.direction) delta *=-1;
                //printf("sweep from %d to ", freq);
                freq+=delta;
                //printf("%d due to delta %d\n", freq, delta);
                if(freq < 0) {
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
        if(chan1_active && chan1_env.volume > 0 && chan1_env.period) {
            chan1_env_counter--;
            if(!chan1_env_counter) {
                if(chan1_env.direction && chan1_env.volume < 15) {
                    chan1_env.volume++;
                }
                else if(!chan1_env.direction && chan1_env.volume > 0) {
                    chan1_env.volume--;
                }
                else if(!chan1_env.direction && !chan1_env.volume) {
                    chan1_active = false;
                    APRINTF("ch1 env at zero\n");
                }
                chan1_env_counter = chan1_env.period;
                if(!chan1_env_counter) chan1_env_counter = 8;
                if(!chan1_env.volume) {
                    chan1_active = false;
                    APRINTF("ch1 env to zero\n");
                }
            }
        }
        if(chan2_active && chan2_env.volume > 0 && chan2_env.period) {
            chan2_env_counter--;
            if(!chan2_env_counter) {
                if(chan2_env.direction && chan2_env.volume < 15) {
                    chan2_env.volume++;
                }
                else if(!chan2_env.direction && chan2_env.volume > 0) {
                    chan2_env.volume--;
                }
                else if(!chan2_env.direction && !chan2_env.volume) {
                    chan2_active = false;
                    APRINTF("ch2 env at zero\n");
                }
                chan2_env_counter = chan2_env.period;
                if(!chan2_env_counter) chan2_env_counter = 8;
                if(!chan2_env.volume) {
                    chan2_active = false;
                    APRINTF("ch2 env to zero\n");
                }
            }
        }
        if(chan4_active && chan4_env.volume > 0 && chan4_env.period) {
            chan4_env_counter--;
            if(!chan4_env_counter) {
                if(chan4_env.direction && chan4_env.volume < 15) {
                    chan4_env.volume++;
                }
                else if(!chan4_env.direction && chan4_env.volume > 0) {
                    chan4_env.volume--;
                }
                else if(!chan4_env.direction && !chan4_env.volume) {
                    chan4_active = false;
                }
                chan4_env_counter = chan4_env.period;
                if(!chan4_env_counter) chan4_env_counter = 8;
                if(!chan4_env.volume) chan4_active = false;
            }
        }
    }
}

void apu::clock_freqs(uint64_t c_s /* = 1 */) {
    if(chan1_active) {
        chan1_freq_counter -= c_s;
        if(chan1_freq_counter <= 0) {
            chan1_freq_counter += chan1_freq_shadow;
            chan1_duty_phase++;
            chan1_duty_phase %= square_wave_length;
        }
    }
    if(chan2_active) {
        chan2_freq_counter -= c_s;
        if(chan2_freq_counter <= 0) {
            chan2_freq_counter += (2048 - chan2_freq.freq);
            chan2_duty_phase++;
            chan2_duty_phase %= square_wave_length;
        }
    }
    if(chan3_active) {
        chan3_freq_counter -= (2 * c_s);
        if(chan3_freq_counter <= 0) {
            chan3_freq_counter += (2048 - chan3_freq.freq);
            chan3_duty_phase++;
            chan3_duty_phase %= wave_length;
            if((chan3_duty_phase & 1) == 1) {
                chan3_cur_sample = (wave_pattern[chan3_duty_phase/2] & 0x0f);
            }
            else {
                chan3_cur_sample = ((wave_pattern[chan3_duty_phase/2] & 0xf0)>>4);
            }
            //printf("duty: %d, sample: %d\n", chan3_duty_phase, chan3_cur_sample);
        }
    }
    if(chan4_active) {
        chan4_freq_counter -= c_s;
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
            if(chan1_env.volume == 0) {
                chan1_active = false;
                APRINTF("ch1 volume set to 0\n");
            }
            //APRINTF("apu: S1 default envelope: %d up/down: %d step length: %d\n", c.val>>4, (c.val&8)>>3, (c.val&7));
            break;
        case 0xff13: //sound 1 low-order frequency
            APRINTF(" (ch1 low-freq)\n");
            chan1_freq.freq_low = c.val;
            //APRINTF("apu: S1 freq-low: %d\n", c.val);
            break;
        case 0xff14: //sound 1 high-order frequency + start
            chan1_freq.freq_high = c.val;
            chan1_freq_shadow = 2048 - chan1_freq.freq;
            if(chan1_freq.initial) {
                APRINTF(" (ch1 trigger)\n");
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
            if(chan2_env.volume == 0) {
                chan2_active = false;
                APRINTF("ch2 volume set to 0\n");
            }
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
            if(chan4_env.volume == 0) chan4_active = false;
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

                /*
                if(c.addr == 0xff3f) {
                    char buffer[32];

                    for(int i=0;i<16;i++) {
                        buffer[i*2] = (wave_pattern[i]>>4);
                        buffer[i*2+1] = (wave_pattern[i]&0x0f);
                    }
                    out_ch3.write(buffer, 32);
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

void apu::debug_window(bool on) {
    if(on==debug) return;

    if(on) {
        buffer = SDL_CreateRGBSurface(0,690,512,32,0,0,0,0);
        util::reinit_sdl_screen(&screen, &renderer, &texture, 690/2, 512/2);
    }
    debug = on;
}

void apu::toggle_debug() {
    if(debug) {
        debug = false;
        if(screen) {
            SDL_DestroyWindow(screen);
            screen = NULL;
        }
        if(renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = NULL;
        }
        if(texture) {
            SDL_DestroyTexture(texture);
            texture = NULL;
        }
        if(buffer) {
            SDL_FreeSurface(buffer);
            buffer = NULL;
        }
    }
    else {
        debug = true;
        buffer = SDL_CreateRGBSurface(0,690,512,32,0,0,0,0);
        util::reinit_sdl_screen(&screen, &renderer, &texture, 690/2, 512/2);
    }
}
