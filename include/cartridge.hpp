#ifndef CARTRIDGE_HPP
#define CARTRIDGE_HPP

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "mbc.hpp"

struct Header
{
    std::string title;          //$0134-$013E: game title in upper case ASCII (11 bytes on newer carts, up to $0143 on old ones)
    uint8_t cgbFlag{};          //$0143: $80 = CGB features but DMG compatible, $C0 = CGB only
    uint8_t cartridgeType{};    //$0147: mapper / MBC type
    uint8_t romSize{};          //$0148
    uint8_t ramSize{};          //$0149
    uint8_t headerChecksum{};   //$014D
};


class Cartridge 
{
public:
    Cartridge();
    Cartridge(Cartridge &&) = delete;
    Cartridge(const Cartridge &) = delete;
    Cartridge &operator=(Cartridge &&) = delete;
    Cartridge &operator=(const Cartridge &) = delete;
    ~Cartridge();

    bool LoadRom(const std::vector<uint8_t>& data);
    void WriteHeader(uint16_t addr, uint8_t value);
    void CreateMbc();
    uint8_t Read(uint16_t addr);
    void Write(uint16_t addr, uint8_t value);
private:
    Header header;
    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;
    std::unique_ptr<MBC> mbc;
    void WriteTitle(uint16_t index, uint8_t value);
    void WriteCgbFlag(uint8_t value);
    void WriteCartridgeType(uint8_t value);
    void WriteRomSize(uint8_t value);
    void WriteRamSize(uint8_t value);
    void WriteHeaderChecksum(uint8_t value);
    bool HasBattery() const
    {
        return header.cartridgeType == 0x03 || header.cartridgeType == 0x06;
    }
};

#endif // !CARTRIDGE_HPP
