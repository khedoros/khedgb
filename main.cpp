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
    memmap bus(romfile, fwfile);
    cpu    proc(&bus,bus.has_firmware());

    if( SDL_Init(SDL_INIT_EVERYTHING|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER|SDL_INIT_NOPARACHUTE) < 0 ) {
        if( SDL_Init(SDL_INIT_NOPARACHUTE|SDL_INIT_AUDIO) < 0) {
            fprintf(stderr,"Couldn't initialize SDL: %s\n", SDL_GetError());
            exit(1);
        }
        //headless = true;
        std::cout<<"Running in headless mode"<<std::endl;
    }

    int count = 0;
    while(count = proc.run()) {
        std::cout<<"Frame: "<<proc.frame<<std::endl;
        util::process_events(&bus);
        //if(proc.frame == 2000) break;
        //bus.render(proc.frame);
        //bus.dump_tiles();


        /* FRAME DELAY CODE to activate later
        //Calculates the time that the frame should end at, and delays until that time
        //cout<<"Timing two"<<endl;
        int frames = ppui.get_frame();
        int now = SDL_GetTicks();
        int delay = pstart + int(double(frames) * double(1000) / double(60)) - now;
        //cout<<"Frame: "<<frames<<" now: "<<now<<" delay: "<<delay<<endl;       
        if(delay > 0) {
            SDL_Delay(delay);
        }
        */

    }
    return 0;
}
