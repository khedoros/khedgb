#include "lcd.h"
#include<iostream>
#include<cstring>
#include<cassert>
#include<fstream>
#include<string>

lcd::lcd() : lyc(0), status(0), bg_scroll_x(0), bg_scroll_y(0), lyc_last_frame(0), m1_last_frame(0), m2_last_line(0), m2_last_frame(0), m0_last_line(0), m0_last_frame(0), active_cycle(0) {
    control.val = 0x91;
    vram.resize(0x2000);
    oam.resize(0xa0);

    /* Initialize the SDL library */
    screen = SDL_CreateWindow("KhedGB",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            320, 288,
            SDL_WINDOW_RESIZABLE);
    if ( screen == NULL ) {
        fprintf(stderr, "Couldn't set 320x240x8 video mode: %s\nStarting without video output.\n",
                SDL_GetError());
        //exit(1);
        return;
    }

    SDL_SetWindowMinimumSize(screen, 320, 288);
    cur_x_res=160;
    cur_y_res=144;

    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED/*|SDL_RENDERER_PRESENTVSYNC*/);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE|SDL_RENDERER_PRESENTVSYNC);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE/*|SDL_RENDERER_PRESENTVSYNC*/);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,160,144);

    assert(renderer && texture);

    buffer = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    overlay = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    lps = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    hps = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    win = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    bg = SDL_CreateRGBSurface(0,512,512,8,0,0,0,0);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

}

void lcd::write(int addr, void * val, int size, int cycle) {
    assert(size==1||(addr==0xff46&&size==0xa0));
    if(size > 1) return;
    if(addr >= 0x8000 && addr < 0xa000) {
        memcpy(&(vram[addr-0x8000]), val, size);
    }
    else {
        switch(addr) {
            case 0xff40:
                control.val = *((uint8_t *)val);
                std::cout<<"PPU: CTRL change"<<
                    " Priority: "<<control.priority<<
                    " sprites on : "<<control.sprite_enable<<
                    " sprite size: "<<control.sprite_size<<
                    " bg map: "<<control.bg_map<<
                    " tile addr mode: "<<control.tile_addr_mode<<
                    " window enable: "<<control.window_enable<<
                    " window map: "<<control.window_map<<
                    " display on: "<<control.display_enable<<std::endl;
                break;
            case 0xff41:
                status = (*((uint8_t *)val)) & 0xf8;
                printf("LCD status set to %02X\n", status);
                break;
            case 0xff42:
                bg_scroll_y = *((uint8_t *)val);
                break;
            case 0xff43:
                bg_scroll_x = *((uint8_t *)val);
                break;
            case 0xff45:
                lyc = *((uint8_t *)val);
                break;
            case 0xff46://OAM DMA
                memcpy(&oam[0],val,0xa0);
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
            case 0xff4a:
                win_scroll_y = *((uint8_t *)val);
                break;
            case 0xff4b:
                win_scroll_x = *((uint8_t *)val);
                break;
            default:
                std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
    }
    return;
}

void lcd::read(int addr, void * val, int size, int cycle) {
    if(size > 1) return;
    //assert(size==1);
    if(addr >= 0x8000 && addr < 0xa000) {
        memcpy(val, &(vram[addr-0x8000]), size);
    }
    else {
        switch(addr) {
            case 0xff41: {
                int line = cycle / 114;
                int mode = 0; //hblank
                if(line > 143) {
                    mode = 1; //vblank
                }
                else {
                    //oam around 20
                    //transfer around 43
                    //h-blank around 51
                    int line_cycle = cycle % 114;
                    if(line_cycle < 20) mode = 2; //OAM access
                    else if (line < 20+43) mode = 3; //LCD transfer
                }
                if(lyc == line) {
                    mode += 4;
                }
                *((uint8_t *)val) = mode | status | 0x80;
                }
                break;
            case 0xff42:
                *((uint8_t *)val) = bg_scroll_y;
                break;
            case 0xff43:
                *((uint8_t *)val) = bg_scroll_x;
                break;
            case 0xff4a:
                *((uint8_t *)val) = win_scroll_y;
                break;
            case 0xff4b:
                *((uint8_t *)val) = win_scroll_x;
                break;
            default:
                std::cout<<"Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
        }
    }
    return;
}

void lcd::render_background(int frame) {
    bool zero=true;
    for(int i=0;i<0x1800;i++) {
        if(vram[i]) zero = false;
    }
    if(!zero) { std::cout<<"Some tiles defined. ";}
    else return;
    zero = true;
    for(int i=0x1800;i<0x2000;i++) {
        if(vram[i]) zero = false;
    }
    if(!zero) {std::cout<<"Some BG map defined. "<<std::endl;}

    std::cout<<"PPU: Priority: "<<control.priority<<
                    " sprites on : "<<control.sprite_enable<<
                    " sprite size: "<<control.sprite_size<<
                    " bg map: "<<control.bg_map<<
                    " tile addr mode: "<<control.tile_addr_mode<<
                    " window enable: "<<control.window_enable<<
                    " window map: "<<control.window_map<<
                    " display on: "<<control.display_enable<<std::endl;
    std::ofstream vid((std::to_string(frame)+".pgm").c_str());
    //std::ofstream vid("frame.pgm");
    vid<<"P5\n256 256\n3\n";
    uint8_t buffer[256][256];
    uint32_t bgbase = 0x1800;
    if(control.bg_map) bgbase = 0x1c00;
    for(int xtile=0;xtile<32;xtile++) {
        for(int ytile=0;ytile<32;ytile++) {
            int tilenum = vram[bgbase+ytile*32+xtile];
            if(control.tile_addr_mode) {
                tilenum = 256 + int8_t(tilenum);
            }
            int base = tilenum*16;
            for(int yp=0;yp<8;yp++) {
                int b1=vram[base+yp*2];
                int b2=vram[base+yp*2+1];
                int shift=128;
                for(int xp=0;xp<8;xp++) {
                    int c=(b1 & shift)/shift + 2*((b2&shift)/shift);
                    assert(c==0||c==1||c==2||c==3);
                    buffer[ytile*8+yp][xtile*8+xp]=3-c;
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

void lcd::render(int frame) {
    std::cout<<"PPU: Priority: "<<control.priority<<
                    " sprites on : "<<control.sprite_enable<<
                    " sprite size: "<<control.sprite_size<<
                    " bg map: "<<control.bg_map<<
                    " tile addr mode: "<<control.tile_addr_mode<<
                    " window enable: "<<control.window_enable<<
                    " window map: "<<control.window_map<<
                    " display on: "<<control.display_enable<<std::endl;
    std::ofstream vid((std::to_string(frame)+".pgm").c_str());
    //std::ofstream vid("frame.pgm");
    vid<<"P5\n160 144\n3\n";
    uint8_t buffer[144][160];
    uint32_t bgbase = 0x1800;
    if(control.bg_map) bgbase = 0x1c00;
    for(int xtile=0;xtile<32;xtile++) {
        for(int ytile=0;ytile<32;ytile++) {
            int tilenum = vram[bgbase+ytile*32+xtile];
            if(control.tile_addr_mode) {
                tilenum = 256 + int8_t(tilenum);
            }
            int base = tilenum*16;
            for(int yp=0;yp<8;yp++) {
                int b1=vram[base+yp*2];
                int b2=vram[base+yp*2+1];
                int shift=128;
                for(int xp=0;xp<8;xp++) {
                    int xpix=xtile*8+xp; int ypix=ytile*8+yp;
                    if((xpix < bg_scroll_x + 160 || xpix + 96 < bg_scroll_x) && (ypix < bg_scroll_y + 144 || ypix + 112 < bg_scroll_y)) {
                        int c=(b1 & shift)/shift + 2*((b2&shift)/shift);
                        assert(c==0||c==1||c==2||c==3);
                        buffer[(ytile*8+yp)%144][(xtile*8+xp)%160]=3-c;
                    }
                    shift/=2;
                }
            }
        }
    }
    for(int i=0;i<144;i++) {
        vid.write(reinterpret_cast<char *>(&buffer[i][0]),160);
    }
    vid.close();
    return;
}

bool lcd::interrupt_triggered(uint32_t frame, uint32_t cycle) {
    int line = cycle / 114;

    //Line co-incidence
    //Trigger interrupt if it's activated, the line matches, and either it hasn't been triggered this frame, or it was moved up to a later line
    if(((LYC & status) != 0) && (lyc == line) && (frame > lyc_last_frame || line > lyc_last_line)) {
        lyc_last_frame = frame; //Don't trigger twice in one frame
        lyc_last_line = line;   //But go ahead and do it, if it's on a later line
        return true;
    }

    //V-Blank
    //Trigger interrupt if it's activated, the GB is in vsync, and it hasn't been triggered this frame.
    if((M1 & status) != 0 && line >= 144 && frame > m1_last_frame) {
        m1_last_frame = frame;
        return true;
    }

    int line_cycle = cycle % 114;

    //Line start, reading from OAM
    //Trigger if before vsync, in cycle 0-19 
    if((M2 & status) != 0 && line < 144 && line_cycle < 20 && (m2_last_frame < frame || m2_last_line < line)) {
        m2_last_line = line;
        m2_last_frame = frame;
        return true;
    }

    //H-Blank
    if((M0 & status) != 0 && line < 144 && line_cycle >= (20+43) && (m0_last_frame < frame || m0_last_line < line)) {
        m0_last_line = line;
        return true;
        //printf("Warning: M0 interrupt set, but not implemented\n");
    }

    return false;
}

void lcd::dump_tiles() {
    std::ofstream vid(std::string("frame-dead.pgm").c_str());
    vid<<"P5\n192 128\n3\n";
    uint8_t buffer[128][192];
    for(int ytile=0;ytile<16;ytile++) {
        for(int xtile=0;xtile<24;xtile++) {
            int base = (xtile *16 + ytile)*16;
            for(int yp=0;yp<8;yp++) {
                int b1=vram[base+yp*2];
                int b2=vram[base+yp*2+1];
                int shift=128;
                for(int xp=0;xp<8;xp++) {
                    int c=(b1 & shift)/shift + 2*((b2 & shift)/shift);
                    assert(c==0||c==1||c==2||c==3);
                    buffer[ytile*8+yp][xtile*8+xp]=3-c;
                    shift/=2;
                }
            }
        }
    }
    for(int i=0;i<128;i++) {
        vid.write(reinterpret_cast<char *>(&buffer[i][0]),192);
    }
    vid.close();
}
