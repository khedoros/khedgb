#include<cstdio>
#include<cassert>
#include<SDL2/SDL.h>
#include "printer.h"

printer::printer() {
    state = MAGIC1;
}

uint8_t printer::send(uint8_t incoming) {
    uint8_t outval = 0;
    //printf("Printer starts in state %d, ", state);
    switch(state) {
        case MAGIC1:
            if(incoming == 0x88) {
                state = MAGIC2;
                //printf(", got expected magic1, ");
            }
            else {
                //printf(", got unexpected magic1, not proceeding, ");
            }
            break;
        case MAGIC2:
            if(incoming == 0x33) {
                state = COMMAND;
                //printf(", got expected magic2,");
            }
            else {
                state = MAGIC1;
                //printf(", got unexpected magic2, resetting state, ");
            }
            break;
        case COMMAND:
            command = incoming;
            if(command == 1 || command == 2 || command == 4 || command == 15) {
                //printf(", received known command %d, ", command);
            }
            else {
                printf(", received unknown command %d, ", command);
            }
            state = COMPRESS;
            break;
        case COMPRESS:
            compression = (incoming & 1);
            //printf(", received compression value %d, ", incoming);
            state = LENGTH1;
            break;
        case LENGTH1:
            cmd_length = incoming;
            state = LENGTH2;
            break;
        case LENGTH2:
            cmd_length |= 256 * incoming;
            data_ptr = 0;
            data_buffer.resize(cmd_length);
            //printf(", received 2 bytes of length: %d, ", cmd_length);
            if(cmd_length > 0) {
                state = DATA;
            }
            else { //skip data, if none is included in this command
                state = CHECK1;
            }
            break;
        case DATA:
            //printf(", data[%d] = %02x, ", data_ptr, incoming);
            data_buffer[data_ptr] = incoming;
            data_ptr++;
            if(data_ptr == cmd_length) {
                state = CHECK1;
            }
            break;
        case CHECK1:
            check_data = incoming;
            state = CHECK2;
            break;
        case CHECK2:
            check_data |= 256 * incoming;
            state = ALIVE;
            //printf(", got checksum %04x, ", check_data);
            break;
        case ALIVE:
            if(incoming == 0) state = STATUS;
            else              state = MAGIC1;
            outval = 0x81;
            break;
        case STATUS:
            switch(command) {
                case INIT:
                    graphics_ptr = 0;
                    status = 0;
                    break;
                case PRINT:
                    outval = status;
                    status = 4;
                    print_start_time = SDL_GetTicks();
                    assert(data_buffer.size() == 4);
                    assert(data_buffer[0] == 1);
                    start_margin = (data_buffer[1] & 0xf0)>>4;
                    end_margin = (data_buffer[1] & 0x0f);
                    p.pal[0] = (data_buffer[2] & 0x03);
                    p.pal[1] = (data_buffer[2] & 0x0c)>>2;
                    p.pal[2] = (data_buffer[2] & 0x30)>>4;
                    p.pal[3] = (data_buffer[2] & 0xc0)>>6;
                    exposure = (data_buffer[3] & 0x7f);
                    //printf("Print, margins(start: %d end: %d), palette(%d, %d, %d, %d), exposure: %d", start_margin, end_margin, p.pal[0], p.pal[1], p.pal[2], p.pal[3], exposure);
                    print_buffer();
                    break;
                case GRAPHICS:
                    for(int i=0;i<cmd_length&&i+graphics_ptr<8192;i++) graphics_buffer[graphics_ptr + i] = data_buffer[i];
                    graphics_ptr+=(cmd_length < 8192)?cmd_length:8191-graphics_ptr;
                    if(graphics_ptr >= 0x27f) status = 8;
                    break;
                case READ_STATUS:
                    outval = status;
                    if(status==2 && print_start_time + 10000 > SDL_GetTicks()) outval|=2; //pretend that it takes 10 seconds to print
                    break;
                default:
                    break;
            }
            state = MAGIC1;
            break;
        default:
            state = MAGIC1;
            break;
    }
    //printf("Printer ends in state %d, ", state);
    //printf("Printer got %02x, sending %02x.\n", incoming, outval);
    return outval;
}

void printer::print_buffer() {
    printf("Printer received %d tiles (%d bytes), for an image 160x%d pixels in size:\n", graphics_ptr/16, graphics_ptr, graphics_ptr/40);
    Vect<uint8_t> output(160*graphics_ptr/40, 0);
    for(int tile=0;tile<graphics_ptr/16;tile++) {
        int tile_x = tile % 20;
        int tile_y = tile / 20;
        for(int y=0;y<8;y++) {
            int offset = tile*16+y*2;
            int byte0 = graphics_buffer[offset];
            int byte1 = graphics_buffer[offset+1];
            for(int x=0;x<8;x++) {
                int out = tile_y*1280+y*160+tile_x*8+x;
                output[out] = (byte0&0x80)>>7;
                output[out] |= (byte1&0x80)>>6;
                output[out] = 3 - p.pal[output[out]];
                byte0<<=1;
                byte1<<=1;
            }
        }
    }
    /*
    for(int i=0;i<4*graphics_ptr;i++) {
        printf("%d", output[i]);
        if((i+1)%160 == 0) printf("\n");
    }
    */
    std::string filename = std::to_string(SDL_GetTicks()) + ".png";
    util::output_png(filename, 160,graphics_ptr/40, output);
    printf("Output printer image to %s.\n", filename.c_str());
}
