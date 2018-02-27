#pragma once

#include<cstdint>
#include<SDL2/SDL.h>
#include "util.h"
#include<list>
#include<string>

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
    ~lcd();
    void write(int addr, void * val, int size, uint64_t cycle);
    void read(int addr, void * val, int size, uint64_t cycle);
    bool interrupt_triggered(uint64_t cycle);
    uint64_t get_active_cycle();
    uint64_t run(uint64_t cycle_count);
    uint8_t get_mode(uint64_t cycle, bool ppu_view = false);
    void set_debug(bool db);
    void toggle_debug();
    uint64_t get_frame();
    void dma(bool dma_active, uint64_t cycle, uint8_t dma_addr);

    void sgb_trigger_dump(std::string filename);
    void sgb_set_pals(uint8_t pal1, uint8_t pal2, Vect<uint16_t>& colors);
    void sgb_vram_transfer(uint8_t type);
    void sgb_set_mask_mode(uint8_t mode);
    uint8_t sgb_vram_transfer_type;

private:
    void update_estimates(uint64_t cycle);
    void apply(int addr, uint8_t val, uint64_t index, uint64_t cycle);
    uint64_t render(int frame, uint64_t start_cycle, uint64_t end_cycle);
    void get_tile_row(int tilenum, int row, bool reverse, Vect<uint8_t>& pixels);
    uint32_t map_bg_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);
    uint32_t map_win_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);
    uint32_t map_oam1_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);
    uint32_t map_oam2_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);

    void draw_debug_overlay();

    bool debug;
    bool during_dma;
    bool cpu_during_dma;

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

    struct oam_data {
        uint8_t ypos;
        uint8_t xpos;
        uint8_t tile;
        unsigned palnum_cgb:3; //CGB only
        unsigned tilebank:1; //CGB only
        unsigned palnum_dmg:1; //DMG only
        unsigned xflip:1;
        unsigned yflip:1;
        unsigned priority:1; //0=above BG, 1=behind BG colors 1-3
    };

    //Needed for rendering, so must be mirrored to "catch up" with the CPU's view of the PPU state
    std::list<util::cmd> cmd_queue; //List of commands to catch up PPU with CPU
    std::list<util::cmd> timing_queue; //Abusing the util::cmd type to store line, cycle, and whether the access was a write to oam, vram, or some other control register
    Vect<uint8_t> vram;
    Vect<uint8_t> oam;
    uint64_t cycle;      //Last cycle executed during previous invocation of "lcd::run()"
    uint64_t next_line; //Next line to render in frame
    control_reg control; //0xff40
    uint8_t bg_scroll_y; //0xff42
    uint8_t bg_scroll_x; //0xff43

    dmgpal bgpal;        //0xff47
    dmgpal obj1pal;      //0xff48
    dmgpal obj2pal;      //0xff49

    Vect<uint32_t> sys_bgpal;
    Vect<uint32_t> sys_winpal;
    Vect<uint32_t> sys_obj1pal;
    Vect<uint32_t> sys_obj2pal;

    uint8_t win_scroll_y;//0xff4a
    uint8_t win_scroll_x;//0xff4b
    uint64_t active_cycle; //Most recent cycle that the display was enabled
    uint64_t frame;      //current frame count



    /*
        0 201-207 clks, 2 about 77-83 clks, and 3 about 169-175 clks
        1 is 4560 clks

        (0=51, 2=20, 3=43)*114, 1=1140
        0(hblank)=7344 cycles/frame
        1(vblank)=1140 cycles/frame
        2(oam=ro)=2880 cycles/frame
        3(vramro)=6192 cycles/frame
    */

    //Needed for proper responses to the CPU. A lot of these are mirrored versions of the PPU-view registers.
    uint64_t lyc_next_cycle; //cycle of next time to trigger lyc interrupt
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
    SDL_Texture * prev_texture; //Texture used for output

    SDL_Surface * overlay; //overlay surface (if I want to render non-game info)
    SDL_Surface * lps; //Low-priority sprite compositing buffer
    SDL_Surface * hps; //High-priority sprite compositing buffer

    SDL_Surface * bg1; //Background map 1 compositing buffer
    SDL_Surface * bg2; //Background map 2 compositing buffer


    //Super GameBoy-related structs and values
    std::string sgb_dump_filename;

    union sgb_color {
        struct {
            unsigned red: 5;
            unsigned green: 5;
            unsigned blue: 5;
            unsigned unused:1;
        };
        struct {
            uint8_t low;
            uint8_t high;
        };
        uint16_t val;
    };

    union sgb_pal {
        sgb_color col[4];
    };

    union sgb_attr {
        struct {
            unsigned tile:10;
            unsigned pal:3;
            unsigned priority:1;
            unsigned xflip:1;
            unsigned yflip:1;
        };
        struct {
            uint8_t low;
            uint8_t high;
        };
        uint16_t val;
    };

    bool sgb_mode; //Should I activate SGB mode?
    //Vect<sgb_pal>  sgb_pals[8]; //8 visible palettes, actually, I'll reuse the palette entries I'm already using for translation to SDL colors anyhow
    Vect<sgb_pal>  sgb_sys_pals[512]; //set of 512 palettes that can be copied into the visible ones. These will need translated to SDL colors when used.
    Vect<uint8_t>  sgb_attrs[20*18]; //palette selections for the main GB display window. These will act as indices into the SDL palette table.
    Vect<sgb_attr> sgb_frame_attrs[32*32]; //tilemap, palette selections, priorities, etc for window border
    uint8_t sgb_mask_mode; //0=cancel, 1=freeze, 2=black, 3=color 0
    Vect<uint8_t> sgb_tiles[256*8*8]; //
    bool sgb_set_low_tiles;
    bool sgb_set_high_tiles;
    bool sgb_set_bg_attr;

};
