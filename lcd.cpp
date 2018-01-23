#include "lcd.h"
#include<iostream>
#include<cstring>
#include<cassert>
#include<fstream>
#include<string>

lcd::lcd() : lyc(0), status(0), bg_scroll_x(0), bg_scroll_y(0), lyc_last_frame(0), m1_last_frame(0), m2_last_line(0), m2_last_frame(0), m0_last_line(0), m0_last_frame(0), active_cycle(0), screen(NULL), renderer(NULL), texture(NULL), buffer(NULL), overlay(NULL), lps(NULL), hps(NULL), win(NULL), bg(NULL) {
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
        fprintf(stderr, "lcd::Couldn't set 320x240x8 video mode: %s\nStarting without video output.\n",
                SDL_GetError());
        //exit(1);
        return;
    }

    SDL_SetWindowMinimumSize(screen, 320, 288);
    cur_x_res=160;
    cur_y_res=144;

    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED/*|SDL_RENDERER_PRESENTVSYNC*/);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE|SDL_RENDERER_PRESENTVSYNC);
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE/*|SDL_RENDERER_PRESENTVSYNC*/);
    if(!renderer) {
        fprintf(stderr, "lcd::Couldn't create a renderer: %s\nStarting without video output.\n",
                SDL_GetError());
        SDL_DestroyWindow(screen);
        screen = NULL;
        //exit(1);
        return;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,160,144);
    if(!texture) {
        fprintf(stderr, "lcd::Couldn't create a texture: %s\nStarting without video output.\n",
                SDL_GetError());
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
        SDL_DestroyWindow(screen);
        screen = NULL;
        //exit(1);
        return;
    }

    //assert(renderer && texture);

    buffer = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);
    overlay = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    lps = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    hps = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    win = SDL_CreateRGBSurface(0,160,144,8,0,0,0,0);
    bg = SDL_CreateRGBSurface(0,512,512,8,0,0,0,0);

    printf("lcd::Setting render draw color to black\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    printf("lcd::Clearing rendering output\n");
    SDL_RenderClear(renderer);
    printf("lcd::Pushing video update to renderer\n");
    SDL_RenderPresent(renderer);
    printf("lcd::constructor reached end\n");

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
                bgpal.pal[0] = *((uint8_t *)val) & 0x03;
                bgpal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                bgpal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                bgpal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                break;
            case 0xff48:
                obj1pal.pal[0] = *((uint8_t *)val) & 0x03;
                obj1pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                obj1pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                obj1pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                break;
            case 0xff49:
                obj2pal.pal[0] = *((uint8_t *)val) & 0x03;
                obj2pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                obj2pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                obj2pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
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

void lcd::render(int frame,bool output_file) {

    bool output_sdl = true;
    if(!screen||!texture||!renderer) {
        printf("PPU: problem!\n");
        output_sdl = false;
    }
    std::cout<<"PPU: Priority: "<<control.priority<<
                    " sprites on : "<<control.sprite_enable<<
                    " sprite size: "<<control.sprite_size<<
                    " bg map: "<<control.bg_map<<
                    " tile addr mode: "<<control.tile_addr_mode<<
                    " window enable: "<<control.window_enable<<
                    " window map: "<<control.window_map<<
                    " display on: "<<control.display_enable<<std::endl;
    std::ofstream vid;
    if(output_file) {
        vid.open((std::to_string(frame)+".pgm").c_str());
        vid<<"P5\n160 144\n3\n";
    }
    //std::ofstream vid("frame.pgm");
    uint8_t pgm_buffer[144][160];

    //Draw the background
    uint32_t bgbase = 0x1800;
    if(control.priority) { //controls whether the background displays, in regular DMG mode
        if(control.bg_map) bgbase = 0x1c00;
        for(int x_out_pix = 0; x_out_pix < 160; x_out_pix++) {
            int x_in_pix = (x_out_pix + bg_scroll_x) & 0xff;
            int x_tile = x_in_pix / 8;
            int x_tile_pix = x_in_pix % 8;
            for(int y_out_pix = 0; y_out_pix < 144; y_out_pix++) {
                int y_in_pix = (y_out_pix + bg_scroll_y) & 0xff;
                int y_tile = y_in_pix / 8;
                int y_tile_pix = y_in_pix % 8;
                int tile_num = vram[bgbase+y_tile*32+x_tile];
                if(control.tile_addr_mode) {
                    tile_num = 256 + int8_t(tile_num);
                }
                int base = tile_num * 16;
                int b1=vram[base+y_tile_pix*2];
                int b2=vram[base+y_tile_pix*2+1];
                int shift = 128>>(x_tile_pix);
                int c=3 - ((b1&shift)/shift + 2*((b2&shift)/shift));
                assert(c==0||c==1||c==2||c==3);

                pgm_buffer[y_out_pix][x_out_pix]=bgpal.pal[c];

                /*
                uint32_t color = SDL_MapRGB(this->buffer->format,85*bgpal.pal[c],85*bgpal.pal[c],85*bgpal.pal[c]);
                ((uint32_t *)this->buffer->pixels)[y_out_pix*(this->buffer->pitch)+x_out_pix] = color;

               */

                if(output_sdl) {
                    SDL_SetRenderDrawColor(renderer, 85*bgpal.pal[c], 85*bgpal.pal[c], 85*bgpal.pal[c], 255);
                    SDL_RenderDrawPoint(renderer, x_out_pix, y_out_pix);
                }
            }
        }

    }
    else if(output_sdl) {
        //clear the screen, because the background isn't going to be drawn
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }


    //Draw the window
    uint32_t winbase = 0x1800;
    if(control.window_map) winbase = 0x1c00;
    if(control.window_enable) {
        for(int tile_y = 0; tile_y + (win_scroll_y/8) < 18; tile_y++) {
            for(int tile_x = 0; tile_x + (win_scroll_x/8) < 20; tile_x++) {
                int tile_num = vram[winbase+tile_y*32+tile_x];
                if(control.tile_addr_mode) {
                    tile_num = 256+int8_t(tile_num);
                }
                int base = tile_num * 16;
                for(int y_tile_pix = 0; y_tile_pix < 8 && y_tile_pix + win_scroll_y + tile_y * 8 < 144; y_tile_pix++) {
                    int b1=vram[base+y_tile_pix*2];
                    int b2=vram[base+y_tile_pix*2+1];
                    for(int x_tile_pix = 0; x_tile_pix < 8 && x_tile_pix + win_scroll_x + tile_x * 8 < 144; x_tile_pix++) {
                        int shift = 128>>(x_tile_pix);
                        int c=3 - ((b1&shift)/shift + 2*((b2&shift)/shift));
                        assert(c==0||c==1||c==2||c==3);

                        pgm_buffer[tile_y*8+y_tile_pix][tile_x*8+x_tile_pix]=bgpal.pal[c];

                        /*
                        uint32_t color = SDL_MapRGB(this->buffer->format,85*bgpal.pal[c],85*bgpal.pal[c],85*bgpal.pal[c]);
                        ((uint32_t *)this->buffer->pixels)[y_out_pix*(this->buffer->pitch)+x_out_pix] = color;

                        */

                        if(output_sdl) {
                            SDL_SetRenderDrawColor(renderer, 85*bgpal.pal[c], 85*bgpal.pal[c], 85*bgpal.pal[c], 255);
                            SDL_RenderDrawPoint(renderer, tile_x*8+x_tile_pix, tile_y*8+y_tile_pix);
                        }
                    }
                }
            }
        }
    }
    
    //Draw the sprites
    if(control.sprite_enable) {

    }

    if(output_sdl) {
        /* Activate when I'm not using SDL_RenderDrawPoint anymore
        SDL_DestroyTexture(texture);

        texture = SDL_CreateTextureFromSurface(renderer, this->buffer);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer,texture,NULL,NULL);
        */

        SDL_RenderPresent(renderer);
    }
    if(output_file) {
        for(int i=0;i<144;i++) {
            vid.write(reinterpret_cast<char *>(&pgm_buffer[i][0]),160);
        }
        vid.close();
    }
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
