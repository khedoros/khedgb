#pragma once
#include<vector>
#include<cstdint>
#include<string>

class memmap;
class cpu;

namespace util {
template<typename T>
class RangeCheckVector : public std::vector<T> {
    public:
        using std::vector<T>::vector;
        T& operator[](int i) {
            return std::vector<T>::at(i);
        }

        const T& operator[](int i) const {
            return std::vector<T>::at(i);
        }
};

#define Vect util::RangeCheckVector
//#define Vect std::vector

struct cmd {
    uint64_t cycle;
    int addr;
    uint8_t val;
    uint64_t data_index;
};

bool process_events(cpu * c, memmap * bus);
#ifndef __CYGWIN__
int unzip(const std::string& zip_filename, Vect<uint8_t>& output, size_t min, size_t max);
#endif
int read(const std::string& filename, Vect<uint8_t>& output, size_t min_size, size_t max_size);
}


//Sign-extend value from 8-bit to 32-bit
#define extend(x) ( (x>0x7f) ? 0xffffff00|x : x)

//Bit constants
#define BIT0 0x0001
#define BIT1 0x0002
#define BIT2 0x0004
#define BIT3 0x0008
#define BIT4 0x0010
#define BIT5 0x0020
#define BIT6 0x0040
#define BIT7 0x0080
#define BIT8 0x0100
#define BIT9 0x0200
#define BITa 0x0400
#define BITb 0x0800
#define BITc 0x1000
#define BITd 0x2000
#define BITe 0x4000
#define BITf 0x8000
