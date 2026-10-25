#ifndef PPU_HPP
#define PPU_HPP

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

class GameBoy;
              
using Tile = std::array<std::pair<uint8_t,uint8_t>,8>;
enum class Mode : uint8_t
{
    H_BLANK  = 0,
    V_BLANK  = 1,
    OAM_SCAN = 2,
    DRAWING  = 3
};



struct Sprite
{
    uint8_t xPosition{};
    uint8_t yPosition{};
    uint8_t tileIndex{}; 
    //In 8×8 mode (LCDC bit 2 = 0), specifies the tile index from tileData at $8000-$8FFF
    //In 8×16 mode (LCDC bit 2 = 1), the memory area at $8000-$8FFF is still interpreted as a series of 8×8 tiles,
    //The hardware derives both tile indices from that single byte using a fixed rule:
    //The top tile's index is tileIndex with bit 0 forced to 0. The bottom tile's index is tileIndex with bit 0 forced to 1.

    uint8_t attributes{};
    //Bit 7 - Priority: If set bkg & window with color indices 1-3 are drawn over this Sprite
    //Bit 6 - Y flip: If set entire sprite is VERTICALLY mirrored
    //Bit 5 - X flip: If set entire sprite is HORIZONTALLY mirrored
    //Bit 4 - DMG palette: 0 = OBP0, 1 = OBP1
    //Bits 0-3 - ONLY USED IN CGB
};

class Ppu
{
public:
    Ppu(GameBoy& gb);
    void WriteTileData(uint8_t val, uint16_t addr)
    {
        if(addr < 0x8000 || addr > 0x97FF) return;
        uint16_t offset = addr - 0x8000;
        uint16_t tile   = offset / 16;
        uint8_t  row    = (offset % 16) / 2;

        if(offset % 2 == 0) tileData[tile][row].first  = val;
        else tileData[tile][row].second = val;  
    }
    uint8_t ReadTileData(uint16_t addr)
    {
        if(addr < 0x8000 || addr > 0x97FF) return 0xFF;
        uint16_t offset = addr - 0x8000;
        uint16_t tile   = offset / 16;
        uint8_t  row    = (offset % 16) / 2;

        if(offset % 2 == 0) return tileData[tile][row].first;
        else return tileData[tile][row].second;  
    }
    void WriteTileMap0(uint8_t val, uint16_t addr)
    {
        if(addr < 0x9800 || addr > 0x9BFF) return;
        uint16_t offset = addr - 0x9800;
        uint16_t row = offset / 32;
        uint8_t col = offset % 32;
        tileMap0[col][row] = val;
    }
    uint8_t ReadTileMap0(uint16_t addr)
    {
        if(addr < 0x9800 || addr > 0x9BFF) return 0xFF;
        uint16_t offset = addr - 0x9800;
        uint16_t row = offset / 32;
        uint8_t col = offset % 32;
        return tileMap0[col][row];
    }
    void WriteTileMap1(uint8_t val, uint16_t addr)
    {
        if(addr < 0x9C00 || addr > 0x9FFF) return;
        uint16_t offset = addr - 0x9C00;
        uint16_t row = offset / 32;
        uint8_t col = offset % 32;
        tileMap1[col][row] = val;
    }
    uint8_t ReadTileMap1(uint16_t addr)
    {
        if(addr < 0x9C00 || addr > 0x9FFF) return 0xFF;
        uint16_t offset = addr - 0x9C00;
        uint16_t row = offset / 32;
        uint8_t col = offset % 32;
        return tileMap1[col][row];
    }

    void WriteWX(uint8_t val) { WX = val;}
    void WriteWY(uint8_t val) { WY = val;}
    uint8_t ReadWX() {return WX;}
    uint8_t ReadWY() {return WY;}

    void WriteSCX(uint8_t val) { SCX = val;}
    void WriteSCY(uint8_t val) { SCY = val;}
    uint8_t ReadSCX() {return SCX;}
    uint8_t ReadSCY() {return SCY;}


    void WriteOAM(uint8_t val, uint16_t addr)
    {
        if(addr < 0xFE00 || addr > 0xFE9F) return;
        uint16_t offset = addr - 0xFE00;
        uint16_t spriteIndex = offset / 4;
        uint8_t valueToModify = offset % 4;
        Sprite& obj  = OAM[spriteIndex];
        switch (valueToModify) 
        {
            case 0: {obj.yPosition = val; break;}
            case 1: {obj.xPosition = val; break;}
            case 2: {obj.tileIndex = val; break;}
            case 3: {obj.attributes = val; break;}
            default: return;
        }
    }
    uint8_t ReadOAM(uint16_t addr)
    {
        if(addr < 0xFE00 || addr > 0xFE9F) return 0xFF;
        uint16_t offset = addr - 0xFE00;
        uint16_t spriteIndex = offset / 4;
        uint8_t valueToReturn = offset % 4;
        const Sprite& obj  = OAM[spriteIndex];
        switch (valueToReturn) 
        {
            case 0: {return obj.yPosition; }
            case 1: {return obj.xPosition; }
            case 2: {return obj.tileIndex; }
            case 3: {return obj.attributes; }
            default: return 0xFF;
        }
    }

    void WriteLCDC(uint8_t val) { LCDC = val;}
    uint8_t ReadLCDC() {return LCDC;}
    
    void WriteLY(uint8_t val) { /* LY IS Read-only*/;}
    uint8_t ReadLY() {return LY;}

    void WriteLYC(uint8_t val) { LYC = val; CheckLYC(); }
    uint8_t ReadLYC() {return LYC;}

    void WriteSTAT(uint8_t val) { STAT = (STAT & 0b10000111) | (val & 0b01111000); CheckLYC(); }

    uint8_t ReadSTAT()
    {
        uint8_t mode = static_cast<uint8_t>(ppuMode);
        uint8_t coincidence = (LY == LYC) ? 0b100 : 0;
        return 0b10000000 | (STAT & 0b01111000) | coincidence | mode;
    }

    void WriteBGP(uint8_t val) { BGP = val;}
    uint8_t ReadBGP() {return BGP;}

    void WriteOBP0(uint8_t val) { OBP0 = val;}
    uint8_t ReadOBP0() {return OBP0;}

    void WriteOBP1(uint8_t val) { OBP1 = val;}
    uint8_t ReadOBP1() {return OBP1;}

    void Step(uint32_t tCycles);
    bool isFrameReady{false};
    const auto& GetFrame() {return frameBuffer;} 
private:
    void ScanOAM(); //Scans OAM for Sprites in line LY and adds them to spriteLineBuffer
    void RequestVBLANK_INT();
    void RequestSTAT_INT();
    void CheckLYC();
    void RenderScanline();
    const Tile& GetTileData(uint8_t tileIndex, bool isSprite);
    uint8_t ComputePixelColorID(const std::pair<uint8_t,uint8_t>& row, uint8_t bit);
    uint8_t ApplyPalette(uint8_t palette, uint8_t colorID) { return (palette >> (colorID * 2)) & 0b11;}
private:

    GameBoy& gameboy; //Glue
    
    const uint16_t IF_ADDR = 0xFF0F;
    const uint8_t V_BLANK_INT = 0x40;

    std::array<Tile,384> tileData{}; //$8000-$97FF
    std::array<std::array<uint8_t, 32>, 32> tileMap0{}; // $9800-$9BFF Tile indices
    std::array<std::array<uint8_t, 32>, 32> tileMap1{}; // $9C00-$9FFF Tile indices
    //Tiles are obtained from tileData array using either of the two addressing modes. which can be selected via the LCDC register.

    uint8_t WX{0};      //$FF4A, 0 meaning the top
    uint8_t WY{0};      //$FF4B, 7 meaning the far left
    //Position of the window is determined by (WX - 7 , WY) 
    //The window overlays the bkg. Not scrollable. displayed starting top left of the tile map.
    //The bkg and window share the same tile map.
    //Disabling the window display is toggled using LCDC bit 5. However in Non-CGB mode this bit only works if bit 0 is SET. 
    //Enabling the window makes Mode 3 longer on scanlines where it's visible 
    //The Window is visible (if enabled) when WX and WY are in the range [0; 166] and [0; 143]

    uint8_t SCX{0};     //$FF42,
    uint8_t SCY{0};     //$FF43,
    //SCY and SCX can be used to scroll the Background, specifying the origin of the visible 160×144 pixel area within the 256×256 pixel bkg map.
    //The visible area of the Background wraps around the Background map 
    //(that is, when part of the visible area goes beyond the map edge, it starts displaying the opposite side of the map).
    //In Non-CGB mode, the Background (and the Window) can be disabled using LCDC bit 0.
    
    std::array<Sprite,40> OAM{}; //$FE00-FE9F
    std::vector<Sprite> spriteLineBuffer{}; 
    
    Mode ppuMode{}; //Controls the Mode of the ppu
    uint8_t LCDC{0}; //$FF40
    //Bit 7: LCD & PPU enable bit 
    //Bit 6: Window TileMap bit, 0 = tileMap0 1 = tileMap1
    //Bit 5: Window enable bit
    //Bit 4: bkg & window tile data area,  0 = $8800-$97FF 1 = $8000–$8FFF
    //Bit 3: bkg tile map area, 0 = tileMap0 1 = tileMap1
    //Bit 2: Sprite/Object Size, 0 = 8x8 1 = 8x16
    //Bit 1: Sprite/Object enable bit
    //Bit 0: bkg & window enable, 0 = blank/white displays for window/bkg 1 = enabled

    uint8_t LY{0};  //$FF44 current scanline value(0-153), in mode V_BLANK (144-153)
    uint8_t LYC{0}; //$FF45 when LYC == LY the flag in STAT is set, and (if enabled) STAT interrupt is requested
    uint8_t STAT{0};//$FF41
    //Bit 6: LYC int select (Read/Write): If set, selects the LYC == LY condition for the STAT interrupt.
    //Bit 5: Mode 2 int select (Read/Write): If set, selects the Mode 2 condition for the STAT interrupt.
    //Bit 4: Mode 1 int select (Read/Write): If set, selects the Mode 1 condition for the STAT interrupt.
    //Bit 3: Mode 0 int select (Read/Write): If set, selects the Mode 0 condition for the STAT interrupt.
    //Bit 2: LYC == LY (Read-only): Set when LY contains the same value as LYC; it is constantly updated.
    //Bits 0 & 1: PPU mode (Read-only): Indicates the PPU’s current status. 
                                       //Reports 0 instead when the PPU is disabled
    //Palettes:
    uint8_t BGP{0};  //$FF47 - Background Palette for bkg and window tiles
    uint8_t OBP0{0}; //$FF48 palette for sprites
    uint8_t OBP1{0}; //$FF49 palette for sprites
    //work exactly like BGP,except that the lower two bits are ignored because color index 0 is transparent for OBJs.
                     
    uint32_t cycleCounter{0}; //Counts in T-Cycles
    std::array<std::array<uint8_t,144> ,160> frameBuffer{}; //Final frame to be written 
    uint8_t windowLineCounter{0};
};

#endif // !PPU_HPP
