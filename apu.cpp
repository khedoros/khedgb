#include "apu.h"
#include<cstdio>

apu::apu() {}

void apu::write(uint16_t addr, uint8_t val, uint64_t cycle) {
    //printf("addr: %04x val: %02x ", addr, val);

}

uint8_t apu::read(uint16_t addr, uint64_t cycle) {
    return 0xff;
}

void apu::run(uint64_t run_to) {
}

void apu::apply(util::cmd& c) {
    switch(c.addr) {
        //sound 1: rectangle with sweep + envelope
        case 0xff10: //sound 1 sweep
            //printf("apu: S1 sweep time: %d increase: %d shift: %d\n", (val&0x70)>>4, (val&8)>>3, (val&7));
            break;
        case 0xff11: //sound 1 length + duty cycle
            //printf("apu: S1 duty cycle: %d length: %d\n", val>>6, (val&0x3f));
            break;
        case 0xff12: //sound 1 envelope
            //printf("apu: S1 default envelope: %d up/down: %d step length: %d\n", val>>4, (val&8)>>3, (val&7));
            break;
        case 0xff13: //sound 1 low-order frequency
            //printf("apu: S1 freq-low: %d\n", val);
            break;
        case 0xff14: //sound 1 high-order frequency + start
            //printf("apu: S1 start: %d freq-high: %d\n", val>>7, (0x03&val));
            break;

        //sound 2: rectangle with envelope
        case 0xff16: //sound 2 length + duty cycle
            //printf("apu: S2 duty cycle: %d length: %d\n", val>>6, (val&0x3f));
            break;
        case 0xff17: //sound 2 envelope
            //printf("apu: S2 default envelope: %d up/down: %d step length: %d\n", val>>4, (val&8)>>3, (val&7));
            break;
        case 0xff18: //sound 2 low-order frequency
            //printf("apu: S2 freq-low: %d\n", val);
            break;
        case 0xff19: //sound 2 high-order frequency + start
            //printf("apu: S2 start: %d freq-high: %d\n", val>>7, (0x03&val));
            break;

        //sound 3: arbitrary waveform
        case 0xff1a: //sound 3 start/stop
            //printf("apu: S3 enable: %d\n", val>>7);
            break;
        case 0xff1b: //sound 3 length
            //printf("apu: S3 length: %d\n", val);
            break;
        case 0xff1c: //sound 3 output level
            //printf("apu: S3 level: %d\n", (val&0x60)>>5);
            break;
        case 0xff1d: //sound 3 low-order frequency
            //printf("apu: S3 freq-low: %d\n", val);
            break;
        case 0xff1e: //sound 3 high-order frequency + start
            //printf("apu: S3 start: %d freq-high: %d\n", val>>7, (0x03&val));
            break;

        //sound 4: noise
        case 0xff20: //sound 4 length
            //printf("apu: S4 length: %d\n", val&0x3f);
            break;
        case 0xff21: //sound 4 envelope
            //printf("apu: S4 default envelope: %d up/down: %d step length: %d\n", val>>4, (val&0x08)>>3, (val&7));
            break;
        case 0xff22: //sound 4 frequency settings
            //printf("apu: S4 shift freq: %d counter steps: %d freq division ratio: %d\n", val>>4, (val&0x08)>>3, (val&7));
            break;
        case 0xff23: //sound 4 start
            //printf("apu: S4 start: %d counter/continuous: %d\n", val>>7, (val&0x40)>>6);
            break;

        //audio control
        case 0xff24: //V-in output levels+channels
            //printf("apu: Vin->SO2: %d, S02 level: %d, Vin->SO1: %d, S01 level: %d\n", val>>7, (val&0x70)>>4, (val&0x08)>>3, (val&0x07));
            break;
        case 0xff25: //Left+right channel selections
            //printf("apu: S1->SO2: %d S2->S02: %d S3->S02: %d S4->S02: %d S1->SO1: %d S2->S01: %d S3->S01: %d S4->S01: %d\n", val>>7, (val&0x40)>>6, (val&0x20)>>5, (val&0x10)>>4,
                                                                                                                             //(val&0x0f)>>3, (val&0x04)>>2, (val&0x02)>>1, (val&0x01));
            break;
        case 0xff26: //Sound activation
            //printf("apu: master: %d S4-on: %d S3-on: %d S2-on: %d S1-on: %d\n", val>>7, (val&0x08)>>3, (val&0x04)>>2, (val&0x02)>>1, (val&0x01));
            break;

        default:
            if(c.addr >= 0xff30 && c.addr < 0xff40) {
                //printf("apu: wave[%d] = %d, wave[%d] = %d\n", (addr-0xff30)*2, (val&0xf0)>>4, (addr-0xff30)*2+1, val&0x0f);
                //waveform RAM
            }
            else {
                //printf("apu: unknown, addr=0x%04x, val=0x%02x\n", addr, val);
            }
    }
}
