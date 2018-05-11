#include "lcd.h"
#include<iostream>
#include<cstring>
#include<cassert>
#include<fstream>
#include<string>

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT //assert
#endif

lcd::lcd() : cgb_mode(false), debug(false), during_dma(false), cycle(0), next_line(0), control{.val=0x00}, bg_scroll_y(0), bg_scroll_x(0), 
             bgpal{{0,1,2,3}}, obj1pal{{0,1,2,3}}, obj2pal{{0,1,2,3}}, 
             win_scroll_y(0), win_scroll_x(0), active_cycle(0), frame(0), 
             lyc_next_cycle(0), m0_next_cycle(0), m1_next_cycle(0), m2_next_cycle(0), 
             cpu_control{.val=0x00}, cpu_status(0), cpu_bg_scroll_y(0), cpu_bg_scroll_x(0), cpu_lyc(0), cpu_dma_addr(0), 
             cpu_bgpal{{0,1,2,3}}, cpu_obj1pal{{0,1,2,3}}, cpu_obj2pal{{0,1,2,3}},             cpu_win_scroll_y(0), cpu_win_scroll_x(0), cpu_active_cycle(0), 
             screen(NULL), renderer(NULL), buffer(NULL), texture(NULL), overlay(NULL), lps(NULL), hps(NULL), bg1(NULL), bg2(NULL), sgb_border(NULL), sgb_texture(NULL),
             sgb_mode(false), sgb_dump_filename(""), sgb_vram_transfer_type(0), sgb_mask_mode(0), trace(false), vram_bank(0), cpu_vram_bank(0) {

    /* Initialize the SDL library */
    cur_x_res=SCREEN_WIDTH;
    cur_y_res=SCREEN_HEIGHT;

    prev_texture = NULL;

    bool success = util::reinit_sdl_screen(&screen, &renderer, &texture, cur_x_res, cur_y_res);
    if(!success) {
        fprintf(stderr, "Something failed while init'ing video.\n");
        return;
    }

    render = std::bind(&lcd::dmg_render, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    buffer = SDL_CreateRGBSurface(0,SCREEN_WIDTH,SCREEN_HEIGHT,32,0,0,0,0);

    overlay = SDL_CreateRGBSurface(0,SCREEN_WIDTH,SCREEN_HEIGHT,32,0,0,0,0);
    lps = SDL_CreateRGBSurface(0,SCREEN_WIDTH,SCREEN_HEIGHT,32,0,0,0,0);
    hps = SDL_CreateRGBSurface(0,SCREEN_WIDTH,SCREEN_HEIGHT,32,0,0,0,0);

    bg1 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);
    bg2 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);

    if(buffer && bg1) {
        ASSERT(buffer->pitch == SCREEN_WIDTH*4);
        ASSERT(bg1->pitch == 512 * 4);
    }

    //printf("lcd::Setting render draw color to black\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //printf("lcd::Clearing rendering output\n");
    SDL_RenderClear(renderer);
    //printf("lcd::Pushing video update to renderer\n");
    SDL_RenderPresent(renderer);
    //printf("lcd::constructor reached end\n");
    
    //Set default palettes; pal#0 is DMG default, 0-3 are used for SGB, 0-7 are used for CGB

    //Semi-colorized default palette
    //    uint8_t default_palette[] =
    //         0              1              2              3
    //*bg*/  {255, 255, 255, 170, 170, 170,  85,  85,  85,   0,   0,   0, //B+W, going from white to black
    //*win*/  255, 210, 210, 175, 140, 140,  95,  70,  70,  15,   0,   0, //Light pink to deep, dark red
    //*ob1*/  210, 255, 210, 140, 175, 140,  70,  95,  70,   0,  15,   0, //Seafoam green to Schwarzwald at midnight
    //*ob2*/  210, 210, 255, 140, 140, 175,  70,  70,  95,   0,   0,  15}; //Powder blue to Mariana trench


    //GB Pocket inspired palette
        uint8_t default_palette[] =
    //         0              1              2              3
    /*bg*/  {224, 219, 205, 168, 159, 148, 112, 107, 102,  43,  43,  38, //B+W, going from white to black
    /*win*/  224, 219, 205, 168, 159, 148, 112, 107, 102,  43,  43,  38, //Light pink to deep, dark red
    /*ob1*/  224, 219, 205, 168, 159, 148, 112, 107, 102,  43,  43,  38, //Seafoam green to Schwarzwald at midnight
    /*ob2*/  224, 219, 205, 168, 159, 148, 112, 107, 102,  43,  43,  38}; //Powder blue to Mariana trench


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

    //Precalculate possible tile rows (about 1/2 MB of data)
    for(int b2=0;b2<256;b2++) {
        for(int b1=0;b1<256;b1++) {
            int index = (b2<<8|b1);
            for(int x = 0; x < 8; x++) {
                int shift = 128>>(x);
                rows[index][x] = ((b1&shift)/shift + 2*((b2&shift)/shift));
            }
        }
    }
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
    ASSERT(cycle < run_to);
    //printf("PPU: Start: %ld run_to: %ld\n", cycle, run_to);

    uint64_t render_cycle = 0;
    while(!cmd_queue.empty()) {
        util::cmd current = cmd_queue.front();
        if(current.cycle >= cycle) {
            if(!render_cycle) {
                render_cycle = render(frame, cycle, current.cycle);
            }
            else {
                render(frame, cycle, current.cycle);
            }
        }

        apply(current.addr, current.val, current.cycle);

        cmd_queue.pop();
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
void lcd::apply(int addr, uint8_t val, uint64_t cycle) {
    //uint8_t prev = 0xb5; //stands for "BS"
    if(addr >= 0x8000 && addr < 0xa000) {
        uint8_t prev = vram[vram_bank][addr&0x1fff];
        vram[vram_bank][addr & 0x1fff] = val;
        if(addr < 0x9800 && prev != val) {
            update_row_cache(addr, val);
        }
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        //prev = oam[addr-0xfe00];
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
                    //prev = old_val.val;
                }
                break;
            case 0xff42:
                //printf("cycle: %ld (mode: %d line: %d cycle: %d) bg_scroll_y = %d\n", cycle, get_mode(cycle, true), ((cycle-active_cycle)%17556)/CYCLES_PER_LINE, (cycle - active_cycle)%CYCLES_PER_LINE, val);
                //prev = bg_scroll_y;
                bg_scroll_y = val;
                break;
            case 0xff43:
                //printf("cycle: %ld (mode: %d line: %d cycle: %d) bg_scroll_x = %d\n", cycle, get_mode(cycle, true), ((cycle-active_cycle)%17556)/CYCLES_PER_LINE, (cycle - active_cycle)%CYCLES_PER_LINE ,val);
                //prev = bg_scroll_x;
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
            case 0xff4f:
                vram_bank = val;
                break;
            case 0xff68:
                cgb_bgpal_index = (val & 0x3f);
                cgb_bgpal_advance = ((val & 0x80) == 0x80);
                break;
            case 0xff69:
                {
                    uint8_t hilo = (cgb_bgpal_index & 1);
                    uint8_t col = ((cgb_bgpal_index>>1) & 3);
                    uint8_t pal = (cgb_bgpal_index >> 3);
                    if(!hilo) { //low byte
                        cgb_bgpal[pal][col].low = val;
                    }
                    else {
                        cgb_bgpal[pal][col].high = val;
                    }
                    sys_bgpal[pal][col] = SDL_MapRGB(buffer->format, cgb_bgpal[pal][col].red * 8, cgb_bgpal[pal][col].green * 8, cgb_bgpal[pal][col].blue * 8);
                    if(cgb_bgpal_advance) {
                        cgb_bgpal_index++;
                        cgb_bgpal_index &= 0x3f;
                    }
                    //if(hilo)
                        //printf("BG pal %d col %d is now (%d, %d, %d)\n", pal, col, cgb_bgpal[pal][col].red * 8, cgb_bgpal[pal][col].green * 8, cgb_bgpal[pal][col].blue * 8);
                }
                break;
            case 0xff6a:
                cgb_objpal_index = (val & 0x3f);
                cgb_objpal_advance = ((val & 0x80) == 0x80);
                break;
            case 0xff6b:
                {
                    uint8_t hilo = (cgb_objpal_index & 1);
                    uint8_t col = ((cgb_objpal_index>>1) & 3);
                    uint8_t pal = (cgb_objpal_index >> 3);
                    if(!hilo) { //low byte
                        cgb_objpal[pal][col].low = val;
                    }
                    else {
                        cgb_objpal[pal][col].high = val;
                    }
                    sys_obj1pal[pal][col] = SDL_MapRGB(buffer->format, cgb_objpal[pal][col].red * 8, cgb_objpal[pal][col].green * 8, cgb_objpal[pal][col].blue * 8);
                    if(cgb_objpal_advance) {
                        cgb_objpal_index++;
                        cgb_objpal_index &= 0x3f;
                    }
                    //if(hilo)
                        //printf("Obj pal %d col %d is now (%d, %d, %d)\n", pal, col, cgb_objpal[pal][col].red * 8, cgb_objpal[pal][col].green * 8, cgb_objpal[pal][col].blue * 8);
                }
                break;
            default:
                printf("Ignoring 0x%04x = %02x @ %ld\n", addr, val, cycle);
                //Various data aren't necessary to render the screen, so we ignore anything like interrupts and status
                break;
        }
    }
    //printf("Processing %04x = %02x (prev: %02x) @ %lld\n", addr, val, prev, cycle);
    return;
}

void lcd::update_row_cache(uint16_t addr, uint8_t val) {
    addr -= 0x8000;
    int tilenum = addr>>4;
    int row = ((addr>>1) & 7);
    int index = vram_bank * 3072 + tilenum * 8 + row;
    int data_base = addr & 0xfffe;
    int b1 = 0;
    int b2 = 0;
    if(addr&1) {
        b1 = vram[vram_bank][data_base];
        b2 = val;
    }
    else {
        b1 = val;
        b2 = vram[vram_bank][data_base + 1];
    }
    row_cache[index] = rows[b2<<8|b1];
    return;
}

void lcd::write(int addr, uint8_t val, uint64_t cycle) {
    if(trace) {printf("PPU write : %s @ %ld\n", lcd_to_string(addr, val).c_str(), cycle);}
    //printf("PPU: 0x%04X = 0x%02x @ %ld (mode %d)\n", addr, *((uint8_t *)val), cycle, get_mode(cycle));
    if(addr >= 0x8000 && addr < 0xa000) {
        if(get_mode(cycle) != 3 || !cpu_control.display_enable) {
            cpu_vram[cpu_vram_bank][addr-0x8000] = val;
            cmd_queue.emplace(util::cmd{cycle, addr, val});
        }
        if(debug) {
            uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
            int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
            timing_queue.emplace(util::timing_data{line,line_cycle,0,0});
        }
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        if(get_mode(cycle) < 2 || !cpu_control.display_enable || cpu_during_dma) {
            cpu_oam[addr - 0xfe00] = val;
            cmd_queue.emplace(util::cmd{cycle, addr, val});
        }
        if(debug) {
            uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
            int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
            timing_queue.emplace(util::timing_data{line,line_cycle,1,cpu_during_dma});
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
                    cpu_control.val = val;
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
                cmd_queue.emplace(util::cmd{cycle, addr, val});

                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,2,0});
                }
                break;
            case 0xff41: 
                {
                    uint8_t old_val = cpu_status; 
                    cpu_status = val&0x78;
                    //If status flags are changed and screen is active, update times for newly active/inactive interrupts
                    if(cpu_status != old_val && cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                //printf("LCD status set to %02X\n", cpu_status);
                break;
            case 0xff42:
                cpu_bg_scroll_y = val;
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,3,0});
                }
                break;
            case 0xff43:
                cpu_bg_scroll_x = val;
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,4,0});
                }
                break;
            case 0xff45:
                {
                    uint8_t old_val = cpu_lyc;
                    cpu_lyc = val;
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
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                //Value of DMA addr is set when DMA is first requested, so we don't handle it here
                break;
            case 0xff47:
                cpu_bgpal.pal[0] = val & 0x03;
                cpu_bgpal.pal[1] = (val & 0x0c)>>2;
                cpu_bgpal.pal[2] = (val & 0x30)>>4;
                cpu_bgpal.pal[3] = (val & 0xc0)>>6;
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,6,0});
                }
                break;
            case 0xff48:
                cpu_obj1pal.pal[0] = val & 0x03;
                cpu_obj1pal.pal[1] = (val & 0x0c)>>2;
                cpu_obj1pal.pal[2] = (val & 0x30)>>4;
                cpu_obj1pal.pal[3] = (val & 0xc0)>>6;
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,7,0});
                }
                break;
            case 0xff49:
                cpu_obj2pal.pal[0] = val & 0x03;
                cpu_obj2pal.pal[1] = (val & 0x0c)>>2;
                cpu_obj2pal.pal[2] = (val & 0x30)>>4;
                cpu_obj2pal.pal[3] = (val & 0xc0)>>6;
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,8,0});
                }
                break;
            case 0xff4a: //TODO: Actually influences mode3 timing
                cpu_win_scroll_y = val;
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,9,0});
                }
                break;
            case 0xff4b: //TODO: influences mode3 timing
                cpu_win_scroll_x = val;
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                if(debug) {
                    uint64_t line = ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE;
                    int line_cycle = (cycle - cpu_active_cycle) % CYCLES_PER_LINE;
                    timing_queue.emplace(util::timing_data{line,line_cycle,0x0a,0});
                }
                break;
            case 0xff4f: //CGB VRAM bank
                cpu_vram_bank = (val & 1);
                cmd_queue.emplace(util::cmd{cycle, addr, uint8_t(val & 1)});
                break;
            case 0xff68:
                cpu_cgb_bgpal_index = (val & 0x3f);
                cpu_cgb_bgpal_advance = ((val & 0x80) == 0x80);
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                break;
            case 0xff69:
                if(get_mode(cycle) != 3 || !cpu_control.display_enable) {
                    cpu_cgb_bgpal[cpu_cgb_bgpal_index] = val;
                    cmd_queue.emplace(util::cmd{cycle, addr, val});
                    if(cpu_cgb_bgpal_advance) {
                        cpu_cgb_bgpal_index++;
                        cpu_cgb_bgpal_index %= 0x3f;
                    }
                }
                break;
            case 0xff6a:
                cpu_cgb_objpal_index = (val & 0x3f);
                cpu_cgb_objpal_advance = ((val & 0x80) == 0x80);
                cmd_queue.emplace(util::cmd{cycle, addr, val});
                break;
            case 0xff6b:
                if(get_mode(cycle) != 3 || !cpu_control.display_enable) {
                    cpu_cgb_objpal[cpu_cgb_objpal_index] = val;
                    cmd_queue.emplace(util::cmd{cycle, addr, val});
                    if(cpu_cgb_objpal_advance) {
                        cpu_cgb_objpal_index++;
                        cpu_cgb_objpal_index %= 0x3f;
                    }
                }
                break;
            default:
                //std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
                break;
        }
    }
    //if(trace) printf("PPU write (post): %s\n", lcd_to_string(addr, *((uint8_t *)val)).c_str());
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
    div_t line_cycle_dat = div(frame_cycle, CYCLES_PER_LINE);

    ASSERT(line_cycle_dat.quot < LINES_PER_FRAME);
    ASSERT(line_cycle_dat.quot >= 0);

    int mode = 0; //hblank; largest amount of time per frame. May as well use it.
    if(line_cycle_dat.quot > LAST_RENDER_LINE) {
        mode = 1; //vblank
    }
    else {
        //1. oam access around 20 cycles (mode 2)
        //2. transfer to lcd for around 43 cycles (mode 3)
        //3. h-blank around 51 cycles (mode 0)
        //4. repeat 144 times, then vblank for 1140 cycles (mode 1)
        if(line_cycle_dat.rem < MODE_2_LENGTH) mode = 2; //OAM access
        else if (line_cycle_dat.rem < MODE_2_LENGTH+MODE_3_LENGTH) mode = 3; //LCD transfer
    }
    return mode;
}

uint64_t lcd::get_active_cycle() {
    return cpu_active_cycle;
}

//Reads the CPU's view of the current state of the PPU
uint8_t lcd::read(int addr, uint64_t cycle) {
    uint8_t retval = 0;
    //if(size > 1 && size != 0x1000) return;
    if(addr >= 0x8000 && addr < 0xa000) {
        if(get_mode(cycle) != 3) {
            retval = cpu_vram[cpu_vram_bank][addr-0x8000];
        }
        else retval = 0xff;
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        if(get_mode(cycle) < 2) {
            retval = cpu_oam[addr-0xfe00];
        }
        else retval = 0xff;
    }
    else {
        switch(addr) {
            case 0xff40:
                retval = cpu_control.val;
                break;
            case 0xff41: 
                if(!cpu_control.display_enable) {
                    retval = BIT7|cpu_status; //return current interrupt flags and v-blank mode, if screen is disabled.
                }
                else {
                    int mode = get_mode(cycle);
                    if(cpu_lyc == ((cycle - cpu_active_cycle) % 17556) / CYCLES_PER_LINE) {
                        mode |= BIT2;
                    }
                    retval = mode | cpu_status | BIT7;
                }
                break;
            case 0xff42:
                retval = cpu_bg_scroll_y;
                break;
            case 0xff43:
                retval = cpu_bg_scroll_x;
                break;
            case 0xff44:
                if(!cpu_control.display_enable) {
                    retval = 0;
                }
                else {
                    int frame_cycle = (cycle - cpu_active_cycle) % 17556;
                    int line = frame_cycle / CYCLES_PER_LINE;
                    //Weird timing bug, that line 153 reads as line 0 for most of the time
                    if(line == 153 && frame_cycle % CYCLES_PER_LINE >= 4) {
                        line = 0;
                    }
                    retval = line;
                }
                break;
            case 0xff45:
                retval = cpu_lyc;
                break;
            case 0xff46: //This value is now set in lcd::dma
                retval = cpu_dma_addr;
                break;
            case 0xff47:
                retval = cpu_bgpal.pal[0] | cpu_bgpal.pal[1]<<2 | cpu_bgpal.pal[2]<<4 | cpu_bgpal.pal[3]<<6;
                break;
            case 0xff48:
                retval = cpu_obj1pal.pal[0] | cpu_obj1pal.pal[1]<<2 | cpu_obj1pal.pal[2]<<4 | cpu_obj1pal.pal[3]<<6;
                break;
            case 0xff49:
                retval = cpu_obj2pal.pal[0] | cpu_obj2pal.pal[1]<<2 | cpu_obj2pal.pal[2]<<4 | cpu_obj2pal.pal[3]<<6;
                break;
            case 0xff4a:
                retval = cpu_win_scroll_y;
                break;
            case 0xff4b:
                retval = cpu_win_scroll_x;
                break;
            case 0xff4f:
                retval = (0xfe | cpu_vram_bank);
                break;
            case 0xff68:
                retval = cpu_cgb_bgpal_index;
                break;
            case 0xff69:
                retval = cpu_cgb_bgpal[cpu_cgb_bgpal_index];
                break;
            case 0xff6a:
                retval = cpu_cgb_objpal_index;
                break;
            case 0xff6b:
                retval = cpu_cgb_objpal[cpu_cgb_objpal_index];
                break;
            default:
                //std::cout<<"PPU: Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
                retval = 0xff;
                break;
        }
    }
    if(trace) printf("PPU read: %s\n", lcd_to_string(addr, retval).c_str());
    return retval;
}

void lcd::get_tile_row(int tilenum, int row, bool reverse, Arr<uint8_t, 8>& pixels, int bank/*=0*/) {
#ifdef UNCACHED
    ASSERT(tilenum < 384); ASSERT(row < 16); ASSERT(pixels.size() == 8);
    int addr = tilenum * 16 + row * 2;
    int b1 = vram[bank][addr];
    int b2 = vram[bank][addr + 1];
    for(int x = 0; x < 8; x++) {
        int x_i = x;
        if(reverse) x_i = 7 - x;
        int shift = 128>>(x_i);
        pixels[x] = ((b1&shift)/shift + 2*((b2&shift)/shift));
    }
    return;
#else
    int index = bank * 3072 + tilenum * 8 + row;
    if(reverse) {
        std::copy(row_cache[index].begin(), row_cache[index].end(), pixels.rbegin());
    } else {
        std::copy(row_cache[index].begin(), row_cache[index].end(), pixels.begin());
    }
#endif
}

uint64_t lcd::dmg_render(int frame, uint64_t start_cycle, uint64_t end_cycle) {
    if(!control.display_enable) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        return 0;
    }

    //printf("Raw: start cycle: %lld end: %lld\n", start_cycle, end_cycle);
    if(sgb_dump_filename != "") {
        std::ofstream chr_out(sgb_dump_filename.c_str());
        chr_out.write(reinterpret_cast<char *>(&vram[0][0x0]), 0x2000);
        chr_out.close();
        sgb_dump_filename = "";
    }

    uint64_t start_frame_cycle = (start_cycle - active_cycle) % 17556;
    uint64_t start_frame_line = start_frame_cycle / CYCLES_PER_LINE;
    if(get_mode(start_cycle, true) == 0) { //output from that line has gone to the screen already
        start_frame_line++;
    }

    uint64_t end_frame_cycle = (end_cycle - active_cycle) % 17556;
    uint64_t end_frame_line = end_frame_cycle / CYCLES_PER_LINE;
    if(get_mode(end_cycle, true) >= 2) { //output from that line *hasn't* gone to the screen yet
        end_frame_line--;
    }

    if(end_frame_line > 153) return 0;
    if(end_frame_line < start_frame_line && end_frame_cycle - start_frame_cycle >= CYCLES_PER_LINE) {
        end_frame_line+=LINES_PER_FRAME;
    }

    bool output_sdl = true;
    uint64_t output_cycle = 0;

    Arr<uint8_t, 8> tile_line;

    //uint32_t * pixels = NULL;

    Vect<uint8_t> bgmap(SCREEN_WIDTH, 0);

    if(!screen||!texture||!renderer) {
        printf("PPU: problem!\n");
        output_sdl = false;
    }
    else {
        //pixels = ((uint32_t *)buffer->pixels);
        //pixels = &out_buf[0];
    }

    //printf("Render starting conditions: startline: %lld endline: %lld\n", start_frame_line, end_frame_line);
    for(unsigned int line=start_frame_line;line <= end_frame_line; line++) {
        //printf("Running line: %d\n", line);
        int render_line = line % LINES_PER_FRAME;
        if(debug && !sgb_mode && render_line == 153 && output_sdl) {
            draw_debug_overlay();
            continue;
        }
        else if(render_line > LAST_RENDER_LINE) continue;

        //Draw the background
        if(control.priority) { //cpu_controls whether the background displays, in regular DMG mode
            if(output_sdl && start_frame_line == 0) { //Draw the background color
                SDL_SetRenderDrawColor(renderer, 85*(3 - bgpal.pal[0]), 85*(3 - bgpal.pal[0]), 85*(3 - bgpal.pal[0]), 255);
                SDL_RenderClear(renderer);
            }

            uint32_t bgbase = 0x1800 + 0x400 * control.bg_map;

            int y_in_pix = (render_line + bg_scroll_y) & 0xff;
            int y_tile = y_in_pix / 8;
            int y_tile_pix = y_in_pix % 8;

            bool unloaded = true;
            int tile_num = 0;
            int pal_index = 0;

            for(int x_out_pix = 0; x_out_pix < SCREEN_WIDTH; x_out_pix++) {
                if(x_out_pix % 8 == 0) {
                    pal_index = sgb_attrs[(render_line/8)*SCREEN_TILE_WIDTH+x_out_pix/8];
                }
                int x_in_pix = (x_out_pix + bg_scroll_x) & 0xff;
                int x_tile = x_in_pix / 8;
                int x_tile_pix = x_in_pix % 8;

                if(x_tile_pix == 0 || unloaded) {
                    tile_num = vram[vram_bank][bgbase+y_tile*32+x_tile];
                    if(!control.tile_addr_mode) {
                        tile_num = 256 + int8_t(tile_num);
                    }
                    get_tile_row(tile_num, y_tile_pix, false, tile_line);
                    unloaded = false;
                }

                if(output_sdl /*&& c != 0*/) {
                    out_buf[render_line * SCREEN_WIDTH + x_out_pix] = sys_bgpal[pal_index][bgpal.pal[tile_line[x_tile_pix]]];
                    bgmap[x_out_pix] = tile_line[x_tile_pix];
                }
            }
        }



        //Draw the window
        if(control.window_enable) {
            uint32_t winbase = 0x1800 + 0x400 * control.window_map;

            int win_y = (render_line - win_scroll_y);
            if(win_y >= 0) {
                int tile_y = win_y / 8;
                int y_tile_pix = win_y % 8;
                for(int tile_x = 0; tile_x * 8 + win_scroll_x - 7 < SCREEN_WIDTH; tile_x++) {
                    int tile_num = vram[vram_bank][winbase+tile_y*32+tile_x];
                    if(!control.tile_addr_mode) {
                        tile_num = 256+int8_t(tile_num);
                    }
                    get_tile_row(tile_num, y_tile_pix, false, tile_line);
                    for(int x_tile_pix = 0; x_tile_pix < 8 && x_tile_pix + win_scroll_x + tile_x * 8 - 7 < SCREEN_WIDTH; x_tile_pix++) {
                        int ycoord = tile_y * 8 + y_tile_pix + win_scroll_y;
                        int xcoord = tile_x * 8 + x_tile_pix + (win_scroll_x - 7);

                        int pal_index = 0;
                        if(xcoord >= 0 && xcoord < SCREEN_WIDTH) {
                            pal_index = sgb_attrs[(ycoord/8)*SCREEN_TILE_WIDTH+xcoord/8];
                        }

                        if(output_sdl && xcoord >= 0) {
                            out_buf[ycoord * SCREEN_WIDTH + xcoord] = sys_winpal[pal_index][bgpal.pal[tile_line[x_tile_pix]]];
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
                get_tile_row(tile, y_i, sprite_dat.xflip, tile_line);

                int pal_index = 0;
                if(sprite_x >=0 && sprite_x < SCREEN_WIDTH) {
                    pal_index = sgb_attrs[(render_line/8)*SCREEN_TILE_WIDTH+sprite_x/8];
                }

                for(int x=0;x!=8;x++) {
                    uint8_t col = tile_line[x];
                    uint32_t color = 0;
                    if(!col) continue;

                    int xcoord = sprite_x + x;
                    int ycoord = render_line;

                    if(xcoord % 8 == 0 && xcoord >= 0 && xcoord < SCREEN_WIDTH) {
                        pal_index = sgb_attrs[(render_line/8)*SCREEN_TILE_WIDTH+sprite_x/8];
                    }

                    if(sprite_dat.palnum_dmg) {
                        color = sys_obj2pal[pal_index][obj2pal.pal[col]];
                    }
                    else {
                        color = sys_obj1pal[pal_index][obj1pal.pal[col]];
                    }

                    if(xcoord > 159 || xcoord < 0) continue;

                    if(xcoord >= 0 && xcoord < SCREEN_WIDTH && (bgmap[xcoord] == 0 || !sprite_dat.priority)) {
                        out_buf[ycoord * SCREEN_WIDTH + xcoord] = color;
                    }
                }
            }

        }

        if(output_sdl && render_line == LAST_RENDER_LINE) {
            if(sgb_vram_transfer_type != 0) {
                //printf("Map:%02x Scroll:(%02x. %02x) AddrMode:%02x pal:(%x,%x,%x,%x)\n", control.bg_map, bg_scroll_x, bg_scroll_y, control.tile_addr_mode, bgpal.pal[0], bgpal.pal[1], bgpal.pal[2], bgpal.pal[3]);
                //uint16_t map_base=0x1800 + 0x400 * control.bg_map;
                //for(int i=0;i<0x400;i++) {
                    //printf("%02x ", vram[map_base+i]);
                    //if((i+1)%32==0) printf("\n");
                //}
                do_vram_transfer();
                sgb_vram_transfer_type = 0;
            }
            if(prev_texture) {
                SDL_DestroyTexture(prev_texture);
                prev_texture = NULL;
            }

            if(texture) {
                prev_texture = texture;
            }

            memcpy(buffer->pixels, &out_buf[0], 160*144*4);
            texture = SDL_CreateTextureFromSurface(renderer, buffer);

            if(sgb_mode) { //Super GameBoy has a few modes that mask video output
                if(sgb_mask_mode == 0) {
                    float xscale = float(win_x_res) / float(cur_x_res);
                    float yscale = float(win_y_res) / float(cur_y_res);
                    sgb_color bgcol = sgb_frame_pals[0].col[0];
                    SDL_SetRenderDrawColor(renderer, bgcol.red, bgcol.green, bgcol.blue, 255);
                    SDL_RenderClear(renderer);
                    SDL_Rect screen_middle{int(48.0*xscale),int(40.0*yscale),int(xscale * SCREEN_WIDTH),int(yscale * SCREEN_HEIGHT)};
                    SDL_RenderCopy(renderer,texture,NULL,&screen_middle);
                    SDL_RenderCopy(renderer, sgb_texture, NULL, NULL);
                }
                else if(sgb_mask_mode == 1) { //Screen frozen
                }
                else if(sgb_mask_mode == 2) { //Screen to black
                    SDL_SetRenderDrawColor(renderer, 0,0,0,255);
                    SDL_RenderClear(renderer);
                }
                else if(sgb_mask_mode == 3) { //Screen to bg color 0
                    SDL_SetRenderDrawColor(renderer, sys_bgpal[0][bgpal.pal[0]], sys_bgpal[0][bgpal.pal[0]], sys_bgpal[0][bgpal.pal[0]], 255);
                    SDL_RenderClear(renderer);
                }

                if(sgb_mask_mode != 1) {
                    SDL_RenderPresent(renderer);
                }
            }
            else if(!debug) {
                int ww,wh;
                SDL_GetWindowSize(screen, &ww, &wh);
                int bh = 0;
                int bw = 0;
                if(ww >= wh) {
                    bh = wh;
                    bw = ((wh*160)/144);
                }
                else {
                    bw = ww;
                    bh = ((ww*144)/160);
                }
                SDL_Rect dontstretch{(ww-bw)>>1, (wh-bh)>>1, bw, bh};
                SDL_RenderCopy(renderer,texture,NULL,&dontstretch);

                SDL_RenderPresent(renderer);
            }
            else { //Debug, but not SGB
                SDL_Rect debug_space{CYCLES_PER_LINE,0,SCREEN_WIDTH,SCREEN_HEIGHT};
                SDL_RenderCopy(renderer, texture, NULL, &debug_space);
            }
        }
        if(render_line == LAST_RENDER_LINE) {
            output_cycle = start_cycle - start_frame_cycle + (LAST_RENDER_LINE*CYCLES_PER_LINE) + MODE_2_LENGTH + MODE_3_LENGTH;
            //printf("Outputting at cycle: %lld\n", output_cycle);
        }

    }

    return output_cycle;
}

uint64_t lcd::cgb_render(int frame, uint64_t start_cycle, uint64_t end_cycle) {
    if(!control.display_enable) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        return 0;
    }

    if(sgb_dump_filename != "") {
        std::ofstream chr_out(sgb_dump_filename.c_str());
        chr_out.write(reinterpret_cast<char *>(&vram[0][0x0]), 0x2000);
        chr_out.write(reinterpret_cast<char *>(&vram[1][0x0]), 0x2000);
        chr_out.close();
        sgb_dump_filename = "";
    }

    //printf("Raw: start cycle: %lld end: %lld\n", start_cycle, end_cycle);

    uint64_t start_frame_cycle = (start_cycle - active_cycle) % 17556;
    uint64_t start_frame_line = start_frame_cycle / CYCLES_PER_LINE;
    if(get_mode(start_cycle, true) == 0) { //output from that line has gone to the screen already
        start_frame_line++;
    }

    uint64_t end_frame_cycle = (end_cycle - active_cycle) % 17556;
    uint64_t end_frame_line = end_frame_cycle / CYCLES_PER_LINE;
    if(get_mode(end_cycle, true) >= 2) { //output from that line *hasn't* gone to the screen yet
        end_frame_line--;
    }

    if(end_frame_line > 153) return 0;
    if(end_frame_line < start_frame_line && end_frame_cycle - start_frame_cycle >= CYCLES_PER_LINE) {
        end_frame_line+=LINES_PER_FRAME;
    }

    bool output_sdl = true;
    uint64_t output_cycle = 0;

    Arr<uint8_t, 8> tile_line;

    //uint32_t * pixels = NULL;


    if(!screen||!texture||!renderer) {
        printf("PPU: problem!\n");
        output_sdl = false;
    }
    else {
        //pixels = ((uint32_t *)buffer->pixels);
        //pixels = &out_buf[0];
    }

    //printf("Render starting conditions: startline: %lld endline: %lld\n", start_frame_line, end_frame_line);
    for(unsigned int line=start_frame_line;line <= end_frame_line; line++) {
        Vect<uint8_t> bgmap(SCREEN_WIDTH, 0);
        //printf("Running line: %d\n", line);
        int render_line = line % LINES_PER_FRAME;
        if(debug && render_line == 153 && output_sdl) {
            draw_debug_overlay();
            continue;
        }
        else if(render_line > LAST_RENDER_LINE) continue;

        //Draw the background
        uint32_t bgbase = 0x1800 + 0x400 * control.bg_map;

        int y_in_pix = (render_line + bg_scroll_y) & 0xff;
        int y_tile = y_in_pix / 8;
        int y_tile_pix = y_in_pix % 8;

        bool unloaded = true;
        int tile_num = 0;
        bg_attrib a{.val=0};
        int bgpriority = control.priority * 2;
        int tile_priority = 0;

        for(int x_out_pix = 0; x_out_pix < SCREEN_WIDTH; x_out_pix++) {
            int x_in_pix = (x_out_pix + bg_scroll_x) & 0xff;
            int x_tile = x_in_pix / 8;
            int x_tile_pix = x_in_pix % 8;

            if(x_tile_pix == 0 || unloaded) {
                tile_num = vram[0][bgbase+y_tile*32+x_tile];
                a.val =    vram[1][bgbase+y_tile*32+x_tile];
                if(!control.tile_addr_mode) {
                    tile_num = 256 + int8_t(tile_num);
                }
                if(bgpriority) {
                    if(a.priority) {
                        tile_priority = 4;
                    }
                    else {
                        tile_priority = 2;
                    }
                }
                else {
                    tile_priority = 0;
                }
                int y_i = y_tile_pix;
                if(a.yflip) y_i = 7 - y_tile_pix;
                get_tile_row(tile_num, y_i, a.xflip, tile_line, a.char_bank);
                unloaded = false;
            }

            if(output_sdl /*&& c != 0*/) {
                int col_num = tile_line[x_tile_pix];
                out_buf[render_line * SCREEN_WIDTH + x_out_pix] = sys_bgpal[a.palette][col_num];
                if(col_num) {
                    bgmap[x_out_pix] = tile_priority;
                }
            }
        }

        //Draw the window
        if(control.window_enable) {
            uint32_t winbase = 0x1800 + 0x400 * control.window_map;

            int win_y = (render_line - win_scroll_y);
            if(win_y >= 0) {
                int tile_y = win_y / 8;
                int y_tile_pix = win_y % 8;
                for(int tile_x = 0; tile_x * 8 + win_scroll_x - 7 < SCREEN_WIDTH; tile_x++) {
                    int tile_num = vram[0][winbase+tile_y*32+tile_x];
                    bg_attrib a{.val = vram[1][winbase+tile_y*32+tile_x]};
                    if(!control.tile_addr_mode) {
                        tile_num = 256+int8_t(tile_num);
                    }
                    if(bgpriority) {
                        if(a.priority) {
                            tile_priority = 4;
                        }
                        else {
                            tile_priority = 2;
                        }
                    }
                    else {
                        tile_priority = 0;
                    }

                    int y_i = y_tile_pix;
                    if(a.yflip) y_i = 7 - y_tile_pix;

                    get_tile_row(tile_num, y_i, a.xflip, tile_line, a.char_bank);
                    for(int x_tile_pix = 0; x_tile_pix < 8 && x_tile_pix + win_scroll_x + tile_x * 8 - 7 < SCREEN_WIDTH; x_tile_pix++) {
                        int ycoord = tile_y * 8 + y_tile_pix + win_scroll_y;
                        int xcoord = tile_x * 8 + x_tile_pix + (win_scroll_x - 7);

                        if(output_sdl && xcoord >= 0) {
                            int col_num = tile_line[x_tile_pix];
                            out_buf[ycoord * SCREEN_WIDTH + xcoord] = sys_bgpal[a.palette][col_num];
                            if(col_num) {
                                bgmap[xcoord] = tile_priority;
                            }
                        }
                    }
                }
            }
        }

        //Draw the sprites
        if(control.sprite_enable && !during_dma) {
            for(int spr = 39; spr >= 0; spr--) {
                oam_data sprite_dat;
                memcpy(&sprite_dat, &oam[spr*4], 4);
                int sprite_y = sprite_dat.ypos - 16;
                int sprite_x = sprite_dat.xpos - 8;
                uint8_t tile = sprite_dat.tile;
                int pal_index = sprite_dat.palnum_cgb;

                int sprite_size = 8 + (control.sprite_size * 8);

                tile_priority = ((sprite_dat.priority)?1:3);

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
                get_tile_row(tile, y_i, sprite_dat.xflip, tile_line, sprite_dat.tilebank);

                int ycoord = render_line;
                for(int x=0;x<8;x++) {
                    int col = tile_line[x];
                    int xcoord = sprite_x + x;
                    if(!col || xcoord > 159 || xcoord < 0) continue;
                    if(tile_priority > bgmap[xcoord]) {
                        out_buf[ycoord * SCREEN_WIDTH + xcoord] = sys_obj1pal[pal_index][col];
                    }
                }
            }
        }

        if(output_sdl && render_line == LAST_RENDER_LINE) {
            if(prev_texture) {
                SDL_DestroyTexture(prev_texture);
                prev_texture = NULL;
            }

            if(texture) {
                prev_texture = texture;
            }

            memcpy(buffer->pixels, &out_buf[0], 160*144*4);
            texture = SDL_CreateTextureFromSurface(renderer, buffer);

            if(debug) {
                SDL_Rect debug_space{CYCLES_PER_LINE,0,SCREEN_WIDTH,SCREEN_HEIGHT};
                SDL_RenderCopy(renderer, texture, NULL, &debug_space);
            }
            else {
                int ww,wh;
                SDL_GetWindowSize(screen, &ww, &wh);
                int bh = 0;
                int bw = 0;
                if(ww >= wh) {
                    bh = wh;
                    bw = ((wh*160)/144);
                }
                else {
                    bw = ww;
                    bh = ((ww*144)/160);
                }

                SDL_Rect dontstretch{(ww-bw)>>1, (wh-bh)>>1, bw, bh};
                SDL_RenderCopy(renderer,texture,NULL,&dontstretch);
            }
            SDL_RenderPresent(renderer);
        }
        if(render_line == LAST_RENDER_LINE) {
            output_cycle = start_cycle - start_frame_cycle + (LAST_RENDER_LINE*CYCLES_PER_LINE) + MODE_2_LENGTH + MODE_3_LENGTH;
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
    uint64_t line = frame_cycle / CYCLES_PER_LINE;
    uint64_t line_cycle = frame_cycle % CYCLES_PER_LINE;
    uint64_t frame_base_cycle = cycle - frame_cycle;

    //uint64_t mode = get_mode(cycle);

    //TODO: The code telling it to skip to the next frame makes me uneasy. I should probably track whether it was just triggered, and needs to be updated.
    if((cpu_status & LYC) > 0) {
        lyc_next_cycle = frame_base_cycle + cpu_lyc * CYCLES_PER_LINE + 1;
        if(lyc_next_cycle <= cycle) { //We've passed it this frame; go to the next.
            lyc_next_cycle = frame_base_cycle + 17556 + cpu_lyc * CYCLES_PER_LINE + 1;
        }
    }
    else {
        lyc_next_cycle = -1;
    }

    if((cpu_status & M0) > 0) {
        m0_next_cycle = frame_base_cycle + (CYCLES_PER_LINE * line) + 63;
        if(line_cycle >= MODE_2_LENGTH + 43) {
            m0_next_cycle += CYCLES_PER_LINE;
        }
    }
    else {
        m0_next_cycle = -1;
    }

    if((cpu_status & M1) > 0) {
        m1_next_cycle = frame_base_cycle + (SCREEN_HEIGHT * CYCLES_PER_LINE);
        if(m1_next_cycle <= cycle) {
            m1_next_cycle += 17556;
        }
    }
    else {
        m1_next_cycle = -1;
    }

    if((cpu_status & M2) > 0) {
        m2_next_cycle = frame_base_cycle + (CYCLES_PER_LINE * (line + 1));
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

void lcd::toggle_trace() {
    trace = !trace;
}

uint64_t lcd::get_frame() {
    return frame;
}

void lcd::dma(bool during_dma, uint64_t cycle, uint8_t dma_addr) {
    uint8_t temp = during_dma;
    cpu_during_dma = during_dma;
    cpu_dma_addr = dma_addr;
    write(0xff46, temp, cycle);
}

void lcd::draw_debug_overlay() {
    //Draw the guides
    SDL_Rect white_rect  = { 0,0,CYCLES_PER_LINE,LINES_PER_FRAME};
    SDL_Rect yellow_rect = { 0,0, MODE_2_LENGTH,SCREEN_HEIGHT};
    SDL_Rect pink_rect   = {MODE_2_LENGTH,0, 43,SCREEN_HEIGHT};
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
    while(!timing_queue.empty() && timing_queue.front().cycle >= last_seen) {
        util::timing_data c = timing_queue.front();
        uint64_t line = c.cycle;
        last_seen = line;
        int line_cycle = c.addr;
        uint8_t type = c.val;
        uint64_t during_dma = c.data_index;
        if(type == 1 || type == 5) { //OAM writes
            if(line_cycle < 63 && line < SCREEN_HEIGHT && control.display_enable && ! during_dma) { //Forbidden
                SDL_SetRenderDrawColor(renderer, 255,0,0,255);
                SDL_RenderDrawPoint(renderer, line_cycle, line);
            }
            else {
                SDL_SetRenderDrawColor(renderer, 0,255,0,255);
                SDL_RenderDrawPoint(renderer, line_cycle, line);
            }
        }
        else if(type == 0) { //VRAM writes
            if(line_cycle >= MODE_2_LENGTH && line_cycle < 63 && line < SCREEN_HEIGHT && control.display_enable) { //VRAM writes
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
        timing_queue.pop();
    }
    SDL_RenderPresent(renderer);
}

void lcd::sgb_set_pals(uint8_t pal1, uint8_t pal2, Arr<uint16_t, 7>& colors) { //SGB commands 00, 01, 02, 03
    /*printf("Setting palettes %d and %d to: ", pal1, pal2);
    for(int i=0;i<7;i++) {
	    sgb_color col;
	    col.val=colors[i];
	    printf("%04x=(%02x,%02x,%02x) ",colors[i],col.red,col.green,col.blue);
    }*/
    //ASSERT(colors.size() == 7);
    sgb_color col;
    col.val=colors[0];
    uint32_t col0 = SDL_MapRGB(buffer->format, col.red*8, col.green*8, col.blue*8);
    sys_bgpal[pal1][0] = col0;
    sys_winpal[pal1][0] = col0;
    sys_obj1pal[pal1][0] = col0;
    sys_obj2pal[pal1][0] = col0;
    sys_bgpal[pal2][0] = col0;
    sys_winpal[pal2][0] = col0;
    sys_obj1pal[pal2][0] = col0;
    sys_obj2pal[pal2][0] = col0;
    for(int i=1;i<4;i++) {
        col.val = colors[i];
        uint32_t colx = SDL_MapRGB(buffer->format, col.red*8, col.green*8, col.blue*8);
        sys_bgpal[pal1][i] = colx;
        sys_winpal[pal1][i] = colx;
        sys_obj1pal[pal1][i] = colx;
        sys_obj2pal[pal1][i] = colx;

        col.val = colors[i+3];
        colx = SDL_MapRGB(buffer->format, col.red*8, col.green*8, col.blue*8);
        sys_bgpal[pal2][i] = colx;
        sys_winpal[pal2][i] = colx;
        sys_obj1pal[pal2][i] = colx;
        sys_obj2pal[pal2][i] = colx;
    }
}

void lcd::sgb_set_attrs(Arr<uint8_t,360>& attrs) {
    //ASSERT(attrs.size() == 360);
    for(int i=0;i<360;i++) {
        if(attrs[i] != 10)
            sgb_attrs[i] = attrs[i];
    }
}

void lcd::sgb_set_attrib_from_file(uint8_t attr_file, bool cancel_mask) {
    if(cancel_mask) sgb_mask_mode = 0;
    for(int byte=0; byte<90;byte++) {
        sgb_attrs[byte*4] = sgb_attr_files[attr_file].block[byte].block0;
        sgb_attrs[byte*4+1] = sgb_attr_files[attr_file].block[byte].block1;
        sgb_attrs[byte*4+2] = sgb_attr_files[attr_file].block[byte].block2;
        sgb_attrs[byte*4+3] = sgb_attr_files[attr_file].block[byte].block3;
    }

}
void lcd::sgb_pal_transfer(uint16_t pal0, uint16_t pal1, uint16_t pal2, uint16_t pal3, uint8_t attr_file, bool use_attr, bool cancel_mask) {
    uint16_t pals[] = {pal0, pal1, pal2, pal3};
    if(cancel_mask) sgb_mask_mode = 0;
    if(use_attr) {
        for(int byte=0; byte<90;byte++) {
            sgb_attrs[byte*4] = sgb_attr_files[attr_file].block[byte].block0;
            sgb_attrs[byte*4+1] = sgb_attr_files[attr_file].block[byte].block1;
            sgb_attrs[byte*4+2] = sgb_attr_files[attr_file].block[byte].block2;
            sgb_attrs[byte*4+3] = sgb_attr_files[attr_file].block[byte].block3;
        }
    }

    for(int pal=0;pal<4;pal++) {
        for(int col=0;col<4;col++) {
            uint8_t r = sgb_sys_pals[pals[pal]].col[col].red * 8;
            uint8_t g = sgb_sys_pals[pals[pal]].col[col].green * 8;
            uint8_t b = sgb_sys_pals[pals[pal]].col[col].blue * 8;
            uint32_t colx = SDL_MapRGB(buffer->format, r, g, b);
            sys_bgpal[pal][col] = colx;
            sys_winpal[pal][col] = colx;
            sys_obj1pal[pal][col] = colx;
            sys_obj2pal[pal][col] = colx;
        }
    }
}

//Just requests that a transfer occurs when next frame finishes rendering
void lcd::sgb_vram_transfer(uint8_t type) { //SGB commands 0B, 13, 14, 15
    sgb_vram_transfer_type = type;
}

//Transfers the data to the appropriate section of memory
void lcd::do_vram_transfer() {
    uint8_t type = sgb_vram_transfer_type;
    Arr<uint8_t, 4096> vram_data;
    if(type > 0) {
        interpret_vram(vram_data);
        /*
        for(int i=0;i<4096;i++) {
            if(i%64==0) printf("%04x  ", i);
            printf("%02X ", vram_data[i]);
            if((i+1) % 64 == 0) printf("\n");
        }
        printf("\n");
        */
    }

    switch(type) {
        case 0: //none
            break;
        case 1: //pal, transfers 512 palettes of 8 bytes each
            ASSERT(sizeof(sgb_color) == 2);
            ASSERT(sizeof(sgb_pal4) == 8);
            ASSERT(sgb_sys_pals.size() == 512);
            memcpy(&(sgb_sys_pals[0]), &(vram_data[0]), 4096);
            break;
        case 2: //chr0, transfers 128 32-byte tiles in 4-bit color, using the 4 bit plane arrangement from SNES (tiles 0-127)
        case 3: //chr1, transfers 128 32-byte tiles in 4-bit color, using the 4 bit plane arrangement from SNES (tiles 128-255)
            {
                uint16_t offset = 0;
                if(type == 3) offset = 8192;
                for(int i=0;i<4096;i+=32) {
                    for(int y=0;y<8;y++) {
                        uint8_t byte0 = vram_data[i+2*y];
                        uint8_t byte1 = vram_data[i+2*y+1];
                        uint8_t byte2 = vram_data[i+2*y+16];
                        uint8_t byte3 = vram_data[i+2*y+17];
                        for(int x=0;x<8;x++) {
                            uint8_t col = ((byte0&0x80)>>7) | 2*((byte1&0x80)>>7) | 4*((byte2&0x80)>>7) | 8*((byte3&0x80)>>7);
                            byte0<<=1;
                            byte1<<=1;
                            byte2<<=1;
                            byte3<<=1;
                            sgb_tiles[offset + (i/32)*64 + y*8 + x] = col;
                        }
                    }
                }
            }
            break;
        case 4: //pct, transfers 1024 16-bit background attributes, then 16 2-byte colors (as palettes 4-7)
            //printf("sgb_attr size: %d\n", sizeof(sgb_attr));
            ASSERT(sizeof(sgb_attr) == 2);
            ASSERT(sizeof(sgb_color) == 2);
            ASSERT(sgb_frame_pals.size() == 4);
            ASSERT(sizeof(sgb_pal16) == 32);
            ASSERT(sgb_frame_attrs.size() == 1024);
            memcpy(&(sgb_frame_attrs[0]), &(vram_data[0]), 2048);
            memcpy(&(sgb_frame_pals[0]), &(vram_data[0x800]), 128);
            /*
            for(int p=0;p<4;p++) {
                printf("pal %d: ", p);
                for(int c=0;c<16;c++) 
                    printf("%d: %x ", c, sgb_frame_pals[p].col[c]);
                printf("\n");
            }
            */
            break;
        case 5: //attr, transfers 45 90-byte attribute transfer files
            ASSERT(sgb_attr_files.size() == 45);
            printf("attrib file size: %ld\n", sizeof(attrib_file));
            ASSERT(sizeof(attrib_file) == 90);
            memcpy(&(sgb_attr_files[0]), &(vram_data[0]), 4050);
            break;
        default: //undefined
            break;
    }
    if(type == 2 || type == 3 || type == 4) {
        regen_background();
    }
}

//regenerate the background texture
void lcd::regen_background() {
    //printf("Regenerate! (from type %d)\n", sgb_vram_transfer_type);
    uint32_t * pixels = (uint32_t *)(sgb_border->pixels);
    uint32_t palette[16];
    palette[0] = SDL_MapRGBA(sgb_border->format, 255,0,0,0);
    for(int i = 0; i < 28; i++) {
        for(int j = 0; j < 32; j++) {
            sgb_attr attr = sgb_frame_attrs[i*32+j];
            int tile_index = (attr.tile & 0xff);
            int pal_index = (attr.pal & 0x3);
            ASSERT(sgb_mode == true);
            for(int c = 1; c < 16; c++) {
                uint8_t r = sgb_frame_pals[pal_index].col[c].red * 8;
                uint8_t g = sgb_frame_pals[pal_index].col[c].green * 8;
                uint8_t b = sgb_frame_pals[pal_index].col[c].blue * 8;
                palette[c] = SDL_MapRGBA(sgb_border->format, r, g, b, 255);
            }
            bool xflip = attr.xflip;
            bool yflip = attr.yflip;
            for(int y = 0; y < 8; y++) {
                int y_coord = y;
                if(yflip) y_coord = 7 - y;
                for(int x = 0; x < 8; x++) {
                    int x_coord = x;
                    if(xflip) x_coord = 7 - x;
                    int tiledat_index = tile_index * 64 + 8 * y_coord + x_coord;
                    ASSERT(tiledat_index >= 0 && tiledat_index < 256*64);
                    int pixel_index = i * 2048 + y * 256 + j * 8 + x;
                    ASSERT(pixel_index >= 0 && pixel_index < 256*224);
                    uint8_t col = sgb_tiles[tiledat_index];
                    ASSERT(col >= 0 && col < 16);
                    //printf("%x ", col);
                    pixels[pixel_index] = palette[col];
                }
            }
        }
    }
    if(sgb_texture) {
        SDL_DestroyTexture(sgb_texture);
        sgb_texture = NULL;
    }
    sgb_texture = SDL_CreateTextureFromSurface(renderer, sgb_border);
}

//Interprets current rendering state and VRAM and simulates sending it to the SGB
void lcd::interpret_vram(Arr<uint8_t, 4096>& vram_data) {
    ASSERT(bg_scroll_x == 0);
    ASSERT(bg_scroll_y == 0);
    int map_base = 0x1800 + 0x400 * control.bg_map;
    bool signed_addr = !control.tile_addr_mode;
    for(int tile=0;tile<256;tile++) {
        int bgx=tile%SCREEN_TILE_WIDTH;
        int bgy=tile/SCREEN_TILE_WIDTH;
        int map_index=map_base+bgy*32+bgx;
        int tile_num=vram[vram_bank][map_index];
        if(signed_addr) tile_num = 256 + int8_t(tile_num);
        memcpy(&(vram_data[tile*16]), &(vram[vram_bank][tile_num*16]), 16);
    }
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
        if(!success) fprintf(stderr, "Failed to resize screen to %dx%d?\n", cur_x_res, cur_y_res);
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
        sgb_border = SDL_CreateRGBSurface(0,256,224,32,0,0,0,0);
        sgb_border = SDL_ConvertSurface(sgb_border, SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), 0);
        SDL_SetSurfaceBlendMode(sgb_border, SDL_BLENDMODE_BLEND);

        SDL_Rect center{48,40,SCREEN_WIDTH,SCREEN_HEIGHT};
        SDL_FillRect(sgb_border, NULL, SDL_MapRGBA(sgb_border->format, 0,0,0,255));
        SDL_FillRect(sgb_border, &center, SDL_MapRGBA(sgb_border->format, 255,0,0,0));

        if(sgb_texture) {
            SDL_DestroyTexture(sgb_texture);
            sgb_texture = NULL;
        }
        sgb_texture = SDL_CreateTextureFromSurface(renderer, sgb_border);

        ASSERT(sgb_frame_pals.size() == 4);
    }
    else if(switched) {
        cur_x_res = SCREEN_WIDTH;
        cur_y_res = SCREEN_HEIGHT;
        win_x_res = 320;
        win_y_res = 288;
        bool success = util::reinit_sdl_screen(&screen, &renderer, &texture, cur_x_res, cur_y_res);
        if(!success) fprintf(stderr, "Failed to resize screen to %dx%d?\n", cur_x_res, cur_y_res);

        if(overlay) {
            SDL_FreeSurface(overlay);
            overlay = NULL;
        }
        overlay = SDL_CreateRGBSurface(0,SCREEN_WIDTH,SCREEN_HEIGHT,32,0,0,0,0);

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

void lcd::set_window_title(std::string& title) {
    SDL_SetWindowTitle(screen, title.c_str());
}

void lcd::win_resize(unsigned int new_x, unsigned int new_y) {
    win_x_res = new_x;
    win_y_res = new_y;
}

std::string lcd::lcd_to_string(uint16_t addr, uint8_t val) {
    std::string out("");
    if(addr >= 0x8000 && addr < 0xa000) {
        out = std::string("vram[")+util::to_hex_string(addr,4)+"] = 0x"+util::to_hex_string(val,2);
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        uint8_t spr_num = (addr&0xff)/4;
        char name[][10] = {"ypos","xpos","tile","flags"};
        uint8_t spr_type = addr % 4;
        out = std::string("oam[")+util::to_hex_string(addr,4)+"] ("+name[spr_type]+" for #" + std::to_string(spr_num) + ") = 0x"+util::to_hex_string(val,2);
    }
    else {
        switch(addr) {
            case 0xff40:
                out = std::string("display: ") + std::to_string(cpu_control.display_enable) +
                      std::string(" background: ") + std::to_string(cpu_control.priority) +
                      std::string(" bg map: ") + util::to_hex_string(0x9800 + cpu_control.bg_map * 0x400, 4) +
                      std::string(" tile addr mode: ") + ((cpu_control.tile_addr_mode)?"unsigned":"signed")+
                      std::string(" window: ") + std::to_string(cpu_control.window_enable) +
                      std::string(" win map: ") + std::to_string(cpu_control.window_map) +
                      std::string(" sprites: ") + std::to_string(cpu_control.sprite_enable) +
                      std::string(" bigsprites: ") + std::to_string(cpu_control.sprite_size);
                break;
                /*
            case 0xff42:
                break;
            case 0xff43:
                break;
            case 0xff46:
                break;
            case 0xff47:
                break;
            case 0xff48:
                break;
            case 0xff49:
                break;
            case 0xff4a:
                break;
            case 0xff4b:
                break;*/
            default:
                out = util::to_hex_string(addr,4) + " = 0x" + util::to_hex_string(val, 2);
                //Various data aren't necessary to render the screen, so we ignore anything like interrupts and status
                break;
        }
    }
    return out;
}

void lcd::cgb_enable(bool cgb_render) {
    cgb_mode = true;
    cgb_bgpal_index = 0;  //ff68 bits 0-6
    cgb_bgpal_advance = false;   //ff68 bit 7
    cgb_objpal_index = 0; //ff6a bits 0-6
    cgb_objpal_advance = false;  //ff6a bit 7

    cpu_cgb_bgpal_index = 0;  //ff68 bits 0-6
    cpu_cgb_bgpal_advance = false;  //ff6a bit 7
    cpu_cgb_objpal_index = 0; //ff6a bits 0-6
    cpu_cgb_objpal_advance = false;  //ff6a bit 7

    if(cgb_render) {
        render = std::bind(&lcd::cgb_render, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    }
}
