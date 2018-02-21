#include<cstdint>
#include<cstdio>

struct sprite {
    uint8_t x;
    uint8_t y;
};

struct fifo_slot {
    uint8_t color;
    uint8_t source;
};

struct lcd_control {
    unsigned priority:1;
    unsigned sprite_enable:1;
    unsigned sprite_size:1;
    unsigned bg_map:1;
    unsigned tile_addr_mode:1;
    unsigned window_enable:1;
    unsigned window_map:1;
    unsigned display_enable:1;
};

struct scroll {
    uint8_t x;
    uint8_t y;
};

lcd_control control;
uint8_t tiles[0x1800]; //0x8000-0x97FF
uint8_t bg_maps[0x800]; //0x9800-0x9FFF
sprite sprites[40]; //OAM, 0xFE00-0xFE9F


uint8_t sprite_slots[10]; //theoretical registers to hold sprites chosen for the line

uint8_t output_line[160]; //The line as it appears in the LCD display

//Finds the first 10 sprites that will appear on the line
//Takes about 20 CPU clocks (40 PPU memory access cycles), which makes sense if it doesn't stop reading once the slots are full
void find_sprites(int line) {
    int slot_index = 0;
    //Produces sprites in range, from low to high sprite numbers
    for(int sprite=0;sprite<40;sprite++) {
        int y = sprites[sprite].y
        if(y <= line && y + 8 + 8*control.sprite_size > line && slot_index < 10) {
            sprite_slots[slot_index++] = sprite;
        }
    }
    //Clear any extra slots
    for(index=slot_index;index<10;index++) sprite_slots[index] = -1;
}


int render_tiles(int line) {
    int cycles = 0; //cycles spent doing the rendering
    uint8_t output_level = 0; //
    fifo_slot fifo[16]; //theoretical fifo slot to aid in rendering
    uint8_t fifo_level = 0; //how many pixels the fifo has loaded

    //Tile info to be fetched
    uint8_t tile = 0;
    uint8_t int byte0 = 0;
    uint8_t int byte1 = 0;
    bool doing_window = false; //doing bg when not doing window

    while(output_level < 160) { //While not done drawing the line
        while(fifo_level <= 8) { //While there's 8 or more free pixels in the fifo
        }
    }
    return cycles;
}

int main() {
    for(int line=0;line<144;line++) {
        find_sprites(line); //20 CPU cycles, 40 VRAM cycles
        int cycles = render_tiles(line);
    }
}
