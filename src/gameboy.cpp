#include "gameboy.hpp"
#include "joypad.hpp"
#include "memory.hpp"
#include "timer.hpp"
#include "ppu.hpp"
#include "cpu.hpp"
#include "cartridge.hpp"
#include <cstdint>
#include <vector>
#include <stddef.h>
#include <stdexcept>

GameBoy::GameBoy(const std::vector<uint8_t>& rom, const char* title)
    : memory(*this),
      timer(),
      ppu(*this),
      cartridge(),
      joypad(),
      lcd(title),
      cpu(*this)
{
    if (!cartridge.LoadRom(rom))
        throw std::runtime_error("Invalid ROM");

    //Init
    memory.Write(0xFF40, 0x91); // LCDC
    memory.Write(0xFF42, 0x00); // SCY
    memory.Write(0xFF43, 0x00); // SCX
    memory.Write(0xFF45, 0x00); // LYC
    memory.Write(0xFF47, 0xFC); // BGP
    memory.Write(0xFF48, 0xFF); // OBP0
    memory.Write(0xFF49, 0xFF); // OBP1
    memory.Write(0xFF4A, 0x00); // WY
    memory.Write(0xFF4B, 0x00); // WX

}

void GameBoy::Run()
{
    int pollCounter{0};
    int frameCycles{0};
    while (!lcd.ShouldQuit()) //Lcd owns the window so we ask it 
    {
        if (++pollCounter >= 4096)   //about every few ms of emulated time
        {
            pollCounter = 0;
            joypad.SetState(lcd.PollEvents());
            memory.SyncJoypadInterruptIntoIF();
        }

        int m_cycles = cpu.Step();
        timer.Advance(m_cycles * 4);
        memory.SyncTimerInterruptIntoIF();
        ppu.Step(m_cycles * 4);

        //limitting fps
        frameCycles += m_cycles * 4;                    
        if (frameCycles >= 70224) { frameCycles -= 70224; lcd.Throttle(); }

        if (ppu.isFrameReady)
        {
            lcd.SetFrameBuffer(ppu.GetFrame());
            lcd.Step();
            ppu.isFrameReady = false;
        }
    }
}

//WRITE

void GameBoy::WriteToMemory(uint16_t addr, uint8_t val) { memory.Write(addr,val); }

void GameBoy::WriteToCartridge(uint16_t addr, uint8_t val){return cartridge.Write(addr,val);}

void GameBoy::WriteTileData(uint16_t addr, uint8_t val) {return ppu.WriteTileData(val,addr);}
void GameBoy::WriteTileMap0(uint16_t addr, uint8_t val) {return ppu.WriteTileMap0(val,addr);}
void GameBoy::WriteTileMap1(uint16_t addr, uint8_t val) {return ppu.WriteTileMap1(val,addr);}
void GameBoy::WriteOAM(uint16_t addr, uint8_t val) {return ppu.WriteOAM(val,addr);}

void GameBoy::WriteLCDC(uint8_t val) {return ppu.WriteLCDC(val);}
void GameBoy::WriteSTAT(uint8_t val) {return ppu.WriteSTAT(val);}
void GameBoy::WriteSCX(uint8_t val) {return ppu.WriteSCX(val);}
void GameBoy::WriteSCY(uint8_t val) {return ppu.WriteSCY(val);}
void GameBoy::WriteLY(uint8_t val) {return ppu.WriteLY(val);}
void GameBoy::WriteLYC(uint8_t val) {return ppu.WriteLYC(val);}
void GameBoy::WriteBGP(uint8_t val) {return ppu.WriteBGP(val);}
void GameBoy::WriteOBP0(uint8_t val) {return ppu.WriteOBP0(val);}
void GameBoy::WriteOBP1(uint8_t val) {return ppu.WriteOBP1(val);}
void GameBoy::WriteWX(uint8_t val) {return ppu.WriteWX(val);}
void GameBoy::WriteWY(uint8_t val) {return ppu.WriteWY(val);}

void GameBoy::WriteDIV(uint8_t val) {timer.WriteDIV(val);}
void GameBoy::WriteTIMA(uint8_t val) {timer.WriteTIMA(val);}
void GameBoy::WriteTMA(uint8_t val) {timer.WriteTMA(val);}
void GameBoy::WriteTAC(uint8_t val) {timer.WriteTAC(val);}

void GameBoy::WriteJoypad(uint8_t val) {joypad.Write(val);}

//READ

uint8_t GameBoy::ReadFromMemory(uint16_t addr) { return memory.Read(addr); }

uint8_t GameBoy::ReadFromCartridge(uint16_t addr) {return cartridge.Read(addr);}

uint8_t GameBoy::ReadFromTileData(uint16_t addr) {return ppu.ReadTileData(addr);}
uint8_t GameBoy::ReadFromTileMap0(uint16_t addr) {return ppu.ReadTileMap0(addr);}
uint8_t GameBoy::ReadFromTileMap1(uint16_t addr) {return ppu.ReadTileMap1(addr);}
uint8_t GameBoy::ReadFromOAM(uint16_t addr) {return ppu.ReadOAM(addr);}

uint8_t GameBoy::ReadLCDC() {return ppu.ReadLCDC();}
uint8_t GameBoy::ReadSTAT() {return ppu.ReadSTAT();}
uint8_t GameBoy::ReadSCX() {return ppu.ReadSCX();}
uint8_t GameBoy::ReadSCY() {return ppu.ReadSCY();}
uint8_t GameBoy::ReadLY() {return ppu.ReadLY();}
uint8_t GameBoy::ReadLYC() {return ppu.ReadLYC();}
uint8_t GameBoy::ReadBGP() {return ppu.ReadBGP();}
uint8_t GameBoy::ReadOBP0() {return ppu.ReadOBP0();}
uint8_t GameBoy::ReadOBP1() {return ppu.ReadOBP1();}
uint8_t GameBoy::ReadWX() {return ppu.ReadWX();}
uint8_t GameBoy::ReadWY() {return ppu.ReadWY();}

uint8_t GameBoy::ReadDIV() {return timer.ReadDIV();}
uint8_t GameBoy::ReadTIMA() {return timer.ReadTIMA();}
uint8_t GameBoy::ReadTMA(){return timer.ReadTMA();}
uint8_t GameBoy::ReadTAC(){return timer.ReadTAC();}

uint8_t GameBoy::ReadJoypad(){return joypad.Read();}

bool GameBoy::TimerHasPendingInterrupt(){ return timer.HasPendingInterrupt();}
void GameBoy::TimerClearInterrupt(){ timer.ClearInterrupt();}

bool GameBoy::JoypadHasPendingInterrupt(){ return joypad.HasPendingInterrupt();}
void GameBoy::JoypadClearInterrupt(){ joypad.ClearInterrupt();}

void GameBoy::TimerAdvance(int tCycles) { timer.Advance(tCycles); }
void GameBoy::PpuStep(uint32_t tCycles) { ppu.Step(tCycles); }
void GameBoy::PpuClearFrameReady() { ppu.isFrameReady = false; }
void GameBoy::SyncTimerInterruptIntoIF() { memory.SyncTimerInterruptIntoIF(); }
