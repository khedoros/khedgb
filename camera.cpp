#include "camera.h"
#include<cstdio>

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT //assert
#endif

camera::camera() : cap(0), valid(false), capture_start_cycle(0), capture_length(0),
                   screen_raw(NULL), screen_processed(NULL), renderer_raw(NULL), buffer_raw(NULL),
                   buffer_processed(NULL), texture_raw(NULL), texture_processed(NULL), renderer_processed(NULL)
{
    if(cap.isOpened()) {
        valid = true;
    }

    /*
    util::reinit_sdl_screen(&screen_raw, &renderer_raw, &texture_raw, 128, 128);
    util::reinit_sdl_screen(&screen_processed, &renderer_processed, &texture_processed, 128, 128);

    buffer_raw = SDL_CreateRGBSurface(0,128,128,32,0,0,0,0); 
    buffer_processed = SDL_CreateRGBSurface(0,128,128,32,0,0,0,0); 
    
    SDL_SetRenderDrawColor(renderer_raw, 0, 0, 0, 0);
    SDL_RenderClear(renderer_raw);
    SDL_RenderPresent(renderer_raw);
    SDL_SetRenderDrawColor(renderer_processed, 0, 0, 0, 0);
    SDL_RenderClear(renderer_processed);
    SDL_RenderPresent(renderer_processed);
    */
}

camera::~camera() {
    if(valid) {
        cap.release();
    }
}

void camera::capture(uint64_t cycle, uint8_t * camera_frame) {

    capture_start_cycle = cycle;

    capture_length = 32446 + ((parameters[0] & BIT7) ? 0 : 512) + 16 * ((((uint16_t)parameters[1])<<8)+parameters[2]);

    cv::Mat image;
    cv::Mat gray_image;
    cap>>image;
    cv::Mat sub_image;
    int height = image.size().height;
    int width = image.size().width;
    //cv::cvtColor(image, gray_image, CV_BGR2GRAY);
    cv::cvtColor(image, gray_image, cv::COLOR_BGR2GRAY);
    cv::getRectSubPix(gray_image, cv::Size{(2*height)/3, (2*height)/3}, cv::Point2f{float(width)/float(2),float(height)/float(2)}, sub_image);
    cv::Mat shrunk_image;
    cv::resize(sub_image, shrunk_image, cv::Size{128,128}, 0, 0);

    height = shrunk_image.size().height;
    width = shrunk_image.size().width;
    ASSERT(height == 128 && width == 128);
    ASSERT(shrunk_image.rows == 128 && shrunk_image.cols == 128);

    //uint32_t * pix_r = ((uint32_t *)buffer_raw->pixels);
    //uint32_t * pix_p = ((uint32_t *)buffer_processed->pixels);

    uint32_t exposure = uint32_t(((parameters[1])<<8)) + parameters[2];

    for(int i=0;i<128;i++) {
        for(int j=0;j<128;j++) {
            uint32_t val = shrunk_image.at<uint8_t>(i,j);
            //pix_r[i*128+j] = SDL_MapRGB(buffer_raw->format, val, val, val);
            val = util::clamp(0,(exposure*val)/2048,255);
            val = util::clamp(0,val,255);
            uint8_t matrix_base = (i%4)*12 + (j%4)*3;
            uint8_t l = matrix[matrix_base];
            uint8_t m = matrix[matrix_base+1];
            uint8_t h = matrix[matrix_base+2];
            if(val < l) val = 3;
            else if(val < m) val = 2;
            else if(val < h) val = 1;
            else val = 0;
            //pix_p[i*128+j] = SDL_MapRGB(buffer_processed->format, (3-val)*80, (3-val)*80, (3-val)*80);
            camera_frame[i*128+j] = val;
        }
    }

    /*
    if(texture_raw) {
        SDL_DestroyTexture(texture_raw);
    }
    texture_raw = SDL_CreateTextureFromSurface(renderer_raw, buffer_raw);
    SDL_RenderCopy(renderer_raw, texture_raw, NULL, NULL);
    SDL_RenderPresent(renderer_raw);

    if(texture_processed) {
        SDL_DestroyTexture(texture_processed);
    }
    texture_processed = SDL_CreateTextureFromSurface(renderer_processed, buffer_processed);
    SDL_RenderCopy(renderer_processed, texture_processed, NULL, NULL);
    SDL_RenderPresent(renderer_processed);
    */
}

void camera::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    addr = ((addr - 0xa000) & 0x7f);
    switch(addr) {
        case 0: //trigger register, and lookup for camera registers 4, 5, and 6. A000
            trigger_register = (val & 0x6);
            break;
        case 1: //Register 1, NVHGGGGG N=vert edge, VH=v/h edge, G=analog output gain, GB likes 0,4,8,a, for 5x,10x,20x,40x enhancement A001
            parameters[0] = val;
            break;
        case 2: //MSB of exposure time A002
            parameters[1] = val;
            //printf("Exposure: %d uS\n", 16 * (uint32_t(parameters[1])<<8 | parameters[2]));
            break;
        case 3: //LSB of exposure time. Each unit is roughly 16uS A003
            parameters[2] = val;
            //printf("Exposure: %d uS\n", 16 * (uint32_t(parameters[1])<<8 | parameters[2]));
            break;
        case 4: //EEEEIVVV E=edge enhancement ratio, I=inverted/non V=output node bias voltage
            parameters[3] = val;
            break;
        case 5: //ZZOOOOOO Z=zero point calibration, O=output reference voltage range
            parameters[4] = val;
            break;
        default:
            if(addr > 5 && addr < 0x36) {
                matrix[addr - 6] = val;
                //printf("Set matrix %d to %d\n", addr-6, val);
            }
            else {
                //printf("Camera received write request for unknown address: %02x = %02x\n", addr, val);
            }
    }
}

uint8_t camera::read(uint16_t addr, uint64_t cycle) {
    if(addr == 0xa000) {
        if(cycle >= capture_start_cycle + capture_length) return trigger_register;
        else return (trigger_register | 1);
    }
    return 0;
}
