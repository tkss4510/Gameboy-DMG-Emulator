#ifndef JOYPAD_HPP
#define JOYPAD_HPP

#include <cstdint>
#include <SDL3/SDL_keyboard.h>
class Joypad
{
public:
    void SetState(uint8_t pressed)            // from lcd.PollEvents(), 1 = pressed
    {
        uint8_t now = ~pressed;               
        uint8_t newlyPressed = prev & ~now;   // was released (1), now pressed (0)

        bool dpadSelected    = !(select & 0x10);
        bool buttonsSelected = !(select & 0x20);
        if ((dpadSelected    && (newlyPressed & 0x0F)) ||
            (buttonsSelected && (newlyPressed & 0xF0)))
            interrupt = true;
        state = now;
        prev  = now;
    }

    void Write(uint8_t v) { select = v & 0x30; }

    uint8_t Read() const
    {
        uint8_t low = 0x0F;
        if (!(select & 0x10)) low &= state & 0x0F;          // d-pad selected
        if (!(select & 0x20)) low &= (state >> 4) & 0x0F;   // buttons selected
        return 0xC0 | select | low;
    }

    bool HasPendingInterrupt() const { return interrupt; }
    void ClearInterrupt()            { interrupt = false; }

private:
    uint8_t state{0xFF};    // flipped state, 0 = pressed
    uint8_t prev{0xFF};
    uint8_t select{0x30};   // bits 4-5, nothing selected at start
    bool    interrupt{};
};

#endif 
