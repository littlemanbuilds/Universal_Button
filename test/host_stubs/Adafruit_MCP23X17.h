#pragma once
#include <cstdint>
class Adafruit_MCP23X17
{
public:
    bool begin_I2C(std::uint8_t) { return true; }
    void pinMode(std::uint8_t, int) {}
    int digitalRead(std::uint8_t) { return 1; }
    std::uint16_t readGPIOAB() { return 0xFFFFu; }
};
