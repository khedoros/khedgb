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
    const std::string infile(argv[1]);
    memmap bus(infile);
    cpu    proc(bus,true);
    int count = 0;
    while(count = proc.run()) {

    }
    return 0;
}
