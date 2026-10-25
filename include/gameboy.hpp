#ifndef GAMEBOY_HPP
#define GAMEBOY_HPP

#include "cartridge.hpp"
#include "cpu.hpp"
#include "joypad.hpp"
#include "lcd.hpp"
#include "memory.hpp"
#include "ppu.hpp"
#include "timer.hpp"
#include <cstdint>
#include <vector>

class GameBoy 
{
  Memory memory;
  Timer timer{};
  Ppu ppu;
  Cartridge cartridge{};
  Joypad joypad{};
  LCD lcd;
  Cpu cpu;

public:
  GameBoy(const std::vector<uint8_t> &rom, const char *title);
  void Run();

  // Glue functions to link components easily
  //
  // WRITE
  void WriteToMemory(uint16_t addr, uint8_t val);

  void WriteToCartridge(uint16_t addr, uint8_t val);

  void WriteTileData(uint16_t addr, uint8_t val);
  void WriteTileMap0(uint16_t addr, uint8_t val);
  void WriteTileMap1(uint16_t addr, uint8_t val);
  void WriteOAM(uint16_t addr, uint8_t val);

  void WriteLCDC(uint8_t val);
  void WriteSTAT(uint8_t val);
  void WriteSCX(uint8_t val);
  void WriteSCY(uint8_t val);
  void WriteLY(uint8_t val);
  void WriteLYC(uint8_t val);
  void WriteBGP(uint8_t val);
  void WriteOBP0(uint8_t val);
  void WriteOBP1(uint8_t val);
  void WriteWX(uint8_t val);
  void WriteWY(uint8_t val);

  void WriteDIV(uint8_t val);
  void WriteTIMA(uint8_t val);
  void WriteTMA(uint8_t val);
  void WriteTAC(uint8_t val);

  void WriteJoypad(uint8_t val);

  // READ
  uint8_t ReadFromMemory(uint16_t addr);

  uint8_t ReadFromCartridge(uint16_t addr);

  uint8_t ReadFromTileData(uint16_t addr);
  uint8_t ReadFromTileMap0(uint16_t addr);
  uint8_t ReadFromTileMap1(uint16_t addr);
  uint8_t ReadFromOAM(uint16_t addr);

  uint8_t ReadLCDC();
  uint8_t ReadSTAT();
  uint8_t ReadSCX();
  uint8_t ReadSCY();
  uint8_t ReadLY();
  uint8_t ReadLYC();
  uint8_t ReadBGP();
  uint8_t ReadOBP0();
  uint8_t ReadOBP1();
  uint8_t ReadWX();
  uint8_t ReadWY();

  uint8_t ReadDIV();
  uint8_t ReadTIMA();
  uint8_t ReadTMA();
  uint8_t ReadTAC();

  uint8_t ReadJoypad();

  bool TimerHasPendingInterrupt();
  void TimerClearInterrupt();

  bool JoypadHasPendingInterrupt();
  void JoypadClearInterrupt();

  //Used for testing
  Cpu& GetCpu() { return cpu; }
  Ppu& GetPpu() { return ppu; }
  LCD& GetLcd() { return lcd; }
  Joypad& GetJoypad() { return joypad; }
  Memory& GetMemory() { return memory; }

  void TimerAdvance(int tCycles);
  void PpuStep(uint32_t tCycles);
  void PpuClearFrameReady();
  void SyncTimerInterruptIntoIF();
};

#endif // !GAMEBOY_HPP
