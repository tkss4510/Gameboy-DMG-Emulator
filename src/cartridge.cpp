#include "cartridge.hpp"
#include "mbc1.hpp"
#include "mbc2.hpp"
#include <memory>
#include <vector>
#include <cstdint>

Cartridge::Cartridge()
{

}

Cartridge::~Cartridge()
{
    if (mbc && HasBattery()) mbc->SaveRam();
}

bool Cartridge::LoadRom(const std::vector<uint8_t>& data)
{
    if (data.size() < 0x150) return false;

    rom = data;
    header.title.assign(11, '\0');
    for (uint16_t addr = 0x0134; addr <= 0x014D; addr++)
    {
        WriteHeader(addr, rom[addr]);
    }
    CreateMbc();
    if (mbc && HasBattery()) mbc->LoadRam();
    return true;
}

void Cartridge::WriteHeader(uint16_t addr, uint8_t value)
{
    if (addr >= 0x0134 && addr <= 0x013E) { WriteTitle(addr - 0x0134, value); return; }
    if (addr == 0x0143) { WriteCgbFlag(value); return; }
    if (addr == 0x0147) { WriteCartridgeType(value); return; }
    if (addr == 0x0148) { WriteRomSize(value); return; }
    if (addr == 0x0149) { WriteRamSize(value); return; }
    if (addr == 0x014D) { WriteHeaderChecksum(value); return; }
}

void Cartridge::WriteTitle(uint16_t index, uint8_t value) { header.title[index] = value; }

void Cartridge::WriteCgbFlag(uint8_t value) { header.cgbFlag = value; }

void Cartridge::WriteCartridgeType(uint8_t value) { header.cartridgeType = value; }

void Cartridge::WriteRomSize(uint8_t value) { header.romSize = value; }

void Cartridge::WriteRamSize(uint8_t value) { header.ramSize = value; }

void Cartridge::WriteHeaderChecksum(uint8_t value) { header.headerChecksum = value; }

void Cartridge::CreateMbc()
{
    uint32_t romBanks = 2u << header.romSize;
    uint32_t ramBanks = 0;
    switch (header.ramSize)
    {
        case 0x02: ramBanks = 1; break;
        case 0x03: ramBanks = 4; break;
        case 0x04: ramBanks = 16; break;
        case 0x05: ramBanks = 8; break;
        default: break;
    }

    if (header.cartridgeType >= 0x01 && header.cartridgeType <= 0x03)
    {
        mbc = std::make_unique<MBC1>(romBanks, ramBanks);   // MBC1 owns its RAM now
    }
    else if (header.cartridgeType == 0x05 || header.cartridgeType == 0x06)
    {
        mbc = std::make_unique<MBC2>(romBanks);             // MBC2 has built-in RAM, no ramBanks
    }
}

uint8_t Cartridge::Read(uint16_t addr)
{
    if (addr < 0x8000)
    {
        uint32_t offset = mbc ? mbc->GetRomAddress(addr) : addr;
        return offset < rom.size() ? rom[offset] : 0xFF;
    }
    if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        return mbc ? mbc->ReadRam(addr) : 0xFF;
    }
    return 0xFF;
}

void Cartridge::Write(uint16_t addr, uint8_t value)
{
    if (addr < 0x8000)
    {
        if (mbc) mbc->Write(addr, value);
        return;
    }
    if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        if (mbc) mbc->WriteRam(addr, value);
    }
}
