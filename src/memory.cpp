#include "memory.hpp" 
#include "ppu.hpp" 
#include <cstdint> 
#include "gameboy.hpp"

Memory::Memory(GameBoy& gb) : gameboy(gb){};


[[nodiscard]] uint8_t Memory::Read(uint16_t addr)
{
    if (addr < 0x8000 || (addr >= 0xA000 && addr <= 0xBFFF)) return gameboy.ReadFromCartridge(addr);
    if (addr >= 0x8000 && addr <= 0x97FF) return gameboy.ReadFromTileData(addr);
    if (addr >= 0x9800 && addr <= 0x9BFF) return gameboy.ReadFromTileMap0(addr);
    if (addr >= 0x9C00 && addr <= 0x9FFF) return gameboy.ReadFromTileMap1(addr);
    if (addr >= 0xFE00 && addr <= 0xFE9F) return gameboy.ReadFromOAM(addr);
    if (addr == 0xFF40) return gameboy.ReadLCDC();
    if (addr == 0xFF41) return gameboy.ReadSTAT();
    if (addr == 0xFF42) return gameboy.ReadSCY();
    if (addr == 0xFF43) return gameboy.ReadSCX();
    if (addr == 0xFF44) return gameboy.ReadLY();
    if (addr == 0xFF45) return gameboy.ReadLYC();
    if (addr == 0xFF47) return gameboy.ReadBGP();
    if (addr == 0xFF48) return gameboy.ReadOBP0();
    if (addr == 0xFF49) return gameboy.ReadOBP1();
    if (addr == 0xFF4A) return gameboy.ReadWY();
    if (addr == 0xFF4B) return gameboy.ReadWX();
    if (addr == 0xFF4D) return 0xFF; //doesnt exist on dmg 
    if (addr == DIV_ADDR)  return gameboy.ReadDIV();
    if (addr == TIMA_ADDR) return gameboy.ReadTIMA();
    if (addr == TMA_ADDR)  return gameboy.ReadTMA();
    if (addr == TAC_ADDR)  return gameboy.ReadTAC();
    if (addr == 0xFF00) return gameboy.ReadJoypad(); 
    return mem[addr];
}

void Memory::Write(uint16_t addr, uint8_t value)
{ 
    if (addr < 0x8000 || (addr >= 0xA000 && addr <= 0xBFFF)) { gameboy.WriteToCartridge(addr, value); return; }
    if (addr >= 0x8000 && addr <= 0x97FF) { gameboy.WriteTileData(addr, value); return; }
    if (addr >= 0x9800 && addr <= 0x9BFF) { gameboy.WriteTileMap0(addr, value); return; }
    if (addr >= 0x9C00 && addr <= 0x9FFF) { gameboy.WriteTileMap1(addr, value); return; }
    if (addr >= 0xFE00 && addr <= 0xFE9F) { gameboy.WriteOAM(addr, value); return; }
    if (addr == 0xFF40) { gameboy.WriteLCDC(value); return; }
    if (addr == 0xFF41) { gameboy.WriteSTAT(value); return; }
    if (addr == 0xFF42) { gameboy.WriteSCY(value); return; }
    if (addr == 0xFF43) { gameboy.WriteSCX(value); return; }
    if (addr == 0xFF44) { gameboy.WriteLY(value); return; }
    if (addr == 0xFF45) { gameboy.WriteLYC(value); return; }
    if (addr == 0xFF46) //OAM DMA Transfer
    {
        uint16_t src = static_cast<uint16_t>(value) << 8;
        for (uint16_t i = 0; i < 160; i++)
            gameboy.WriteOAM(0xFE00 + i, Read(src + i));
        mem[addr] = value;
        return;
    }
    if (addr == 0xFF47) { gameboy.WriteBGP(value); return; }
    if (addr == 0xFF48) { gameboy.WriteOBP0(value); return; }
    if (addr == 0xFF49) { gameboy.WriteOBP1(value); return; }
    if (addr == 0xFF4A) { gameboy.WriteWY(value); return; }
    if (addr == 0xFF4B) { gameboy.WriteWX(value); return; }
    if (addr == DIV_ADDR)  { gameboy.WriteDIV(value); return; }
    if (addr == TIMA_ADDR) { gameboy.WriteTIMA(value); return; }
    if (addr == TMA_ADDR)  { gameboy.WriteTMA(value); return; }
    if (addr == TAC_ADDR)  { gameboy.WriteTAC(value); return; }
    if (addr == 0xFF00) { gameboy.WriteJoypad(value); return; }
    mem[addr] = value;
}

// copies timer's overflow flag into IF (bit 2, timer interrupt).
// timer cant reach IF itself, so memory bridges the two.
// Call once per CPU step after advancing the timer.
void Memory::SyncTimerInterruptIntoIF()
{
    if (gameboy.TimerHasPendingInterrupt())
    {
        mem[IF_ADDR] |= 0b00000100; // bit 2 = timer interrupt
        gameboy.TimerClearInterrupt();
    }
}

//same as above
void Memory::SyncJoypadInterruptIntoIF()
{
    if (gameboy.JoypadHasPendingInterrupt())
    {
        mem[IF_ADDR] |= 0b00010000;   // bit 4 = joypad interrupt
        gameboy.JoypadClearInterrupt();
    }
}
