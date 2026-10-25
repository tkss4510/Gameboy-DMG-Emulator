#ifndef TIMER_HPP
#define TIMER_HPP

#include <cstdint>

class Timer
{
public:
    void Advance(int t_cycles)
    {
        for (int i = 0; i < t_cycles; i++)
        {
            if (reload_pending)
            {
                if (--reload_delay <= 0)
                {
                    TIMA = TMA;
                    interrupt = true;
                    reload_pending = false;
                }
            }

            uint16_t old_counter = internal_counter;
            internal_counter++;

            bool old_bit = GetSelectedBit(old_counter);
            bool new_bit = GetSelectedBit(internal_counter);

            // TIMA increments on a falling edge of the selected bit
            if (IsTimerEnabled() && old_bit && !new_bit)
            {
                IncrementTIMA();
            }
        }
    }

    uint8_t ReadDIV() { return internal_counter >> 8; }

    void WriteDIV(uint8_t)
    {
        bool old_bit = GetSelectedBit(internal_counter);
        internal_counter = 0;
        bool new_bit = GetSelectedBit(internal_counter); // always false: counter is 0

        if (IsTimerEnabled() && old_bit && !new_bit)
        {
            IncrementTIMA();
        }
    }

    uint8_t ReadTIMA() { return TIMA; }
    void WriteTIMA(uint8_t val)
    {
        // Writing TIMA while a reload is pending cancels that pending
        // reload/interrupt (matches real hardware behavior for writes that
        // land inside the one-cycle reload delay window).
        reload_pending = false;
        TIMA = val;
    }

    uint8_t ReadTMA() { return TMA; }
    void WriteTMA(uint8_t val) { TMA = val; } // picked up next reload if one is pending

    uint8_t ReadTAC() { return TAC; }
    void WriteTAC(uint8_t val)
    {
        bool old_enabled = IsTimerEnabled();
        int old_bitpos = SelectedBitPos();
        bool old_signal = old_enabled && ((internal_counter >> old_bitpos) & 1);

        TAC = val & 0x07;

        bool new_enabled = IsTimerEnabled();
        int new_bitpos = SelectedBitPos();
        bool new_signal = new_enabled && ((internal_counter >> new_bitpos) & 1);

        if (old_signal && !new_signal)
        {
            IncrementTIMA();
        }
    }

    bool HasPendingInterrupt() {return interrupt;}
    void ClearInterrupt() { interrupt = false; }
private:
    bool interrupt{};
    bool reload_pending{};
    int  reload_delay{};

    int SelectedBitPos()
    {
        switch (TAC & 0b11)
        {
            case 0b00: return 9;
            case 0b01: return 3;
            case 0b10: return 5;
            default:   return 7; // 0b11
        }
    }

    bool GetSelectedBit(uint16_t counter)
    {
        return (counter >> SelectedBitPos()) & 1;
    }

    bool IsTimerEnabled() { return (TAC & 0b100) != 0; }

    // shared by the per-cycle falling-edge path and the DIV/TAC-write edge
    // cases, so overflow-into-reload behaves identically no matter what
    // triggered the increment.
    void IncrementTIMA()
    {
        TIMA++;
        if (TIMA == 0) // overflowed
        {
            reload_pending = true;
            reload_delay = 1; // reload (TMA load + interrupt) lands next T-cycle
        }
    }

    //following registers arent used by other parts of the gameboy so we can store tham in the object itself
    uint16_t internal_counter = 0;
    uint8_t TIMA = 0;
    uint8_t TMA = 0;
    uint8_t TAC = 0;
};

#endif // !TIMER_HPP
