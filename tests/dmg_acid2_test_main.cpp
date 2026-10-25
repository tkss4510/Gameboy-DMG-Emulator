// dmg_acid2_test_main.cpp
//
// Runs dmg-acid2.gb through the GameBoy class (which owns and wires up the
// CPU/Memory/Timer/PPU/Cartridge/Joypad/LCD).
// dmg-acid2 has no serial output, so the result is visual: compare the
// window (or the dumped acid2_output.ppm) with the reference image.
//
// Usage: gb_acid2 <path_to_rom> [frames_before_dump=120]

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "gameboy.hpp"

static void DumpFramePPM(const std::string& path,
                         const std::array<std::array<uint8_t,144>,160>& frame)
{
    static const uint8_t gray[4] = {255, 170, 85, 0};
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            uint8_t g = gray[frame[x][y] & 3];
            out.put(static_cast<char>(g));
            out.put(static_cast<char>(g));
            out.put(static_cast<char>(g));
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: gb_acid2 <path_to_rom> [frames_before_dump=120]\n";
        return 1;
    }

    const std::string romPath = argv[1];
    const int dumpAfterFrames = (argc >= 3) ? std::atoi(argv[2]) : 120;

    std::ifstream file(romPath, std::ios::binary);
    if (!file)
    {
        std::cerr << "ERROR: could not open ROM file: " << romPath << "\n";
        return 1;
    }

    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    if (rom.empty())
    {
        std::cerr << "ERROR: ROM file read as empty: " << romPath << "\n";
        return 1;
    }

    std::cout << "Loaded ROM: " << romPath << " (" << rom.size() << " bytes)\n";
    if (rom.size() > 0x147 && rom[0x147] > 0x03)
    {
        std::cout << "NOTE: cartridge type is 0x" << std::hex << (int)rom[0x147] << std::dec
                  << " (only ROM-only and MBC1 are implemented).\n";
    }

    try
    {
        // GameBoy builds and connects every component and loads the ROM into the
        // cartridge (it throws if the ROM is too small / has an invalid header).
        // It also applies the post-boot register state (no boot ROM is run).
        GameBoy gb(rom, "dmg-acid2");

        Cpu&      cpu     = gb.GetCpu();
        Ppu&      ppu     = gb.GetPpu();
        LCD&      lcd     = gb.GetLcd();
        Joypad&   joypad  = gb.GetJoypad();
        Memory&   memory  = gb.GetMemory();

        // The Cpu constructor prints PC in hex and leaves cout in hex mode.
        std::cout << std::dec << "\n";
        if (cpu.GetPC() != 0x100)
        {
            std::cout << "WARNING: PC did not initialize to 0x100.\n";
        }

        int frames = 0;
        bool dumped = false;
        int pollCounter = 0;

        while (!lcd.ShouldQuit())
        {
            if (++pollCounter >= 4096)
            {
                pollCounter = 0;
                joypad.SetState(lcd.PollEvents());
                memory.SyncJoypadInterruptIntoIF();
            }

            int mCycles = cpu.Step();
            gb.TimerAdvance(mCycles * 4);
            gb.SyncTimerInterruptIntoIF();
            ppu.Step(mCycles * 4);

            if (ppu.isFrameReady)
            {
                lcd.SetFrameBuffer(ppu.GetFrame());
                lcd.Step();
                ppu.isFrameReady = false;
                frames++;

                if (!dumped && frames >= dumpAfterFrames)
                {
                    DumpFramePPM("acid2_output.ppm", ppu.GetFrame());
                    std::cout << "Frame " << frames << " written to acid2_output.ppm\n";
                    dumped = true;
                }
            }
        }

        std::cout << "Exited after " << frames << " frames.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << ": " << romPath << "\n";
        return 1;
    }

    return 0;
}
