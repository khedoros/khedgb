#include<iostream>
#include<cstdint>
#include<string>
#include<fstream>
#include "memmap.h"
#include "cpu.h"
#include "lcd.h"
#include "util.h"

int main(int argc, char *argv[]) {
    if(argc < 2) {
        return 1;
    }
    const std::string romfile((argc>1)?argv[1]:"no_rom_file");
    const std::string fwfile((argc>2)?argv[2]:"");
    std::cout<<"Passing in rom file name of "<<romfile<<" and fw file name of "<<fwfile<<std::endl;

    if( SDL_Init(SDL_INIT_EVERYTHING|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER|SDL_INIT_NOPARACHUTE) < 0 ) {
        if( SDL_Init(SDL_INIT_NOPARACHUTE|SDL_INIT_AUDIO) < 0) {
            fprintf(stderr,"Couldn't initialize SDL: %s\n", SDL_GetError());
            exit(1);
        }
        //headless = true;
        std::cout<<"Running in headless mode"<<std::endl;
    }

    memmap bus(romfile, fwfile);
    cpu    proc(&bus,bus.has_firmware());
    lcd *  ppu = bus.get_lcd();

    uint64_t cycle = 0;
    uint64_t count = 0;
    uint64_t tick_size = 10000;
    bool continue_running = true;
    uint64_t last_output_cycle = 0;
    uint64_t last_output_tick = 0;
    while(continue_running) {
        continue_running = util::process_events(&bus);
        count = proc.run(cycle + tick_size);

        //cur_output_cycle represents which cycle the ppu decided to render a frame at
        uint64_t cur_output_cycle = ppu->run(cycle + tick_size);
        //apu->run(cycle + tick_size); TODO: Add audio support

        //Frame delay
        if(cur_output_cycle != 0) {
            uint64_t now = SDL_GetTicks();
            uint64_t ms_diff = now - last_output_tick;
            uint64_t cycle_diff = cur_output_cycle - last_output_cycle; //cycles since last frame was output

            if(cycle_diff != 0) {
                uint64_t delay = (double(cycle_diff*1000) / double(1024*1024));
                if(delay < 1000) {
                    SDL_Delay(delay);
                }
                uint64_t actual_delay = SDL_GetTicks() - now;
                if(actual_delay > delay) {
                    printf("Delayed for %d ms too long\n", actual_delay - delay);
                }
            }
            last_output_tick = now;
            last_output_cycle = cur_output_cycle;
        }
        cycle += tick_size;
    }
    return 0;
}
