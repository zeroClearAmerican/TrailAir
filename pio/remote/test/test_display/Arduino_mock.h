/**
 * Minimal Arduino.h mock for display testing
 * Provides only what TA_Display needs
 */

#pragma once
#include <cstdint>
#include <cstring>

// String class (minimal implementation)
class String {
public:
    String() : data_(nullptr), len_(0) {}
    String(const char* str) {
        if (str) {
            len_ = strlen(str);
            data_ = new char[len_ + 1];
            strcpy(data_, str);
        } else {
            data_ = nullptr;
            len_ = 0;
        }
    }
    String(int val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", val);
        len_ = strlen(buf);
        data_ = new char[len_ + 1];
        strcpy(data_, buf);
    }
    String(float val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", val);
        len_ = strlen(buf);
        data_ = new char[len_ + 1];
        strcpy(data_, buf);
    }
    String(const String& other) {
        if (other.data_) {
            len_ = other.len_;
            data_ = new char[len_ + 1];
            strcpy(data_, other.data_);
        } else {
            data_ = nullptr;
            len_ = 0;
        }
    }
    ~String() { delete[] data_; }
    
    String& operator=(const String& other) {
        if (this != &other) {
            delete[] data_;
            if (other.data_) {
                len_ = other.len_;
                data_ = new char[len_ + 1];
                strcpy(data_, other.data_);
            } else {
                data_ = nullptr;
                len_ = 0;
            }
        }
        return *this;
    }
    
    const char* c_str() const { return data_ ? data_ : ""; }
    size_t length() const { return len_; }
    
private:
    char* data_;
    size_t len_;
};

// Constrain function
template<typename T>
T constrain(T x, T min, T max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

// delay function (mock - does nothing in tests)
inline void delay(unsigned long) {}
