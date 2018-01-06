#include "lcd.h"
#include<iostream>
#include<cstring>
#include<cassert>
#include<fstream>
#include<string>

lcd::lcd() : vram(0x2000) {
}

void lcd::write(int addr, void * val, int size, int cycle) {
    assert(size==1);
    if(addr >= 0x8000 && addr < 0xa000) {
        memcpy(&(vram[addr-0x8000]), val, size);
    }
    else {
        switch(addr) {
            case 0xff40:
                control = *((uint8_t *)val);
                break;
            case 0xff41:
                status = (*((uint8_t *)val)) & 0xf8;
                break;
            case 0xff42:
                bg_scroll_y = *((uint8_t *)val);
                break;
            case 0xff43:
                bg_scroll_x = *((uint8_t *)val);
                break;
            case 0xff47:
                bgpal.pal = *((uint8_t *)val);
                break;
            case 0xff48:
                obj1pal.pal = *((uint8_t *)val);
                break;
            case 0xff49:
                obj2pal.pal = *((uint8_t *)val);
                break;
            default:
                std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
    }
    return;
}

void lcd::read(int addr, void * val, int size, int cycle) {
    assert(size==1);
    if(addr >= 0x8000 && addr < 0xa000) {
        memcpy(val, &(vram[addr-0x8000]), size);
    }
    else {
        switch(addr) {
            case 0xff42:
                *((uint8_t *)val) = bg_scroll_y;
                break;
            default:
                std::cout<<"Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
        }
    }
    return;
}

void lcd::render(int frame) {
    bool zero=true;
    for(int i=0;i<0x2000;i++) {
        if(vram[i]) zero = false;
    }
    if(zero) return;
    std::ofstream vid((std::to_string(frame)+".pgm").c_str());
    vid<<"P5\n256 256\n3\n";
    uint8_t buffer[256][256];
    for(int xtile=0;xtile<32;xtile++) {
        for(int ytile=0;ytile<32;ytile++) {
            int tilenum = vram[0x1800+ytile*32+xtile];
            int base = tilenum*16;
            for(int yp=0;yp<8;yp++) {
                int b1=vram[base+yp*2];
                int shift=128;
                for(int xp=0;xp<8;xp++) {
                    int c=(b1 & shift)/shift;
                    assert(c==0||c==1||c==2||c==3);
                    buffer[ytile*8+yp][xtile*8+xp]=c;
                    shift/=2;
                }
            }
        }
    }
    for(int i=0;i<256;i++) {
        vid.write(reinterpret_cast<char *>(&buffer[i][0]),256);
    }
    vid.close();
    return;
}
