#include "lcd.h"
#include<iostream>
#include<cstring>
#include<cassert>
#include<fstream>
#include<string>

lcd::lcd() : cpu_lyc(0), cpu_status(0), bg_scroll_x(0), bg_scroll_y(0), cpu_bg_scroll_x(0), cpu_bg_scroll_y(0), lyc_next_cycle(0), m1_next_cycle(0), m2_next_cycle(0), active_cycle(0), cpu_active_cycle(0), screen(NULL), renderer(NULL), texture(NULL), buffer(NULL), overlay(NULL), lps(NULL), hps(NULL), win(NULL), bg(NULL) {
    control.val = 0x91;
    cpu_control.val = 0x91;
    vram.resize(0x2000);
    oam.resize(0xa0);
    cpu_vram.resize(0x2000);
    cpu_oam.resize(0xa0);

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

    //printf("lcd::Setting render draw color to black\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //printf("lcd::Clearing rendering output\n");
    SDL_RenderClear(renderer);
    //printf("lcd::Pushing video update to renderer\n");
    SDL_RenderPresent(renderer);
    //printf("lcd::constructor reached end\n");

}

uint64_t lcd::run(uint64_t cycle_count) {

    render(0,false);
    return 0; //0 means a frame hasn't been rendered during this timeslice
}

//Apply the data extracted from the command queue, and apply it to the PPU view of the state
void lcd::apply(int addr, uint8_t val, uint64_t index, uint64_t cycle) {
    if(addr >= 0x8000 && addr < 0xa000) {
        vram[addr-0x8000] = val;
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        oam[addr-0xfe00] = val;
    }
    else {
        switch(addr) {
            case 0xff40:
                {
                    control_reg old_val{.val = control.val};
                    control.val = val;
                    if(control.display_enable && !old_val.display_enable) {
                        active_cycle = cycle;
                    }
                }
                break;
            case 0xff42:
                bg_scroll_y = val;
                break;
            case 0xff43:
                bg_scroll_x = val;
                break;
            case 0xff46://OAM DMA
                memcpy(&oam[0],&cmd_data[index][0],0xa0);
                break;
            case 0xff47:
                bgpal.pal[0] = (val & 0x03);
                bgpal.pal[1] = (val & 0x0c)>>2;
                bgpal.pal[2] = (val & 0x30)>>4;
                bgpal.pal[3] = (val & 0xc0)>>6;
                break;
            case 0xff48:
                obj1pal.pal[0] = (val & 0x03);
                obj1pal.pal[1] = (val & 0x0c)>>2;
                obj1pal.pal[2] = (val & 0x30)>>4;
                obj1pal.pal[3] = (val & 0xc0)>>6;
                break;
            case 0xff49:
                obj2pal.pal[0] = (val & 0x03);
                obj2pal.pal[1] = (val & 0x0c)>>2;
                obj2pal.pal[2] = (val & 0x30)>>4;
                obj2pal.pal[3] = (val & 0xc0)>>6;
                break;
            case 0xff4a:
                win_scroll_y = val;
                break;
            case 0xff4b:
                win_scroll_x = val;
                break;
            default:
                //Various data aren't necessary to render the screen, so we ignore anything like interrupts and status
                break;
        }
    }
    return;
}

void lcd::write(int addr, void * val, int size, uint64_t cycle) {
    assert(size==1||(addr==0xff46&&size==0xa0));
    if(addr >= 0x8000 && addr < 0xa000) {
        if(get_mode(cycle) != 3) {
            memcpy(&(cpu_vram[addr-0x8000]), val, size);
            cmd_queue.push_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
        }
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        if(get_mode(cycle) < 2) {
            memcpy(&(cpu_oam[addr - 0xfe00]), val, size);
            cmd_queue.push_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
        }
    }
    else {
        switch(addr) {
            case 0xff40:
                {
                    control_reg old_val{.val = cpu_control.val};
                    cpu_control.val = *((uint8_t *)val);
                    if(cpu_control.display_enable && !old_val.display_enable) {
                        cpu_active_cycle = cycle;
                        update_estimates(cycle);
                    }
                    else if(old_val.display_enable != cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                std::cout<<"PPU: CTRL change"<<
                    " Priority: "<<cpu_control.priority<<
                    " sprites on : "<<cpu_control.sprite_enable<<
                    " sprite size: "<<cpu_control.sprite_size<<
                    " bg map: "<<cpu_control.bg_map<<
                    " tile addr mode: "<<cpu_control.tile_addr_mode<<
                    " window enable: "<<cpu_control.window_enable<<
                    " window map: "<<cpu_control.window_map<<
                    " display on: "<<cpu_control.display_enable<<std::endl;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff41: 
                {
                    uint8_t old_val = cpu_status; 
                    cpu_status = *((uint8_t *)val)&0xf8; 
                    if(cpu_status != old_val && cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                //Don't care about status while rendering
                //cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                printf("LCD status set to %02X\n", cpu_status);
                break;
            case 0xff42:
                cpu_bg_scroll_y = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff43:
                cpu_bg_scroll_x = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff45:
                {
                    uint8_t old_val = cpu_lyc;
                    cpu_lyc = *((uint8_t *)val);
                    if(cpu_lyc > 153) { //Out of range to actually trigger
                        lyc_next_cycle = -1;
                    }
                    else if(old_val != cpu_lyc && cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                //Don't care about lyc during rendering
                //cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff46://OAM DMA
                {
                    memcpy(&cpu_oam[0],val,0xa0);
                    uint64_t index = cmd_data.size();
                    cmd_data.resize(index + 1);
                    cmd_data[index].resize(0xa0);
                    for(int i=0;i<0xa0;i++) {
                        cmd_data[index][i] = cpu_oam[i];
                    }
                    cpu_dma_addr = *((uint8_t *)val);
                    cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), index});
                }
                break;
            case 0xff47:
                cpu_bgpal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_bgpal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_bgpal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_bgpal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff48:
                cpu_obj1pal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_obj1pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_obj1pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_obj1pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff49:
                cpu_obj2pal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_obj2pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_obj2pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_obj2pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff4a:
                cpu_win_scroll_y = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff4b:
                cpu_win_scroll_x = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            default:
                std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
    }
    return;
}

uint8_t lcd::get_mode(uint64_t cycle) {
    if(!cpu_control.display_enable) {
        return 1;
    }
    int frame_cycle = (cycle - cpu_active_cycle) % 17556;
    int line = frame_cycle / 114;

    int mode = 0; //hblank; largest amount of time per frame. May as well use it.
    if(line > 143) {
        mode = 1; //vblank
    }
    else {
        //1. oam access around 20 cycles (mode 2)
        //2. transfer to lcd for around 43 cycles (mode 3)
        //3. h-blank around 51 cycles (mode 0)
        //4. repeat 144 times, then vblank for 1140 cycles (mode 1)
        int line_cycle = frame_cycle % 114;
        if(line_cycle < 20) mode = 2; //OAM access
        else if (line_cycle < 20+43) mode = 3; //LCD transfer
    }
    return mode;
}

uint64_t lcd::get_active_cycle() {
    return cpu_active_cycle;
}

//Reads the CPU's view of the current state of the PPU
void lcd::read(int addr, void * val, int size, uint64_t cycle) {
    if(size > 1) return;
    //assert(size==1);
    if(addr >= 0x8000 && addr < 0xa000) {
        memcpy(val, &(cpu_vram[addr-0x8000]), size);
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        memcpy(val, &(cpu_oam[addr-0xfe00]), size);
    }
    else {
        switch(addr) {
            case 0xff40:
                *((uint8_t *)val) = cpu_control.val;
                break;
            case 0xff41: 
                if(!cpu_control.display_enable) {
                    *((uint8_t *)val) = cpu_status|1; //return current interrupt flags and v-blank mode, if screen is disabled.
                }
                else {
                    int mode = get_mode(cycle);
                    if(cpu_lyc == ((cycle - cpu_active_cycle) % 17556) / 114) {
                        mode |= 4;
                    }
                    *((uint8_t *)val) = mode | cpu_status;// | 0x80;
                }
                break;
            case 0xff42:
                *((uint8_t *)val) = cpu_bg_scroll_y;
                break;
            case 0xff43:
                *((uint8_t *)val) = cpu_bg_scroll_x;
                break;
            case 0xff44:
                if(!cpu_control.display_enable) {
                    *((uint8_t *)val) = 0;
                }
                else {
                    int frame_cycle = (cycle - cpu_active_cycle) % 17556;
                    int line = frame_cycle / 114;
                    *((uint8_t *)val) = line;
                }
                break;
            case 0xff45:
                *((uint8_t *)val) = cpu_lyc;
                break;
            case 0xff46:
                *((uint8_t *)val) = cpu_dma_addr;
                break;
            case 0xff47:
                *((uint8_t *)val) = cpu_bgpal.pal[0] | cpu_bgpal.pal[1]<<2 | cpu_bgpal.pal[2]<<4 | cpu_bgpal.pal[3]<<6;
                break;
            case 0xff48:
                *((uint8_t *)val) = cpu_obj1pal.pal[0] | cpu_obj1pal.pal[1]<<2 | cpu_obj1pal.pal[2]<<4 | cpu_obj1pal.pal[3]<<6;
                break;
            case 0xff49:
                *((uint8_t *)val) = cpu_obj2pal.pal[0] | cpu_obj2pal.pal[1]<<2 | cpu_obj2pal.pal[2]<<4 | cpu_obj2pal.pal[3]<<6;
                break;
            case 0xff4a:
                *((uint8_t *)val) = cpu_win_scroll_y;
                break;
            case 0xff4b:
                *((uint8_t *)val) = cpu_win_scroll_x;
                break;
            default:
                std::cout<<"PPU: Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
                *((uint8_t *)val) = 0xff;
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
    std::cout<<"PPU: Priority: "<<cpu_control.priority<<
                    " sprites on : "<<cpu_control.sprite_enable<<
                    " sprite size: "<<cpu_control.sprite_size<<
                    " bg map: "<<cpu_control.bg_map<<
                    " tile addr mode: "<<cpu_control.tile_addr_mode<<
                    " window enable: "<<cpu_control.window_enable<<
                    " window map: "<<cpu_control.window_map<<
                    " display on: "<<cpu_control.display_enable<<std::endl;
    std::ofstream vid;
    if(output_file) {
        vid.open((std::to_string(frame)+".pgm").c_str());
        vid<<"P5\n160 144\n3\n";
    }
    //std::ofstream vid("frame.pgm");
    uint8_t pgm_buffer[144][160];

    //Draw the background
    uint32_t bgbase = 0x1800;
    if(cpu_control.priority) { //cpu_controls whether the background displays, in regular DMG mode
        if(cpu_control.bg_map) bgbase = 0x1c00;
        for(int x_out_pix = 0; x_out_pix < 160; x_out_pix++) {
            int x_in_pix = (x_out_pix + bg_scroll_x) & 0xff;
            int x_tile = x_in_pix / 8;
            int x_tile_pix = x_in_pix % 8;
            for(int y_out_pix = 0; y_out_pix < 144; y_out_pix++) {
                int y_in_pix = (y_out_pix + bg_scroll_y) & 0xff;
                int y_tile = y_in_pix / 8;
                int y_tile_pix = y_in_pix % 8;
                int tile_num = cpu_vram[bgbase+y_tile*32+x_tile];
                if(cpu_control.tile_addr_mode) {
                    tile_num = 256 + int8_t(tile_num);
                }
                int base = tile_num * 16;
                int b1=cpu_vram[base+y_tile_pix*2];
                int b2=cpu_vram[base+y_tile_pix*2+1];
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
    if(cpu_control.window_map) winbase = 0x1c00;
    if(cpu_control.window_enable) {
        for(int tile_y = 0; tile_y + (win_scroll_y/8) < 18; tile_y++) {
            for(int tile_x = 0; tile_x + (win_scroll_x/8) < 20; tile_x++) {
                int tile_num = cpu_vram[winbase+tile_y*32+tile_x];
                if(cpu_control.tile_addr_mode) {
                    tile_num = 256+int8_t(tile_num);
                }
                int base = tile_num * 16;
                for(int y_tile_pix = 0; y_tile_pix < 8 && y_tile_pix + win_scroll_y + tile_y * 8 < 144; y_tile_pix++) {
                    int b1=cpu_vram[base+y_tile_pix*2];
                    int b2=cpu_vram[base+y_tile_pix*2+1];
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
    if(cpu_control.sprite_enable) {

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

void lcd::update_estimates(uint64_t cycle) {
    if(!cpu_control.display_enable) { //realistically shouldn't ever end up here
        lyc_next_cycle = -1;
        m0_next_cycle  = -1;
        m1_next_cycle  = -1;
        m2_next_cycle  = -1;
        return;
    }
    
    uint64_t frame_cycle = (cycle - cpu_active_cycle) % 17556;
    uint64_t line = frame_cycle / 114;
    uint64_t line_cycle = frame_cycle % 114;
    uint64_t frame_base_cycle = cycle - frame_cycle;

    uint64_t mode = get_mode(cycle);

    if(cpu_status & LYC) {
        lyc_next_cycle = frame_base_cycle + cpu_lyc * 114;
        if(lyc_next_cycle <= cycle) { //We've passed it this frame; go to the next.
            lyc_next_cycle += 17556;
        }
    }
    else {
        lyc_next_cycle = -1;
    }

    if(cpu_status & M0) {
        m0_next_cycle = frame_base_cycle + (114 * line) + 63;
        if(line_cycle >= 20 + 43) {
            m0_next_cycle += 114;
        }
    }
    else {
        m0_next_cycle = -1;
    }

    if(cpu_status & M1) {
        m1_next_cycle = frame_base_cycle + (144 * 114);
        if(m1_next_cycle <= cycle) {
            m1_next_cycle += 17556;
        }
    }
    else {
        m1_next_cycle = -1;
    }

    if(cpu_status & M2) {
        m2_next_cycle = frame_base_cycle + (114 * (line + 1));
    }
    else {
        m2_next_cycle = -1;
    }

}

bool lcd::interrupt_triggered(uint64_t cycle) {
    if(!cpu_control.display_enable) return false;

    bool retval = false;

    if(lyc_next_cycle <= cycle) retval = true;
    if(m0_next_cycle <= cycle) retval = true;
    if(m1_next_cycle <= cycle) retval = true;
    if(m2_next_cycle <= cycle) retval = true;

    if(retval) {
        update_estimates(cycle);
    }

    return retval;
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
