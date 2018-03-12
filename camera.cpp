#include "camera.h"
#include<cstdio>

camera::camera() : cap(0), valid(false), capture_start_cycle(0), capture_length(0) {
    if(cap.isOpened()) {
        valid = true;
    }
    //bool success = cap.set(cv::CAP_PROP_FRAME_WIDTH, 128);
    //if(!success) printf("Setting camera width returned false\n");
    //success = cap.set(cv::CAP_PROP_FRAME_HEIGHT, 128);
    //if(!success) printf("Setting camera height returned false\n");
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
    cv::getRectSubPix(image, cv::Size{height, height}, cv::Point2f{float(height/2),float(width/2)}, sub_image);
    cv::cvtColor(sub_image, gray_image, CV_BGR2GRAY);
    cv::Mat shrunk_image;
    cv::resize(sub_image, shrunk_image, cv::Size{128,128}, 0, 0);

    height = shrunk_image.size().height;
    width = shrunk_image.size().width;
    //printf("CAMERA: h: %d w: %d\n", height, width);
    assert(height == 128 && width == 128);
    assert(shrunk_image.rows == 128 && shrunk_image.cols == 128);

    for(int i=8;i<120;i++) {
        for(int j=0;j<128;j++) {
            uint8_t val = shrunk_image.at<uint8_t>(i,j);
            uint8_t matrix_base = (i%4)*12 + (j%4)*3;
            uint8_t l = matrix[matrix_base];
            uint8_t m = matrix[matrix_base];
            uint8_t h = matrix[matrix_base];
            if(val < l) val = 3;
            else if(val < m) val = 2;
            else if(val < h) val = 1;
            else val = 0;
            camera_frame[(i-8)*128+j] = val;
        }
    }
}

void camera::write(uint32_t addr, uint8_t val, uint64_t cycle) {
    addr = ((addr - 0xa000) & 0x7f);
    switch(addr) {
        case 0: //trigger register, and lookup for camera registers 4, 5, and 6.
            trigger_register = (val & 0x6);
            break;
        case 1: //Register 1, NVHGGGGG N=vert edge, VH=v/h edge, G=analog output gain, GB likes 0,4,8,a, for 5x,10x,20x,40x enhancement
            parameters[0] = val;
            break;
        case 2: //MSB of exposure time
            parameters[1] = val;
            break;
        case 3: //LSB of exposure time. Each unit is roughly 16uS
            parameters[2] = val;
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
            }
            else {
                printf("Camera received write request for unknown address: %02x = %02x\n", addr, val);
            }
    }
}

uint8_t camera::read(uint16_t addr, uint64_t cycle) {
    if(addr == 0xa000) {
        if(cycle >= capture_start_cycle + capture_length) return 0;
        else return 1;
    }
    return 0;
}
