#pragma once
#include<vector>

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

bool process_events(memmap * bus);
}

#define Vect util::RangeCheckVector
//#define Vect std::vector
