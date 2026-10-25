#ifndef MBC2_HPP
#define MBC2_HPP

#include <cstdint>
#include <string>
#include <fstream>
#include <ios>
#include "mbc.hpp"


class MBC2 : public MBC
{
public:
    MBC2(uint32_t romBanks) : romBankCount(romBanks){}

    void Write(uint16_t addr, uint8_t value) override
    {
        if (addr < 0x4000)
        {
            if((addr & 0b00000001'00000000) > 0) //bit 8 is set
            {
                romBank = (value & 0x0F);
                if(romBank == 0) romBank = 1;
            }
            else 
            {
               ramEnable = (value & 0x0F) == 0x0A;
            }
        }
    }

    uint32_t GetRomAddress(uint16_t addr) const override
    {
        uint32_t bank = (addr < 0x4000) ? 0 : romBank;   
        if (romBankCount > 0) bank %= romBankCount;
        return (bank * 0x4000) + (addr & 0x3FFF);
    }

    //0xA000-0xBFFF
    uint8_t ReadRam(uint16_t addr) const override
    {
        if (!ramEnable) return 0xFF;
        return 0xF0 | ram[addr & 0x1FF];  // only 4 bits exist, upper 4 read as 1s
    }

    // CPU wrote 0xA000-0xBFFF
    void WriteRam(uint16_t addr, uint8_t value) override
    {
        if (!ramEnable) return;
        ram[addr & 0x1FF] = value & 0x0F; // only 4 bits are stored
    }

    bool IsRamEnabled() const override { return ramEnable; }

    void LoadRam() override
    {
        std::ifstream in(savePath, std::ios::binary);
        if (in) in.read(reinterpret_cast<char*>(ram), 512);
    }
    void SaveRam() override
    {
        std::ofstream out(savePath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(ram), 512);
    }

private:
    uint8_t romBank{1};
    bool ramEnable{};
    uint8_t ram[512]{}; //built in ram
    uint32_t romBankCount;
    static constexpr std::string savePath{"MBC2_SAVE.sav"};
};

#endif // !MBC2_HPP
