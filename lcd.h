#pragma once

#include<cstdint>
//#include<vector>
#include<SDL2/SDL.h>
#include "util.h"
#include<list>

/*
 * ff40: LCD control R/W (various settings)
 * ff41: LCD status  R/W (bit 7 unused, bits 3-6 are R/W, 0-2 are RO)
 * ff42: SCY R/W         (background scroll y)
 * ff43: SCX R/W         (background scroll x)
 * ff44: LY RO           (current LCD line)
 * ff45: LYC R/W         (coincident bit/interrupt if LYC==LY)
 * ff46: DMA WO          (transfer start address for 160 bytes of OAM information)
 * ff47: BG PAL R/W      (non-CGB only. 4 entries for 2-bit palette numbers)
 * ff48: OBJ PAL 0 R/W   ('           ' 3 entries for 2-bit palette numbers)
 * ff49: OBJ PAL 1 R/W   ('           ' 3 entries for 2-bit palette numbers)
 * ff4a: WY R/W  (window scroll y)
 * ff4b: WX R/W  (window scroll x - 7)
 */

enum lcd_ints {
    LYC = 0x40,
    M2  = 0x20,
    M1  = 0x10,
    M0  = 0x08
};

class lcd {
public:
    lcd();
    void dump_tiles();
    void write(int addr, void * val, int size, uint64_t cycle);
    void read(int addr, void * val, int size, uint64_t cycle);
    void render(int frame, bool write_file);
    void render_background(int frame);
    bool interrupt_triggered(uint32_t frame, uint64_t cycle);
    uint64_t get_active_cycle();
    uint64_t run(uint64_t cycle_count);

private:
    uint8_t get_mode(uint64_t cycle);
    void update_estimates(uint64_t cycle);

    struct dmgpal {
        uint8_t pal[4];
    };

    union control_reg {
        struct {
            unsigned priority:1;
            unsigned sprite_enable:1;
            unsigned sprite_size:1;
            unsigned bg_map:1;
            unsigned tile_addr_mode:1;
            unsigned window_enable:1;
            unsigned window_map:1;
            unsigned display_enable:1;
        };
        uint8_t val;
    };

    //Needed for rendering, so must be mirrored to "catch up" with the CPU's view of the PPU state
    std::list<util::cmd> cmd_queue; //List of commands to catch up PPU with CPU
    std::vector<std::vector<uint8_t>> cmd_data; //necessary to store a snapshot of DMA data, for example
    Vect<uint8_t> vram;
    Vect<uint8_t> oam;
    control_reg control; //0xff40
    uint8_t bg_scroll_y; //0xff42
    uint8_t bg_scroll_x; //0xff43

    dmgpal bgpal;        //0xff47
    dmgpal obj1pal;      //0xff48
    dmgpal obj2pal;      //0xff49

    uint8_t win_scroll_y;//0xff4a
    uint8_t win_scroll_x;//0xff4b
    uint64_t active_cycle; //Most recent cycle that the display was enabled


    //Needed for proper responses to the CPU. A lot of these are mirrored versions of the PPU-view registers.
    uint64_t lyc_next_cycle; //cycle of next time to trigger lyc interrupt

    /*
        0 201-207 clks, 2 about 77-83 clks, and 3 about 169-175 clks
        1 is 4560 clks

        (0=51, 2=20, 3=43)*114, 1=1140
        0(hblank)=7344 cycles/frame
        1(vblank)=1140 cycles/frame
        2(oam=ro)=2880 cycles/frame
        3(vramro)=6192 cycles/frame
    */

    uint32_t m0_next_cycle;  //cycle of next time to trigger m0 interrupt (h-blank)
    uint32_t m1_next_cycle;  //cycle of next time to trigger m1 interrupt (v-blank)
    uint32_t m2_next_cycle;  //cycle of next time to trigger m2 interrupt (oam access)
    //Modes cycle: (2-3-0) 144 times, then to 1, with about 20, 

    Vect<uint8_t> cpu_vram;
    Vect<uint8_t> cpu_oam;
    control_reg cpu_control; //0xff40
    uint8_t cpu_status;      //0xff41 (store interrupt flags, calculate coincidence and mode flags)
    uint8_t cpu_bg_scroll_y; //0xff42
    uint8_t cpu_bg_scroll_x; //0xff43
    //uint8_t cpu_ly;        //0xff44 (value is calculated, not stored)
    uint8_t cpu_lyc;         //0xff45
    uint8_t cpu_dma_addr;        //0xff46 (executed when written to)
    dmgpal cpu_bgpal;        //0xff47
    dmgpal cpu_obj1pal;      //0xff48
    dmgpal cpu_obj2pal;      //0xff49

    uint8_t cpu_win_scroll_y;//0xff4a
    uint8_t cpu_win_scroll_x;//0xff4b

    uint64_t cpu_active_cycle; //Most recent cycle that the display was enabled



    //All of these have to do with the SDL2 output implementation
    int cur_x_res;
    int cur_y_res;

    SDL_Window * screen;
    SDL_Renderer * renderer;

    SDL_Surface * buffer; //Output buffer
    SDL_Texture * texture; //Texture used for output

    SDL_Surface * overlay; //overlay surface
    SDL_Surface * lps; //Low-priority sprite compositing buffer
    SDL_Surface * bg; //Background compositing buffer
    SDL_Surface * win; //Floating window compositing buffer
    SDL_Surface * hps; //High-priority sprite compositing buffer

};
