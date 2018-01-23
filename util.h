#pragma once
#include<vector>
#include<cstdint>

class memmap;

namespace util {
template<typename T>
class RangeCheckVector : public std::vector<T> {
    public:
        T& operator[](int i) {
            return std::vector<T>::at(i);
        }

        const T& operator[](int i) const {
            return std::vector<T>::at(i);
        }
};

struct cmd {
    uint64_t cycle;
    uint16_t addr;
    uint8_t val;
};

bool process_events(memmap * bus);
}

#define Vect util::RangeCheckVector
//#define Vect std::vector
