#include "lcd.h"
#include<iostream>
#include<cstring>
#include<cassert>
#include<fstream>
#include<string>

lcd::lcd() : debug(false), during_dma(false), cycle(0), next_line(0), control{.val=0x00}, bg_scroll_y(0), bg_scroll_x(0), 
             bgpal{{0,1,2,3}}, obj1pal{{0,1,2,3}}, obj2pal{{0,1,2,3}}, 
             win_scroll_y(0), win_scroll_x(0), active_cycle(0), frame(0), 
             lyc_next_cycle(0), m0_next_cycle(0), m1_next_cycle(0), m2_next_cycle(0), 
             cpu_control{.val=0x00}, cpu_status(0), cpu_bg_scroll_y(0), cpu_bg_scroll_x(0), cpu_lyc(0), cpu_dma_addr(0), 
             cpu_bgpal{{0,1,2,3}}, cpu_obj1pal{{0,1,2,3}}, cpu_obj2pal{{0,1,2,3}}, pal_index(0),
             cpu_win_scroll_y(0), cpu_win_scroll_x(0), cpu_active_cycle(0), 
             screen(NULL), renderer(NULL), buffer(NULL), texture(NULL), overlay(NULL), lps(NULL), hps(NULL), bg1(NULL), bg2(NULL),
             sgb_mode(false), sgb_dump_filename(""), sgb_mask_mode(0), sgb_vram_transfer_type(0), sgb_sys_pals(512), sgb_attrs(20*18, 0), 
             sgb_frame_attrs(32*32), sgb_tiles(256*8*8), sgb_set_low_tiles(false), sgb_set_high_tiles(false), sgb_set_bg_attr(false) {

    vram.resize(0x2000);
    oam.resize(0xa0);
    cpu_vram.resize(0x2000);
    cpu_oam.resize(0xa0);

    /* Initialize the SDL library */
    cur_x_res=160;
    cur_y_res=144;

    prev_texture = NULL;

    bool success = util::reinit_sdl_screen(&screen, &renderer, &texture, cur_x_res, cur_y_res);
    if(!success) {
        fprintf(stderr, "Something failed while init'ing video.\n");
        return;
    }

    buffer = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);

    overlay = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);
    lps = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);
    hps = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);

    bg1 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);
    bg2 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);

    if(buffer && bg1) {
        assert(buffer->pitch == 160*4);
        assert(bg1->pitch == 512 * 4);
    }

    //printf("lcd::Setting render draw color to black\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //printf("lcd::Clearing rendering output\n");
    SDL_RenderClear(renderer);
    //printf("lcd::Pushing video update to renderer\n");
    SDL_RenderPresent(renderer);
    //printf("lcd::constructor reached end\n");
    
    //Set default palettes; pal#0 is DMG default, and 0-3 are used for SGB
    sys_bgpal.resize(4);
    sys_winpal.resize(4);
    sys_obj1pal.resize(4);
    sys_obj2pal.resize(4);
    for(int i=0;i<4;i++) {
        sys_bgpal[i].resize(4);
        sys_winpal[i].resize(4);
        sys_obj1pal[i].resize(4);
        sys_obj2pal[i].resize(4);
    }

    uint8_t default_palette[] =
//         0              1              2              3
/*bg*/  {255, 255, 255, 170, 170, 170,  85,  85,  85,   0,   0,   0, //B+W, going from white to black
/*win*/  255, 210, 210, 175, 140, 140,  95,  70,  70,  15,   0,   0, //Light pink to deep, dark red
/*ob1*/  210, 255, 210, 140, 175, 140,  70,  95,  70,   0,  15,   0, //Seafoam green to Schwarzwald at midnight
/*ob2*/  210, 210, 255, 140, 140, 175,  70,  70,  95,   0,   0,  15}; //Powder blue to Mariana trench

    for(int i=0; buffer && i<4; i++) {
        sys_bgpal[0][i] = SDL_MapRGB(buffer->format, default_palette[i*3], default_palette[i*3+1], default_palette[i*3+2]);
        sys_winpal[0][i] = SDL_MapRGB(buffer->format, default_palette[i*3+12], default_palette[i*3+13], default_palette[i*3+14]);
        sys_obj1pal[0][i] = SDL_MapRGB(buffer->format, default_palette[i*3+24], default_palette[i*3+25], default_palette[i*3+26]);
        sys_obj2pal[0][i] = SDL_MapRGB(buffer->format, default_palette[i*3+36], default_palette[i*3+37], default_palette[i*3+38]);
    }

    //Documentation of how the values in the table above were generated
    /*
    for(int i=0;buffer && i<4;i++) {
        sys_bgpal[0][i] = SDL_MapRGB(buffer->format, 85*(3-i), 85*(3-i), 85*(3-i));
        sys_winpal[0][i] = SDL_MapRGB(buffer->format, 80*(3-i)+15, 70*(3-i), 70*(3-i));
        sys_obj1pal[0][i] = SDL_MapRGB(buffer->format, 70*(3-i), 80*(3-i)+15, 70*(3-i));
        sys_obj2pal[0][i] = SDL_MapRGB(buffer->format, 70*(3-i), 70*(3-i), 80*(3-i)+15);
    }
    */

    //Print out all SDL palette colors as a debug step
    /*
    for(int i = 0;i<4;i++) {
        for(int j=0;j<4;j++) {
            printf("%08x ", sys_bgpal[i][j]);
            printf("%08x ", sys_winpal[i][j]);
            printf("%08x ", sys_obj1pal[i][j]);
            printf("%08x ", sys_obj2pal[i][j]);
        }
        printf("\n");
    }
    */

}

lcd::~lcd() {
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
    if(prev_texture) {
        SDL_DestroyTexture(prev_texture);
        prev_texture = NULL;
    }
    if(buffer) {
        SDL_FreeSurface(buffer);
        buffer = NULL;
    }   
    if(overlay) {
        SDL_FreeSurface(overlay);
        overlay = NULL;
    }
    if(lps) {
        SDL_FreeSurface(lps);
        lps = NULL;
    }
    if(hps) {
        SDL_FreeSurface(hps);
        hps = NULL;
    }
    if(bg1) {
        SDL_FreeSurface(bg1);
        bg1 = NULL;
    }
    if(bg2) {
        SDL_FreeSurface(bg2);
        bg2 = NULL;
    }
}

uint64_t lcd::run(uint64_t run_to) {
    assert(cycle < run_to);
    //printf("PPU: Start: %ld run_to: %ld\n", cycle, run_to);

    uint64_t render_cycle = 0;
    while(cmd_queue.size() > 0) {
        util::cmd current = cmd_queue.front();
        if(current.cycle >= cycle) {
            if(!render_cycle) {
                render_cycle = render(frame, cycle, current.cycle);
            }
            else {
                render(frame, cycle, current.cycle);
            }
        }

        apply(current.addr, current.val, current.data_index, current.cycle);

        cmd_queue.pop_front();
        cycle = current.cycle;
    }

    //printf("PPU: renderend Startline: %ld endline: %ld\n",start_line,end_line);
    if(cycle < run_to) {
        if(!render_cycle) {
            render_cycle = render(frame, cycle, run_to);
        }
        else {
            render(frame, cycle, run_to);
        }
    }

    cycle = run_to;
    //printf("Reporting render at cycle: %lld\n", render_cycle);
    return render_cycle; //0 means a frame hasn't been rendered during this timeslice
}

//Apply the data extracted from the command queue, and apply it to the PPU view of the state
void lcd::apply(int addr, uint8_t val, uint64_t index, uint64_t cycle) {
    uint8_t prev = 0xb5; //stands for "BS"
    if(addr >= 0x8000 && addr < 0xa000) {
        prev = vram[addr&0x1fff];
        vram[addr & 0x1fff] = val;
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        prev = oam[addr-0xfe00];
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
                    prev = old_val.val;
                }
                break;
            case 0xff42:
                //printf("cycle: %ld (mode: %d line: %d cycle: %d) bg_scroll_y = %d\n", cycle, get_mode(cycle, true), ((cycle-active_cycle)%17556)/114, (cycle - active_cycle)%114, val);
                prev = bg_scroll_y;
                bg_scroll_y = val;
                break;
            case 0xff43:
                //printf("cycle: %ld (mode: %d line: %d cycle: %d) bg_scroll_x = %d\n", cycle, get_mode(cycle, true), ((cycle-active_cycle)%17556)/114, (cycle - active_cycle)%114 ,val);
                prev = bg_scroll_x;
                bg_scroll_x = val;
                break;
            case 0xff46://OAM DMA (deprecated; I'm just going to do this with regular register writes, to maintain the timing)
                during_dma = (val==1);
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
                printf("Ignoring 0x%04x = %02x @ %lld\n", addr, val, cycle);
                //Various data aren't necessary to render the screen, so we ignore anything like interrupts and status
                break;
        }
    }
    //printf("Processing %04x = %02x (prev: %02x) @ %lld\n", addr, val, prev, cycle);
    return;
}

void lcd::write(int addr, void * val, int size, uint64_t cycle) {
    if(size > 1) {
        printf("addr: %04x = ", addr);
        for(int i=0;i<size;i++) {
            printf(" %02x", ((uint8_t *)val)[i]);
        }
        printf(" @ %ld\n", cycle);
    }
    assert(size==1);
    //printf("PPU: 0x%04X = 0x%02x @ %ld (mode %d)\n", addr, *((uint8_t *)val), cycle, get_mode(cycle));
    if(addr >= 0x8000 && addr < 0xa000) {
        if(get_mode(cycle) != 3 || !cpu_control.display_enable) {
            memcpy(&(cpu_vram[addr-0x8000]), val, size);
            cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
        }
        if(debug) {
            uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
            int line_cycle = (cycle - cpu_active_cycle) % 114;
            timing_queue.emplace_back(util::cmd{line,line_cycle,0,0});
        }
        //else {
        //    printf("PPU: Cycle %010ld Denied write during mode   3: 0x%04x = 0x%02x\n", cycle, addr, *((uint8_t *)val));
        //}
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        if(get_mode(cycle) < 2 || !cpu_control.display_enable || cpu_during_dma) {
            memcpy(&(cpu_oam[addr - 0xfe00]), val, size);
            cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
        }
        if(debug) {
            uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
            int line_cycle = (cycle - cpu_active_cycle) % 114;
            timing_queue.emplace_back(util::cmd{line,line_cycle,1,cpu_during_dma});
        }
        //else {
        //    printf("PPU: Cycle %010ld Denied write during mode 2/3: 0x%04x = 0x%02x\n", cycle, addr, *((uint8_t *)val));
        //}
    }
    else {
        switch(addr) {
            case 0xff40:
                {
                    control_reg old_val{.val = cpu_control.val};
                    cpu_control.val = *((uint8_t *)val);
                    //Re-activates STAT interrupts when screen is reenabled
                    if(cpu_control.display_enable && !old_val.display_enable) {
                        cpu_active_cycle = cycle;
                        update_estimates(cycle);
                    }
                    //Disables STAT interrupts when screen is disabled
                    else if(old_val.display_enable && !cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                /*
                std::cout<<"PPU: CTRL change"<<
                    " Priority: "<<cpu_control.priority<<
                    " sprites on : "<<cpu_control.sprite_enable<<
                    " sprite size: "<<cpu_control.sprite_size<<
                    " bg map: "<<cpu_control.bg_map<<
                    " tile addr mode: "<<cpu_control.tile_addr_mode<<
                    " window enable: "<<cpu_control.window_enable<<
                    " window map: "<<cpu_control.window_map<<
                    " display on: "<<cpu_control.display_enable<<std::endl;
                */
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});

                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,2,0});
                }
                break;
            case 0xff41: 
                {
                    uint8_t old_val = cpu_status; 
                    cpu_status = *((uint8_t *)val)&0x78;
                    //If status flags are changed and screen is active, update times for newly active/inactive interrupts
                    if(cpu_status != old_val && cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                //printf("LCD status set to %02X\n", cpu_status);
                break;
            case 0xff42:
                cpu_bg_scroll_y = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,3,0});
                }
                break;
            case 0xff43:
                cpu_bg_scroll_x = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,4,0});
                }
                break;
            case 0xff45:
                {
                    uint8_t old_val = cpu_lyc;
                    cpu_lyc = *((uint8_t *)val);
                    if(cpu_lyc > 153) { //Out of range to actually trigger
                        lyc_next_cycle = -1;
                    }
                    //If LYC is a new value, then we need to recalculate the interrupt time
                    else if(old_val != cpu_lyc && cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                break;
            case 0xff46://OAM DMA
                //Send whether DMA is active or inactive (this is now just a bool, *not* the actual DMA; the DMA is transmitted as a series of writes directly to OAM)
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                //Value of DMA addr is set when DMA is first requested, so we don't handle it here
                break;
            case 0xff47:
                cpu_bgpal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_bgpal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_bgpal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_bgpal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,6,0});
                }
                break;
            case 0xff48:
                cpu_obj1pal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_obj1pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_obj1pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_obj1pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,7,0});
                }
                break;
            case 0xff49:
                cpu_obj2pal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_obj2pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_obj2pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_obj2pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,8,0});
                }
                break;
            case 0xff4a: //TODO: Actually influences mode3 timing
                cpu_win_scroll_y = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,9,0});
                }
                break;
            case 0xff4b: //TODO: influences mode3 timing
                cpu_win_scroll_x = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / 114;
                    int line_cycle = (cycle - cpu_active_cycle) % 114;
                    timing_queue.emplace_back(util::cmd{line,line_cycle,0x0a,0});
                }
                break;
            default:
                std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
    }
    return;
}

//TODO: Doesn't account for variations in mode2 and mode3 times
uint8_t lcd::get_mode(uint64_t cycle, bool ppu_view/*=false*/) {
    control_reg cnt = cpu_control;
    uint64_t active = cpu_active_cycle;
    if(ppu_view) {
        cnt = control;
        active = active_cycle;
    }

    if(!cnt.display_enable) {
        return 1;
    }
    int frame_cycle = (cycle - active) % 17556;
    int line = frame_cycle / 114;

    assert(line < 154);
    assert(line >= 0);

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
    if(size > 1 && size != 0x1000) return;
    //assert(size==1);
    if(addr >= 0x8000 && addr < 0xa000) {
        if(get_mode(cycle) != 3) {
            memcpy(val, &(cpu_vram[addr-0x8000]), size);
        }
        else *((uint8_t *)val) = 0xff;
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        if(get_mode(cycle) < 2) {
            memcpy(val, &(cpu_oam[addr-0xfe00]), size);
        }
        else *((uint8_t *)val) = 0xff;
    }
    else {
        switch(addr) {
            case 0xff40:
                *((uint8_t *)val) = cpu_control.val;
                break;
            case 0xff41: 
                if(!cpu_control.display_enable) {
                    *((uint8_t *)val) = BIT7|BIT0|cpu_status; //return current interrupt flags and v-blank mode, if screen is disabled.
                }
                else {
                    int mode = get_mode(cycle);
                    if(cpu_lyc == ((cycle - cpu_active_cycle) % 17556) / 114) {
                        mode |= BIT2;
                    }
                    *((uint8_t *)val) = mode | cpu_status | BIT7;
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
                    //Weird timing bug, that line 153 reads as line 0 for most of the time
                    if(line == 153 && frame_cycle % 114 >= 4) {
                        line = 0;
                    }
                    *((uint8_t *)val) = line;
                }
                break;
            case 0xff45:
                *((uint8_t *)val) = cpu_lyc;
                break;
            case 0xff46: //This value is now set in lcd::dma
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

void lcd::get_tile_row(int tilenum, int row, bool reverse, Vect<uint8_t>& pixels) {
    assert(tilenum < 384); assert(row < 16); assert(pixels.size() == 8);
    int addr = tilenum * 16 + row * 2;
    int b1 = vram[addr];
    int b2 = vram[addr + 1];
    for(int x = 0; x < 8; x++) {
        int x_i = x;
        if(reverse) x_i = 7 - x;
        int shift = 128>>(x_i);
        pixels[x] = ((b1&shift)/shift + 2*((b2&shift)/shift));
    }
    return;
}

uint64_t lcd::render(int frame, uint64_t start_cycle, uint64_t end_cycle) {
    if(!control.display_enable) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        return 0;
    }

    //printf("Raw: start cycle: %lld end: %lld\n", start_cycle, end_cycle);
    /*
    if(sgb_dump_filename != "") {
        std::ofstream chr_out(sgb_dump_filename.c_str());
        chr_out.write(reinterpret_cast<char *>(&vram[0x800]), 0x1000);
        chr_out.close();
        sgb_dump_filename = "";
    }
    */

    uint64_t start_frame_cycle = (start_cycle - active_cycle) % 17556;
    uint64_t start_frame_line = start_frame_cycle / 114;
    if(get_mode(start_cycle, true) == 0) { //output from that line has gone to the screen already
        start_frame_line++;
    }

    uint64_t end_frame_cycle = (end_cycle - active_cycle) % 17556;
    uint64_t end_frame_line = end_frame_cycle / 114;
    if(get_mode(end_cycle, true) >= 2) { //output from that line *hasn't* gone to the screen yet
        end_frame_line--;
    }

    if(end_frame_line > 153) return 0;
    if(end_frame_line < start_frame_line && end_frame_cycle - start_frame_cycle >= 114) {
        end_frame_line+=154;
    }

    bool output_sdl = true;
    uint64_t output_cycle = 0;

    Vect<uint8_t> tile_line(8, 0);

    uint32_t * pixels = NULL;

    Vect<uint8_t> bgmap(160, 0);

    if(!screen||!texture||!renderer) {
        printf("PPU: problem!\n");
        output_sdl = false;
    }
    else {
        pixels = ((uint32_t *)buffer->pixels);
    }

    //printf("Render starting conditions: startline: %lld endline: %lld\n", start_frame_line, end_frame_line);
    for(int line=start_frame_line;line <= end_frame_line; line++) {
        //printf("Running line: %d\n", line);
        int render_line = line % 154;
        if(debug && !sgb_mode && render_line == 153 && output_sdl) {
            draw_debug_overlay();
        }
        else if(render_line > 143) continue;

        //Draw the background
        if(control.priority) { //cpu_controls whether the background displays, in regular DMG mode
            if(output_sdl && start_frame_line == 0) { //Draw the background color
                SDL_SetRenderDrawColor(renderer, 85*(3 - bgpal.pal[0]), 85*(3 - bgpal.pal[0]), 85*(3 - bgpal.pal[0]), 255);
                SDL_RenderClear(renderer);
            }

            uint32_t bgbase = 0x1800;
            if(control.bg_map) bgbase = 0x1c00;

            int y_in_pix = (render_line + bg_scroll_y) & 0xff;
            int y_tile = y_in_pix / 8;
            int y_tile_pix = y_in_pix % 8;

            bool unloaded = true;
            int tile_num = 0;

            for(int x_out_pix = 0; x_out_pix < 160; x_out_pix++) {
                if(x_out_pix % 8 == 0) {
                    pal_index = sgb_attrs[(render_line/8)*20+x_out_pix/8];
                }
                int x_in_pix = (x_out_pix + bg_scroll_x) & 0xff;
                int x_tile = x_in_pix / 8;
                int x_tile_pix = x_in_pix % 8;

                if(x_tile_pix == 0 || unloaded) {
                    tile_num = vram[bgbase+y_tile*32+x_tile];
                    if(!control.tile_addr_mode) {
                        tile_num = 256 + int8_t(tile_num);
                    }
                    get_tile_row(tile_num, y_tile_pix, false, tile_line);
                    unloaded = false;
                }

                if(output_sdl /*&& c != 0*/) {
                    pixels[render_line * 160 + x_out_pix] = sys_bgpal[pal_index][bgpal.pal[tile_line[x_tile_pix]]];
                    bgmap[x_out_pix] = tile_line[x_tile_pix];
                }
            }
        }



        //Draw the window
        if(control.window_enable) {
            uint32_t winbase = 0x1800;
            if(control.window_map) winbase = 0x1c00;

            int win_y = (render_line - win_scroll_y);
            if(win_y >= 0) {
                int tile_y = win_y / 8;
                int y_tile_pix = win_y % 8;
                for(int tile_x = 0; tile_x * 8 + win_scroll_x - 7 < 160; tile_x++) {
                    int tile_num = vram[winbase+tile_y*32+tile_x];
                    if(!control.tile_addr_mode) {
                        tile_num = 256+int8_t(tile_num);
                    }
                    get_tile_row(tile_num, y_tile_pix, false, tile_line);
                    for(int x_tile_pix = 0; x_tile_pix < 8 && x_tile_pix + win_scroll_x + tile_x * 8 - 7 < 160; x_tile_pix++) {
                        int ycoord = tile_y * 8 + y_tile_pix + win_scroll_y;
                        int xcoord = tile_x * 8 + x_tile_pix + (win_scroll_x - 7);

                        pal_index = sgb_attrs[(ycoord/8)*20+xcoord/8];

                        if(xcoord % 8 == 0) {
                            pal_index = sgb_attrs[(ycoord/8)*20+xcoord/8];
                        }

                        if(output_sdl && xcoord >= 0) {
                            pixels[ycoord * 160 + xcoord] = sys_winpal[pal_index][bgpal.pal[tile_line[x_tile_pix]]];
                            bgmap[xcoord] = tile_line[x_tile_pix];
                        }
                    }
                }
            }
        }



        //Draw the sprites
        if(control.sprite_enable && !during_dma) {
            for(int spr = 0; spr < 40; spr++) {
                oam_data sprite_dat;
                memcpy(&sprite_dat, &oam[spr*4], 4);
                int sprite_y = sprite_dat.ypos - 16;
                int sprite_x = sprite_dat.xpos - 8;
                uint8_t tile = oam[spr*4+2];

                int sprite_size = 8 + (control.sprite_size * 8);

                //If sprite isn't displayed on this line...
                if(sprite_y > render_line || sprite_y + sprite_size <= render_line) {
                    continue;
                }

                if(control.sprite_size) {
                    tile &= 0xfe;
                }

                int y_i = render_line - sprite_y;
                if(sprite_dat.yflip == 1) {
                    y_i = sprite_size - (y_i + 1);
                }
                get_tile_row(tile, y_i, false, tile_line);

                pal_index = sgb_attrs[(render_line/8)*20+sprite_x/8];
                for(int x=0;x!=8;x++) {
                    int x_i = x;
                    if(sprite_dat.xflip == 1) {
                        x_i = 7 - x;
                    }

                    uint8_t col = tile_line[x_i];
                    uint32_t color = 0;
                    if(!col) continue;

                    int xcoord = sprite_x + x;
                    int ycoord = render_line;

                    if(xcoord % 8 == 0) {
                        pal_index = sgb_attrs[(render_line/8)*20+sprite_x/8];
                    }

                    if(sprite_dat.palnum_dmg) {
                        color = sys_obj2pal[pal_index][obj2pal.pal[col]];
                    }
                    else {
                        color = sys_obj1pal[pal_index][obj1pal.pal[col]];
                    }

                    if(xcoord > 159 || xcoord < 0) continue;

                    if(xcoord >= 0 && xcoord < 160 && (bgmap[xcoord] == 0 || !sprite_dat.priority)) {
                        pixels[ycoord * 160 + xcoord] = color;
                    }
                }
            }

        }

        if(output_sdl && render_line == 143) {
            if(prev_texture) {
                SDL_DestroyTexture(prev_texture);
                prev_texture = NULL;
            }

            if(texture) {
                prev_texture = texture;
            }

            texture = SDL_CreateTextureFromSurface(renderer, buffer);

            if(sgb_mode) { //Super GameBoy has a few modes that mask video output
                if(sgb_mask_mode == 0) {
                    float xscale = float(win_x_res) / float(cur_x_res);
                    float yscale = float(win_y_res) / float(cur_y_res);
                    SDL_Rect screen_middle{int(48.0*xscale),int(40.0*yscale),int(160.0*xscale),int(144.0*yscale)};
                    SDL_RenderCopy(renderer,texture,NULL,&screen_middle);
                }
                else if(sgb_mask_mode != 1) { //Screen frozen
                    //SDL_RenderClear(renderer);
                }
                else if(sgb_mask_mode == 2) { //Screen to black
                    SDL_SetRenderDrawColor(renderer, 0,0,0,255);
                }
                else if(sgb_mask_mode == 3) { //Screen to bg color 0
                    SDL_SetRenderDrawColor(renderer, sys_bgpal[0][bgpal.pal[0]], sys_bgpal[0][bgpal.pal[0]], sys_bgpal[0][bgpal.pal[0]], 255);
                }
                //Copy the SGB border texture, then render the screen
                SDL_RenderCopy(renderer, sgb_texture, NULL, NULL);
                SDL_RenderPresent(renderer);
            }
            else if(!debug) {
                SDL_RenderCopy(renderer,texture,NULL,NULL);
                SDL_RenderPresent(renderer);
            }
            else { //Debug, but not SGB
                SDL_Rect debug_space{114,0,160,144};
                SDL_RenderCopy(renderer, texture, NULL, &debug_space);
            }
        }
        if(render_line == 143) {
            output_cycle = start_cycle - start_frame_cycle + (143*114) + 20 + 43;
            //printf("Outputting at cycle: %lld\n", output_cycle);
        }

    }

    return output_cycle;
}

void lcd::update_estimates(uint64_t cycle) {
    if(!cpu_control.display_enable) {
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

    //uint64_t mode = get_mode(cycle);

    //TODO: The code telling it to skip to the next frame makes me uneasy. I should probably track whether it was just triggered, and needs to be updated.
    if((cpu_status & LYC) > 0) {
        lyc_next_cycle = frame_base_cycle + cpu_lyc * 114 + 1;
        if(lyc_next_cycle <= cycle) { //We've passed it this frame; go to the next.
            lyc_next_cycle = frame_base_cycle + 17556 + cpu_lyc * 144 + 1;
        }
    }
    else {
        lyc_next_cycle = -1;
    }

    if((cpu_status & M0) > 0) {
        m0_next_cycle = frame_base_cycle + (114 * line) + 63;
        if(line_cycle >= 20 + 43) {
            m0_next_cycle += 114;
        }
    }
    else {
        m0_next_cycle = -1;
    }

    if((cpu_status & M1) > 0) {
        m1_next_cycle = frame_base_cycle + (144 * 114);
        if(m1_next_cycle <= cycle) {
            m1_next_cycle += 17556;
        }
    }
    else {
        m1_next_cycle = -1;
    }

    if((cpu_status & M2) > 0) {
        m2_next_cycle = frame_base_cycle + (114 * (line + 1));
    }
    else {
        m2_next_cycle = -1;
    }

}

bool lcd::interrupt_triggered(uint64_t cycle) {
    if(!cpu_control.display_enable) return false;

    bool retval = false;

    if(lyc_next_cycle <= cycle) {
        //printf("stat interrupt: ly match: %d (%ld cycles since triggered)\n", cpu_lyc, cycle - lyc_next_cycle);
        retval = true;
    }
    if(m0_next_cycle <= cycle) {
        //printf("stat interrupt: hblank (%ld cycles since triggered)\n", cycle - m0_next_cycle);
        retval = true;
    }
    if(m1_next_cycle <= cycle) {
        //printf("stat interrupt: vblank (%ld cycles since triggered)\n", cycle - m1_next_cycle);
        retval = true;
    }
    if(m2_next_cycle <= cycle) {
        //printf("stat interrupt: oam search (%ld cycles since triggered)\n", cycle - m2_next_cycle);
        retval = true;
    }

    if(retval) {
        update_estimates(cycle);
    }

    return retval;
}

uint32_t lcd::map_bg_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a/*=255*/) {
    //uint32_t color = SDL_MapRGB(buffer->format,85*(3-bgpal.pal[tile_line[x_tile_pix]]),85*(3-bgpal.pal[tile_line[x_tile_pix]]),85*(3-bgpal.pal[tile_line[x_tile_pix]]));
    return 0;
}

uint32_t lcd::map_win_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a/*=255*/) {
    //uint32_t color = SDL_MapRGB(buffer->format,80*(3-bgpal.pal[col])+15,70*(3-bgpal.pal[col]),70*(3-bgpal.pal[col]));
    return 0;
}

uint32_t lcd::map_oam1_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a/*=255*/) {
    //uint32_t color = SDL_MapRGB(buffer->format, 70 * ( 3-obj1pal.pal[col]), 80 * ( 3-obj1pal.pal[col])+15, 70 * ( 3-obj1pal.pal[col]));
    return 0;
}

uint32_t lcd::map_oam2_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a/*=255*/) {
    //uint32_t color = SDL_MapRGB(buffer->format, 70 * ( 3-obj2pal.pal[col]), 70 * ( 3-obj2pal.pal[col]), 80 * ( 3-obj2pal.pal[col])+15);
    return 0;
}

void lcd::sgb_trigger_dump(std::string filename) {
    sgb_dump_filename = filename;
}

void lcd::set_debug(bool db) {
    debug = db;
}

void lcd::toggle_debug() {
    debug = !debug;
}

uint64_t lcd::get_frame() {
    return frame;
}

void lcd::dma(bool during_dma, uint64_t cycle, uint8_t dma_addr) {
    uint8_t temp = during_dma;
    cpu_during_dma = during_dma;
    cpu_dma_addr = dma_addr;
    write(0xff46, &temp, 1, cycle);
}

void lcd::draw_debug_overlay() {
    //Draw the guides
    SDL_Rect white_rect  = { 0,0,114,154};
    SDL_Rect yellow_rect = { 0,0, 20,144};
    SDL_Rect pink_rect   = {20,0, 43,144};
    SDL_SetRenderDrawColor(renderer, 255,255,255,150);
    SDL_RenderFillRect(renderer, &white_rect);
    if(control.display_enable) {
        SDL_SetRenderDrawColor(renderer, 255,255,200,150);
        SDL_RenderFillRect(renderer, &yellow_rect);
        SDL_SetRenderDrawColor(renderer, 255,200,200,150);
        SDL_RenderFillRect(renderer, &pink_rect);
        SDL_SetRenderDrawColor(renderer, 20,20,20,150);
        SDL_RenderDrawRect(renderer, &white_rect);
    }

    uint64_t last_seen = 0;
    while(timing_queue.size() > 0 && timing_queue.front().cycle >= last_seen) {
        util::cmd c = timing_queue.front();
        uint64_t line = c.cycle;
        last_seen = line;
        int line_cycle = c.addr;
        uint8_t type = c.val;
        uint64_t during_dma = c.data_index;
        if(type == 1 || type == 5) { //OAM writes
            if(line_cycle < 63 && line < 144 && control.display_enable && ! during_dma) { //Forbidden
                SDL_SetRenderDrawColor(renderer, 255,0,0,255);
                SDL_RenderDrawPoint(renderer, line_cycle, line);
            }
            else {
                SDL_SetRenderDrawColor(renderer, 0,255,0,255);
                SDL_RenderDrawPoint(renderer, line_cycle, line);
            }
        }
        else if(type == 0) { //VRAM writes
            if(line_cycle >= 20 && line_cycle < 63 && line < 144 && control.display_enable) { //VRAM writes
                SDL_SetRenderDrawColor(renderer, 255,0,0,255);
                SDL_RenderDrawPoint(renderer, line_cycle, line);
            }
            else {
                SDL_SetRenderDrawColor(renderer, 0,255,0,255);
                SDL_RenderDrawPoint(renderer, line_cycle, line);
            }
        }
        else {
            SDL_SetRenderDrawColor(renderer, 0,0,255,255);
            SDL_RenderDrawPoint(renderer, line_cycle, line);
        }
        timing_queue.pop_front();
    }
    SDL_RenderPresent(renderer);
}

void lcd::sgb_set_pals(uint8_t pal1, uint8_t pal2, Vect<uint8_t>& colors) { //SGB commands 00, 01, 02, 03
    assert(colors.size() == 21);
    uint32_t col0 = SDL_MapRGB(buffer->format, colors[0], colors[1], colors[2]);
    sys_bgpal[pal1][0] = col0;
    sys_winpal[pal1][0] = col0;
    sys_obj1pal[pal1][0] = col0;
    sys_obj2pal[pal1][0] = col0;
    sys_bgpal[pal2][0] = col0;
    sys_winpal[pal2][0] = col0;
    sys_obj1pal[pal2][0] = col0;
    sys_obj2pal[pal2][0] = col0;
    for(int i=1;i<4;i++) {
        uint32_t colx = SDL_MapRGB(buffer->format, colors[i*3+0], colors[i*3+1], colors[i*3+2]);
        sys_bgpal[pal1][i] = colx;
        sys_winpal[pal1][i] = colx;
        sys_obj1pal[pal1][i] = colx;
        sys_obj2pal[pal1][i] = colx;

        colx = SDL_MapRGB(buffer->format, colors[i*3+9], colors[i*3+10], colors[i*3+11]);
        sys_bgpal[pal2][i] = colx;
        sys_winpal[pal2][i] = colx;
        sys_obj1pal[pal2][i] = colx;
        sys_obj2pal[pal2][i] = colx;
    }
}

void lcd::sgb_set_attrs(Vect<uint8_t>& attrs) {
    assert(attrs.size() == 360);
    for(int i=0;i<360;i++) {
        sgb_attrs[i] = attrs[i];
    }
}

void lcd::sgb_vram_transfer(uint8_t type) { //SGB commands 0B, 13, 14, 15
    sgb_vram_transfer_type = type;
    /* For reference:
    switch(type) {
        case 0: //none
        case 1: //pal, transfers 512 palettes of 8 bytes each
            break;
        case 2: //chr0, transfers 128 32-byte tiles in 4-bit color, using the 4 bit plane arrangement from SNES (tiles 0-127)
            break;
        case 3: //chr1, transfers 128 32-byte tiles in 4-bit color, using the 4 bit plane arrangement from SNES (tiles 128-255)
            break;
        case 4: //pct, transfers 1024 16-bit background attributes, then 16 2-byte colors (as palettes 4-7)
            break;
        case 5: //attr, transfers 90 attribute transfer files
            break;
    }
    */
}

void lcd::sgb_set_mask_mode(uint8_t mode) { //SGB command 17
    mode &= 0x3;
    sgb_mask_mode = mode;
}

void lcd::sgb_enable(bool enable) {
    bool switched = (sgb_mode != enable);
    sgb_mode = enable;

    if(sgb_mode && switched) {
        cur_x_res = 256;
        cur_y_res = 224;
        win_x_res = 512;
        win_y_res = 448;
        bool success = util::reinit_sdl_screen(&screen, &renderer, &texture, cur_x_res, cur_y_res);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        if(overlay) {
            SDL_FreeSurface(overlay);
            overlay = NULL;
        }
        overlay = SDL_CreateRGBSurface(0,256,224,32,0,0,0,0);

        if(sgb_border) {
            SDL_FreeSurface(sgb_border);
            sgb_border = NULL;
        }
        sgb_border = SDL_CreateRGBSurfaceWithFormat(0,256,224,32,SDL_PIXELFORMAT_RGBA8888);
        SDL_SetSurfaceBlendMode(sgb_border, SDL_BLENDMODE_BLEND);

        SDL_Rect center{48,40,160,144};
        SDL_FillRect(sgb_border, NULL, SDL_MapRGBA(sgb_border->format, 0,0,0,255));
        SDL_FillRect(sgb_border, &center, SDL_MapRGBA(sgb_border->format, 255,0,0,0));

        if(sgb_texture) {
            SDL_DestroyTexture(sgb_texture);
            sgb_texture = NULL;
        }
        sgb_texture = SDL_CreateTextureFromSurface(renderer, sgb_border);

        sgb_frame_pals.resize(4);
        for(int i=0;i<4;i++) {
            sgb_frame_pals[i].resize(16);
        }
    }
    else if(switched) {
        cur_x_res = 160;
        cur_y_res = 144;
        win_x_res = 320;
        win_y_res = 288;
        bool success = util::reinit_sdl_screen(&screen, &renderer, &texture, cur_x_res, cur_y_res);

        if(overlay) {
            SDL_FreeSurface(overlay);
            overlay = NULL;
        }
        overlay = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);

        if(sgb_border) {
            SDL_FreeSurface(sgb_border);
            sgb_border = NULL;
        }
        if(sgb_texture) {
            SDL_DestroyTexture(sgb_texture);
            sgb_texture = NULL;
        }
    }
}

void lcd::win_resize(unsigned int new_x, unsigned int new_y) {
    win_x_res = new_x;
    win_y_res = new_y;
}
