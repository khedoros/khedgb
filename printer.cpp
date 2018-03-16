#include<cstdio>
#include "printer.h"

printer::printer() {
    state = MAGIC1;
}

uint8_t printer::send(uint8_t incoming) {
    uint8_t outval = 0;
    printf("Printer got %02x");//, sending 0.\n");
    switch(state) {
        case MAGIC1:
            if(incoming == 0x88) state = MAGIC2;
            break;
        case MAGIC1:
            if(incoming == 0x33) state = COMMAND;
            else                 state = MAGIC1;
            break;
        case COMMAND:
            command = incoming;
            state = COMPRESS;
            break;
        case COMPRESS:
            compression = (incoming & 1);
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
            state = DATA;
            break;
        case DATA:
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
                    printf("Print, margins(start: %d end: %d), palette(%d, %d, %d, %d), exposure: %d", start_margin, end_margin, p.pal[0], p.pal[1], p.pal[2], p.pal[3], exposure);
                    print_buffer();
                    break;
                case GRAPHICS:
                    for(int i=0;i<cmd_length&&i+graphics_ptr<8192;i++) graphics_buffer[graphics_ptr + i] = data_buffer[i];
                    graphics_ptr+=i;
                    if(graphics_ptr >= 0x27f) status = 8;
                    break;
                case READ_STATUS:
                    outval = status;
                    if(status==2 && print_start_time + 10000 < SDL_GetTicks()) outval|=2; //pretend that it takes 10 seconds to print
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
    return outval;
}

void printer::print_buffer() {
    for(int i=0;i<graphics_ptr;i++) {
        printf("%02x ", graphics_buffer[i]);
        if(i+i % 40 == 0) printf("\n");
    }
    printf("\n");
}
