#pragma once
#include<cstdint>
#include "util.h"
#include<opencv2/opencv.hpp>

class camera {
public:
    camera();
    void capture(uint64_t cycle, uint8_t * destination); //captures to array at "destination"
    void write(uint32_t addr, uint8_t val, uint64_t cycle);
    uint8_t read(uint16_t addr, uint64_t cycle);
    ~camera();

private:
    const uint32_t GB_CAM_WIDTH = 128;
    const uint32_t GB_CAM_HEIGHT = 128; //Bottom 5 lines are corrupted, GameBoy cuts off the top+bottom 8 lines anyhow.
    cv::VideoCapture cap;
    bool valid;
    uint64_t capture_start_cycle;  //Last time a capture was started
    uint64_t capture_length;  //How long it will take, calculated when the transfer was started
    uint8_t trigger_register; //a000: writing 1 to bit 0 triggers capture. returns 1 when hardware is working, and 0 when it's done. Writing 0 stops an in-progress read.
    uint8_t parameters[5]; //a001: maps to register 1, controls output gain and edge operation mode
                           //a002,3: maps to registers 2+3, control exposure time. a003 is lsb. 
                           //a004: maps to register 7, controls output voltage reference, edge enhancement ratio, can invert the image
                           //a005: maps to register 0. sets output reference voltage and enables 0-point calculation.
    uint8_t matrix[48];    //a006-a035: 4x4 matrix, 3 bytes per element. handle dithering and contrast. The matrix is repeated across the screen, in 4x4 pixel groups.
    //a006 is the 0,0 low threshold, a007 is the 0,0 med thresh, a008 is 0,0 high, etc, left-to-right, top-to-bottom.
    //Exposure times (in 1MHz cycles):
    //N_bit = ([A001] & BIT(7)) ? 0 : 512
    //exposure = ([A002]<<8) | [A003]
    //CYCLES = 32446 + N_bit + 16 * exposure
    //
    //Other timings (in 512KHz cycles):
    //Reset pulse.
    //Configure sensor registers.    (11 x 8 CLKs)
    //Wait                           (1 CLK)
    //Start pulse                    (1 CLK)
    //Exposure time                  (exposure_steps x 8 CLKs)
    //Wait                           (2 CLKs)
    //Read start
    //Read period                    (N=1 ? 16128 : 16384 CLKs)
    //Read end
    //Wait                           (3 CLKs)
    //Reset pulse to stop the sensor 
    //(88 + 1 + 1 + 2 + 16128 + 3 = 16223)
    //CLKs = 16223 + ( N_bit ? 0 : 256 ) + 8 * exposure
    //
    //Beginning and ending rows are corrupted, so the ROM ignores the first and last 8 rows of the image. So it captures 128x128, but keeps 128x112
    //
};
