#pragma once
#include<cstdint>
#include "util.h"

class printer {
public:
    printer();
    uint8_t send(uint8_t val);
private:
    void print_buffer();

    enum p_state {
        MAGIC1,
        MAGIC2,
        COMMAND,
        COMPRESS,
        LENGTH1,
        LENGTH2,
        DATA,
        CHECK1,
        CHECK2,
        ALIVE,
        STATUS
    };

    enum commands {
        INIT=1,
        PRINT,
        GRAPHICS=4,
        READ_STATUS=15
    };

    struct dmgpal {
        uint8_t pal[4];
    };

    p_state state;
    uint8_t command;
    uint16_t cmd_length;
    uint16_t data_ptr;
    uint16_t graphics_ptr;
    uint16_t checksum; //Running checksum of received data
    uint16_t check_data; //checksum number received from connection
    bool compression;
    Vect<uint8_t> data_buffer;
    Arr<uint8_t, 8192> graphics_buffer;
    dmgpal p;
    uint8_t start_margin;
    uint8_t end_margin;
    uint8_t exposure;
    uint8_t status;
    uint64_t print_start_time;
};
