#include<iostream>
#include<cstdint>
#include<string>
#include<fstream>
#include "memmap.h"
#include "cpu.h"
#include "lcd.h"
#include "util.h"

void usage(char ** argv);

int main(int argc, char *argv[]) {
    bool sgb = false;
    bool cgb = false;
    bool cpu_trace = false;
    bool headless = false;
    bool audio = true;
    std::string romfile = "";
    std::string fwfile  = "";
    Vect<std::string> args;
    args.resize(argc - 1, "");
    for(int arg = 1; arg < argc; ++arg) {
        args[arg-1] = std::string(argv[arg]);
    }
    for(size_t arg=0; arg < args.size(); ++arg) {
        if(args[arg] == "--sgb") {
            sgb = true;
            printf("Option: Set Super Gameboy mode, if possible.\n");
        }
        else if(args[arg] == "--cgb") {
            cgb = true;
            printf("Option: Set Color Gameboy mode, if possible.\n");
        }
        else if(args[arg] == "--trace") {
            cpu_trace = true;
            printf("Option: Activate CPU instruction trace\n");
        }
        else if(args[arg] == "--headless") {
            headless = true;
            printf("Option: Headless mode (graphics disabled)\n");
        }
        else if(args[arg] == "--nosound") {
            audio = false;
            printf("Option: NoSound mode (audio disabled)\n");
        }
        else if(args[arg] == "--fw") {
            if(arg + 1 < args.size()) {
                fwfile = args[arg+1];
                arg++;
                printf("Option: Firmware set to %s\n", fwfile.c_str());
            }
            else {
                printf("Option --fw requires an argument.\n");
                return 1;
            }
        }
        else if(romfile == "" && args[arg][0] != '-') {
                romfile = args[arg];
        }
    }

    if(romfile == "") {
        usage(argv);
        return 1;
    }

    std::cout<<"Passing in rom file name of "<<romfile<<" and fw file name of "<<fwfile<<std::endl;

    Uint32 sdl_init_flags = SDL_INIT_EVERYTHING|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER;
    if(headless) sdl_init_flags &= (~SDL_INIT_VIDEO);
    if(!audio)   sdl_init_flags &= (~SDL_INIT_AUDIO);
    if(SDL_Init(sdl_init_flags) < 0 ) {
        fprintf(stderr,"Couldn't initialize SDL: %s\n--nosound would disable audio, --headless would disable video. Those might be worth a try, depending on the above error message.\n", SDL_GetError());
    }

    memmap bus(romfile, fwfile);
    if(!bus.valid) {
        return 1;
    }

    if(sgb) {
        bool active = bus.set_sgb(true);
        if(active) {
            printf("Activating Super GameBoy mode\n");
        }
        else {
            printf("Cartridge reports that SGB mode is not supported. Disabling.\n");
        }
    }
    else {
        bus.set_sgb(false);
        printf("Disabling Super GameBoy mode (--sgb to activate it)\n");
    }

    if(cgb) {
        bus.set_color();
        //fprintf(stderr, "Sorry to get your hopes up, color is completely unsupported at this time.\n");
    }
    else {
        if(bus.needs_color()) {
            cgb = true;
            bus.set_color();
        }
    }

    cpu    proc(&bus, cgb, bus.has_firmware());
    lcd *  ppu = bus.get_lcd();
    apu *  sound = bus.get_apu();

    if(cpu_trace) {
        proc.toggle_trace();
        ppu->toggle_trace();
    }

    uint64_t cycle = 144*114; //Cycles since the machine started running (the system starts in vblank)
    uint64_t tick_size = 70224/16; //Cycles to run in a batch
    bool continue_running = true;
    uint64_t last_output_cycle = 0; //Last cycle that the ppu output a frame
    //uint64_t last_output_tick = 0;  //Last millisecond that the ppu output a frame
    while(continue_running) {
        uint64_t start = SDL_GetTicks();
        continue_running = util::process_events(&proc, &bus);
        uint64_t count = proc.run(cycle + tick_size);
        if(!count) {
            continue_running = false;
            SDL_Quit();
            break;
        }

        //cur_output_cycle represents which cycle the ppu decided to render a frame at
        uint64_t cur_output_cycle = 0;
        if(!headless && continue_running) {
            cur_output_cycle = ppu->run(cycle + tick_size);
            //printf("Main got report of output at: %lld\n", cur_output_cycle);
        }
        if(audio && continue_running) {
            sound->run(cycle + tick_size); //TODO: Add audio support
        }

        //Frame delay
        if(cur_output_cycle != 0) {
            uint64_t now = SDL_GetTicks();
            //uint64_t ms_diff = now - last_output_tick;
            uint64_t cycle_diff = cur_output_cycle - last_output_cycle; //cycles since last frame was output
            //printf("cycle diff: %ld\n", cycle_diff);

            if(cycle_diff != 0) {
                uint64_t delay = (double(cycle_diff*1000) / double(1024*1024));
                delay = delay - (now - start);
                if(delay < 1000) {
                    //printf("Delaying for %ld ms\n", delay/2);
                    SDL_Delay(delay/2);
                }
                uint64_t actual_delay = SDL_GetTicks() - now;
                if(actual_delay > delay) {
                    printf("Delayed for %ld ms too long\n", actual_delay - delay);
                }
            }
            //last_output_tick = now;
            last_output_cycle = cur_output_cycle;
            cur_output_cycle = 0;
        }
        cycle += tick_size;
    }
    return 0;
}

void usage(char ** argv) {
    printf("Usage: %s [options] romfile\n\n", argv[0]);
    printf("Options:\n");
    printf("\t--sgb: Set Super Gameboy mode, if possible.\n");
    printf("\t--cgb: Set Color Gameboy mode, if possible.\n");
    printf("\t--trace: Activate CPU instruction trace\n");
    printf("\t--headless: Headless mode (graphics disabled)\n");
    printf("\t--nosound: NoSound mode (audio disabled)\n");
    printf("\t--fw fw_filename: Boot using specified bootstrap firmware (expect 256 byte files for SGB and DMG, and 2,304 byte file for CGB)\n");
}
