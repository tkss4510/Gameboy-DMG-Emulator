#ifndef MBC_HPP
#define MBC_HPP

#include <cstdint>

class MBC
{
public:
    virtual ~MBC() = default;   // needed so deleting through an MBC* calls the derived destructor

    // CPU wrote to 0x0000-0x7FFF (bank switching commands)
    virtual void Write(uint16_t addr, uint8_t value) = 0;

    // CPU read 0x0000-0x7FFF: returns the position inside the ROM file
    virtual uint32_t GetRomAddress(uint16_t addr) const = 0;

    // CPU read/wrote 0xA000-0xBFFF (cartridge RAM)
    virtual uint8_t ReadRam(uint16_t addr) const = 0;
    virtual void WriteRam(uint16_t addr, uint8_t value) = 0;

    virtual bool IsRamEnabled() const = 0;
    virtual void SaveRam() = 0;
    virtual void LoadRam() = 0;
};

#endif // !MBC_HPP
