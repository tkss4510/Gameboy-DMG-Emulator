#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <stdint.h>

class GameBoy;

class Memory
{
public:
    Memory(GameBoy& gb);
    [[nodiscard]] uint8_t Read(uint16_t addr);
    void Write(uint16_t addr, uint8_t value);
    void SyncTimerInterruptIntoIF();
    void SyncJoypadInterruptIntoIF();

private:
    uint8_t mem[65536]{};
    GameBoy& gameboy;

    const uint16_t DIV_ADDR = 0xFF04;
    const uint16_t TIMA_ADDR = 0xFF05;
    const uint16_t TMA_ADDR = 0xFF06;
    const uint16_t TAC_ADDR = 0xFF07;
    const uint16_t IF_ADDR = 0xFF0F;
};

#endif
