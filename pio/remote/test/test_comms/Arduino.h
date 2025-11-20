#pragma once
// Mock Arduino.h for native testing
#include <cstdint>
#include <cstring>

typedef uint8_t byte;
typedef bool boolean;

inline unsigned long millis() {
    static unsigned long counter = 0;
    return ++counter;
}

template<typename T>
T min(T a, T b) { return (a < b) ? a : b; }

template<typename T>
T max(T a, T b) { return (a > b) ? a : b; }
