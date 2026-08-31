#pragma once
#include <cstddef>
#include <cstdint>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define INPUT_PULLUP 2
#define BIN 2
struct __FlashStringHelper {};
#define F(x) (reinterpret_cast<const __FlashStringHelper *>(x))
inline std::uint32_t millis() { return 0u; }
inline void delay(unsigned long) {}
inline void pinMode(std::uint8_t, int) {}
inline int digitalRead(std::uint8_t) { return HIGH; }
struct SerialStub
{
    void begin(unsigned long) {}
    template <typename T> void print(const T &) {}
    template <typename T> void print(const T &, int) {}
    template <typename T> void println(const T &) {}
    template <typename T> void println(const T &, int) {}
    void println() {}
};
static SerialStub Serial __attribute__((unused));
