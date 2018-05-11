#pragma once

#include<cstdint>
#include<SDL2/SDL.h>
#include "util.h"
//#include<list>
#include<queue>
#include<string>
#include<functional>

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

const static int CYCLES_PER_LINE = 114;
const static int SCREEN_WIDTH = 160;
const static int SCREEN_HEIGHT = 144;
const static int SCREEN_TILE_WIDTH = 20;
const static int SCREEN_TILE_HEIGHT = 18;
const static int LINES_PER_FRAME = 154;
const static int CYCLES_PER_FRAME = CYCLES_PER_LINE * LINES_PER_FRAME;
const static int LAST_RENDER_LINE = SCREEN_HEIGHT - 1;
const static int MODE_2_LENGTH = 20;
const static int MODE_3_LENGTH = 43;
const static int MODE_0_LENGTH = 51;
const static int MODE_1_LENGTH = CYCLES_PER_LINE * (LINES_PER_FRAME - SCREEN_HEIGHT);

class lcd {
public:
    lcd();
    ~lcd();
    void write(int addr, uint8_t val, uint64_t cycle);
    uint8_t read(int addr, uint64_t cycle);
    bool interrupt_triggered(uint64_t cycle);
    uint64_t get_active_cycle();
    uint64_t run(uint64_t cycle_count);
    uint8_t get_mode(uint64_t cycle, bool ppu_view = false);
    void set_debug(bool db);
    void toggle_debug();
    void toggle_trace();
    uint64_t get_frame();
    void dma(bool dma_active, uint64_t cycle, uint8_t dma_addr);
    void win_resize(unsigned int new_x, unsigned int new_y);
    void set_window_title(std::string&);

    void sgb_trigger_dump(std::string filename);
    void sgb_set_pals(uint8_t pal1, uint8_t pal2, Arr<uint16_t, 7>& colors);
    void sgb_vram_transfer(uint8_t type);
    void sgb_set_mask_mode(uint8_t mode);
    void sgb_enable(bool enable);
    void cgb_enable(bool dmg_render);
    void sgb_set_attrs(Arr<uint8_t, 360>& attrs);
    void sgb_set_attrib_from_file(uint8_t attr_file, bool cancel_mask);
    void sgb_pal_transfer(uint16_t pal0, uint16_t pal1, uint16_t pal2, uint16_t pal3, uint8_t attr_file, bool use_attr, bool cancel_mask);

private:
    void update_estimates(uint64_t cycle);
    void apply(int addr, uint8_t val, uint64_t cycle);
    uint64_t dmg_render(int frame, uint64_t start_cycle, uint64_t end_cycle);
    uint64_t cgb_render(int frame, uint64_t start_cycle, uint64_t end_cycle);
    std::function<uint64_t(int, uint64_t, uint64_t)> render;
    void get_tile_row(int tilenum, int row, bool reverse, Arr<uint8_t, 8>& pixels, int bank=0);
    uint32_t map_bg_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);
    uint32_t map_win_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);
    uint32_t map_oam1_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);
    uint32_t map_oam2_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255);
    void do_vram_transfer();
    void interpret_vram(Arr<uint8_t, 4096>& vram_data);
    void regen_background();
    std::string lcd_to_string(uint16_t addr, uint8_t val);
    void update_row_cache(uint16_t,uint8_t);

    void draw_debug_overlay();

    bool cgb_mode;
    bool debug;
    bool during_dma;
    bool cpu_during_dma;

    struct dmgpal {
        uint8_t pal[4];
    };

    union control_reg { //0xFF40
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

    union status_reg { //0xFF41
        struct {
            unsigned mode:2; //0=hblank, 1=vblank, 2=searching oam, 3=transferring to lcd
            unsigned lyc_flag:1;
            unsigned hblank_int:1; //activate mode0 interrupt
            unsigned vblank_int:1; //activate mode1 interrupt
            unsigned oam_int:1;    //activate mode2 interrupt
            unsigned lyc_int:1;    //activate lyc=ly interrupt
            unsigned unused:1;
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
    //std::list<util::cmd> cmd_queue; //List of commands to catch up PPU with CPU
    //std::list<util::timing_data> timing_queue; //Abusing the util::cmd type to store line, cycle, and whether the access was a write to oam, vram, or some other control register
    std::queue<util::cmd> cmd_queue; //List of commands to catch up PPU with CPU
    std::queue<util::timing_data> timing_queue; //Abusing the util::cmd type to store line, cycle, and whether the access was a write to oam, vram, or some other control register
    Arr<Arr<uint8_t, 0x2000>, 2> vram;
    Arr<uint8_t, 0xa0> oam;
    Arr<Arr<uint8_t, 8>, 768*8> row_cache; //pre-calculated cache of tile data
    Arr<Arr<uint8_t, 8>, 256*256> rows; //The 65k possible tile lines
    Arr<uint32_t, 160*144> out_buf; //Graphics output buffer
    uint64_t cycle;      //Last cycle executed during previous invocation of "lcd::run()"
    uint64_t next_line; //Next line to render in frame
    control_reg control; //0xff40
    uint8_t bg_scroll_y; //0xff42
    uint8_t bg_scroll_x; //0xff43

    dmgpal bgpal;        //0xff47
    dmgpal obj1pal;      //0xff48
    dmgpal obj2pal;      //0xff49

    //First dimension is palette number. Second dimension is color number.
    //These are useful for both SGB and GBC palettes. Plain GB just uses the first, SGB uses the first 4, CGB can use all 8
    Arr<Arr<uint32_t, 4>, 8> sys_bgpal;
    Arr<Arr<uint32_t, 4>, 8> sys_winpal;
    Arr<Arr<uint32_t, 4>, 8> sys_obj1pal;
    Arr<Arr<uint32_t, 4>, 4> sys_obj2pal;

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

    Arr<Arr<uint8_t, 0x2000>, 2> cpu_vram;
    Arr<uint8_t, 0xa0> cpu_oam;
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
    int cur_x_res; //Rendering resolution
    int cur_y_res;
    int win_x_res; //Window resolution
    int win_y_res;

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

    SDL_Surface * sgb_border; //Super GameBoy border
    SDL_Texture * sgb_texture; //Texture to store the border in


    //Super GameBoy-related structs and values

    union sgb_color {
        struct {
            unsigned red: 5;
            unsigned green: 5;
            unsigned blue: 5;
            unsigned unused:1;
        } __attribute__((packed));
        struct {
            uint8_t low;
            uint8_t high;
        };
        uint16_t val;
    };

    union sgb_pal4 {
        sgb_color col[4];
    };

    union sgb_pal16 {
        sgb_color col[16];
    };

    union sgb_attr {
        struct {
            unsigned tile:10;
            unsigned pal:3;
            unsigned priority:1;
            unsigned xflip:1;
            unsigned yflip:1;
        } __attribute__((packed));
        struct {
            uint8_t low;
            uint8_t high;
        };
        uint16_t val;
    };

    struct attrib_byte {
        unsigned block3:2;
        unsigned block2:2;
        unsigned block1:2;
        unsigned block0:2;
    } __attribute__((packed));

    union attrib_file {
        attrib_byte block[90];
        uint8_t bytes[90];
    };

    bool sgb_mode; //Is SGB mode active?
    std::string sgb_dump_filename;
    uint8_t sgb_vram_transfer_type;
    uint8_t sgb_mask_mode; //0=cancel, 1=freeze, 2=black, 3=color 0
    Arr<sgb_pal4, 512>  sgb_sys_pals; //set of 512 palettes that can be copied into the visible ones. These will need translated to SDL colors when used.
    Arr<uint8_t, SCREEN_TILE_WIDTH * SCREEN_TILE_HEIGHT>  sgb_attrs; //palette selections for the main GB display window. These will act as indices into the SDL palette table.
    Arr<sgb_attr, 32*32> sgb_frame_attrs; //tilemap, palette selections, priorities, etc for window border
    Arr<sgb_pal16, 4> sgb_frame_pals;
    Arr<uint8_t, 256*8*8> sgb_tiles; //256 8x8 4-bit tiles
    Arr<attrib_file,45> sgb_attr_files; //45 attribute files
    bool trace; //Enable trace output

    uint8_t vram_bank;
    uint8_t cpu_vram_bank;

    union bg_attrib {
        struct {
            unsigned palette:3;
            unsigned char_bank:1;
            unsigned unused:1;
            unsigned xflip:1;
            unsigned yflip:1;
            unsigned priority:1;
        };
        uint8_t val;
    };

    uint8_t cgb_bgpal_index;  //ff68 bits 0-6
    bool cgb_bgpal_advance;   //ff68 bit 7
    uint8_t cgb_objpal_index; //ff6a bits 0-6
    bool cgb_objpal_advance;  //ff6a bit 7

    uint8_t cpu_cgb_bgpal_index;  //ff68 bits 0-6
    bool cpu_cgb_bgpal_advance;  //ff6a bit 7
    uint8_t cpu_cgb_objpal_index; //ff6a bits 0-6
    bool cpu_cgb_objpal_advance;  //ff6a bit 7
    Arr<uint8_t,64> cpu_cgb_bgpal; //ff69-accessed cpu mirror
    Arr<uint8_t,64> cpu_cgb_objpal; //ff6b-accessed cpu mirror

    Arr<Arr<sgb_color,4>,8> cgb_bgpal; //accessed from ff69
    Arr<Arr<sgb_color,4>,8> cgb_objpal; //accessed from ff6b
};
