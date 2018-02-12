#pragma once
#include<vector>
#include<cstdint>
#include<string>

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
    int addr;
    uint8_t val;
    uint64_t data_index;
};

bool process_events(memmap * bus);
int unzip(const std::string& zip_filename, std::vector<uint8_t>& output, size_t min, size_t max);
int read(const std::string& filename, std::vector<uint8_t>& output, size_t min_size, size_t max_size);
}

//#define Vect util::RangeCheckVector
#define Vect std::vector
