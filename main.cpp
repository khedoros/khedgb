#include<iostream>
#include<cstdint>
#include<string>
#include<fstream>
#include "memmap.h"
#include "cpu.h"
#include "lcd.h"

int main(int argc, char *argv[]) {
    if(argc < 2) {
        return 1;
    }
    const std::string romfile((argc>1)?argv[1]:"no_rom_file");
    const std::string fwfile((argc>2)?argv[2]:"");
    std::cout<<"Passing in rom file name of "<<romfile<<" and fw file name of "<<fwfile<<std::endl;
    memmap bus(romfile, fwfile);
    cpu    proc(&bus,bus.has_firmware());
    int count = 0;
    while(count = proc.run()) {
        std::cout<<"Frame: "<<proc.frame<<std::endl;
        //bus.render(proc.frame);
    }
    return 0;
}
