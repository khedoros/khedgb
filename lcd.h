#pragma once

#include<cstdint>
//#include<vector>
#include "util.h"

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
    void write(int addr, void * val, int size, int cycle);
    void read(int addr, void * val, int size, int cycle);
    void render(int frame);
    bool interrupt_triggered(uint32_t frame, uint32_t cycle);
private:
    Vect<uint8_t> vram;
    union dmgpal {
        struct {
            unsigned p0:2;
            unsigned p1:2;
            unsigned p2:2;
            unsigned p3:2;
        };
        uint8_t pal;
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

    control_reg control; //0xff40
    uint8_t status;      //0xff41
    uint8_t bg_scroll_y; //0xff42
    uint8_t bg_scroll_x; //0xff43
    uint8_t lyc;         //0xff45

    dmgpal bgpal;        //0xff47
    dmgpal obj1pal;      //0xff48
    dmgpal obj2pal;      //0xff49

    uint8_t win_scroll_y;//0xff4a
    uint8_t win_scroll_x;//0xff4b

    uint32_t lyc_last_frame;
    uint32_t lyc_last_line;
    uint32_t m1_last_frame;
    uint32_t m2_last_line;
    uint32_t m2_last_frame;
    uint32_t m0_last_line;
    uint32_t m0_last_frame;
};
