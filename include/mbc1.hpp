#ifndef MBC1_HPP
#define MBC1_HPP

#include <cstdint>
#include <string>
#include <fstream>
#include <ios>
#include <vector>
#include "mbc.hpp"

class MBC1 : public MBC
{
public:
    MBC1(uint32_t romBanks, uint32_t ramBanks)
        : romBankCount(romBanks), ramBankCount(ramBanks), ram(ramBanks * 0x2000) {}

    void Write(uint16_t addr, uint8_t value) override
    {
        if (addr < 0x2000)
        {
            ramEnable = (value & 0x0F) == 0x0A;
        }
        else if (addr < 0x4000)
        {
            romBank = value & 0x1F;
            if (romBank == 0) romBank = 1;
        }
        else if (addr < 0x6000)
        {
            ramBank = value & 0x03;
        }
        else if (addr < 0x8000)
        {
            mode = (value & 0x01) != 0;
        }
    }

    uint32_t GetRomAddress(uint16_t addr) const override
    {
        uint32_t bank;
        if (addr < 0x4000) bank = mode ? (ramBank << 5) : 0;
        else bank = (ramBank << 5) | romBank;

        if (romBankCount > 0) bank %= romBankCount;
        return (bank * 0x4000) + (addr & 0x3FFF);
    }

    uint32_t GetRamAddress(uint16_t addr) const
    {
        uint32_t bank = mode ? ramBank : 0;

        if (ramBankCount > 0) bank %= ramBankCount;
        return (bank * 0x2000) + (addr - 0xA000);
    }

    uint8_t ReadRam(uint16_t addr) const override
    {
        if (!ramEnable || ram.empty()) return 0xFF;
        return ram[GetRamAddress(addr)];
    }

    void WriteRam(uint16_t addr, uint8_t value) override
    {
        if (!ramEnable || ram.empty()) return;
        ram[GetRamAddress(addr)] = value;
    }

    bool IsRamEnabled() const override { return ramEnable; }

    void LoadRam() override
    {
        std::ifstream in(savePath, std::ios::binary);
        if (in) in.read(reinterpret_cast<char*>(ram.data()), ram.size());
    }
    void SaveRam() override
    {
        std::ofstream out(savePath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(ram.data()), ram.size());
    }
private:
    uint8_t romBank{1};
    uint8_t ramBank{};
    bool mode{};
    bool ramEnable{};
    uint32_t romBankCount;
    uint32_t ramBankCount;
    std::vector<uint8_t> ram;   // cartridge RAM now lives inside the class
    static constexpr std::string savePath{"MBC1_SAVE.sav"};
};

#endif // !MBC1_HPP
