#include "ppu.hpp"
#include "gameboy.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <sys/stat.h>


Ppu::Ppu(GameBoy& gb) : gameboy(gb)
{
    spriteLineBuffer.reserve(10);
}

void Ppu::Step(uint32_t tCycles)
{
    if((LCDC & 0b10000000) == 0) 
    {
        LY = 0;
        windowLineCounter = 0;
        cycleCounter = 0;
        spriteLineBuffer.clear();
        ppuMode = Mode::H_BLANK;
        return;
    } //LCDC Bit 7 enables PPU and LCD

    CheckLYC();

    cycleCounter += tCycles;

    switch (ppuMode) 
    {
        case Mode::OAM_SCAN:
            {
                if(cycleCounter >= 80)
                {
                    cycleCounter -= 80;
                    ScanOAM();
                    ppuMode = Mode::DRAWING;
                }
                break;
            }
        case Mode::DRAWING:
            {
                if (cycleCounter >= 172) { // simplified: ignore variable timing 
                    cycleCounter -= 172;
                    RenderScanline();
                    ppuMode = Mode::H_BLANK;
                }
                break;
            }
        case Mode::H_BLANK:
            {
                if(cycleCounter >= 204) //456 - 80 - 172
                {
                    cycleCounter -= 204;
                    LY++;
                    if(LY == 144)
                    {
                        ppuMode = Mode::V_BLANK;
                        isFrameReady = true;
                        RequestVBLANK_INT();
                    }
                    else {ppuMode = Mode::OAM_SCAN;}
                }
                break;
            }
        case Mode::V_BLANK:
            {
                if(cycleCounter >= 456) 
                {
                    cycleCounter -= 456;
                    LY++;
                    if(LY > 153)
                    {
                        LY = 0;
                        windowLineCounter = 0;
                        ppuMode = Mode::OAM_SCAN;
                    }
                }
                break;
            }
    }
}

void Ppu::ScanOAM()
{
    uint8_t objHeight = (LCDC & 0b00000100) ? 16 : 8;
    uint16_t lyPlus16 = LY + 16;

    for (auto& obj : OAM)
    {
        if (spriteLineBuffer.size() == 10) break;
        if (obj.xPosition > 0 && lyPlus16 >= obj.yPosition && lyPlus16 < obj.yPosition + objHeight)
        {
            spriteLineBuffer.push_back(obj);
        }
    }
    std::stable_sort(spriteLineBuffer.begin(), spriteLineBuffer.end(),[](const Sprite& a, const Sprite& b) 
            { return a.xPosition < b.xPosition; }); 
}

void Ppu::CheckLYC()
{
    if((LCDC & 0b10000000) == 0) return;

    if(LY == LYC)
    {
        if((STAT & 0b01000000) && !(STAT & 0b00000100)) RequestSTAT_INT();   // LYC == LY is enabled and not already requested int
        STAT |= 0b00000100; // flag the int
    }
    else
    {
        STAT &= ~0b00000100; 
    }
}

void Ppu::RequestVBLANK_INT()
{
    uint8_t IF = gameboy.ReadFromMemory(IF_ADDR);
    IF = IF | 0x01; //Or-ing Bit 0 for V_BLANK interrupt
    gameboy.WriteToMemory(IF_ADDR, IF);
}

void Ppu::RequestSTAT_INT()
{
    uint8_t IF = gameboy.ReadFromMemory(IF_ADDR);
    IF = IF | 0b00000010; //Or-ing Bit 1 for STAT/LCD interrupt
    gameboy.WriteToMemory(IF_ADDR, IF);
}

void Ppu::RenderScanline()
{
    uint8_t bgLine[160];

    if((LCDC & 0b00000001) > 0)//Background
    {
        //If bkg is enabled -> check tile map and draw to frameBuffer on line LY using pos (scx, scy) and wrapping around when out of range

        auto& tileMap = ((LCDC & 0b00001000) > 0) ? tileMap1 : tileMap0; //choosing tilemaps based on LCDC
        for(uint8_t i = 0; i < frameBuffer.size(); i++)
        {
            uint8_t bgX = SCX + i; //Pixel Coords
            uint8_t bgY = SCY + LY;//Pixel Coords

            uint8_t tileIndexX = (bgX / 8) & 0xFF;
            uint8_t tileIndexY = (bgY / 8) & 0xFF;

            uint8_t tileIndex = tileMap[tileIndexX][tileIndexY];
            Tile tile = GetTileData(tileIndex,false);

            uint8_t colorID = ComputePixelColorID(tile[bgY % 8],bgX % 8);
            bgLine[i] = colorID;
            frameBuffer[i][LY] = ApplyPalette(BGP, colorID);
        }

        if((LCDC & 0b00100000) > 0 && LY >= WY && WX <= 166) //window enabled and reached it's first line
        {
            auto& tileMap = ((LCDC & 0b01000000) > 0) ? tileMap1 : tileMap0;

            int winY = windowLineCounter;

            for(int i = 0; i < 160; i++)           // i = x inside the window
            {
                int screenX = WX - 7 + i;          // where it lands on screen
                if(screenX < 0) continue;          // off the left edge
                if(screenX >= 160) break;          // off the right edge

                uint8_t tileIndexX = i / 8;
                uint8_t tileIndexY = winY / 8;

                uint8_t tileIndex = tileMap[tileIndexX][tileIndexY];
                Tile tile = GetTileData(tileIndex, false);

                uint8_t colorID = ComputePixelColorID(tile[winY % 8], i % 8);
                bgLine[screenX] = colorID;
                frameBuffer[screenX][LY] = ApplyPalette(BGP, colorID);
            }
            windowLineCounter++;
        }
    }
    else 
    {
        //Background & Window are disabled = White/Blank
        for(int x = 0; x < 160; x++)
        {
            bgLine[x] = 0;
            frameBuffer[x][LY] = 0;
        }
    }
    if((LCDC & 0b00000010) > 0)//Sprites
    {
        //If sprites are enabled -> check size and draw using spriteLineBuffer vector and check attributes
        for(uint8_t i = 0; i < frameBuffer.size(); i++) 
        {
            for(auto& sprite : spriteLineBuffer) //Checking which sprite belongs on this i pixel
            {
                int spriteCol = i - (sprite.xPosition - 8);
                int yPos = LY - (sprite.yPosition - 16); //0-7 / 0-15 in 8x16 mode
                if(spriteCol < 0 || spriteCol > 7) continue;
                if((LCDC & 0b00000100) > 0)//8x16 mode
                {
                    int row = yPos;
                    if((sprite.attributes & 0b01000000) > 0) {row = 15 - row;} //y flip
                    uint8_t spriteTileIndex = row > 7 ? (sprite.tileIndex | 1) : (sprite.tileIndex & 0xFE);
                    Tile tile = GetTileData(spriteTileIndex,true);

                    uint8_t xPixelColor = spriteCol;
                    uint8_t yPixelColor = row % 8;

                    if((sprite.attributes & 0b00100000) > 0) {xPixelColor = 7 - xPixelColor;} //X flip

                    uint8_t colorID = ComputePixelColorID(tile[yPixelColor],xPixelColor);
                    if(colorID == 0) continue;
                    if((sprite.attributes & 0b10000000) > 0 && bgLine[i] != 0) break;
                    uint8_t palette = ((sprite.attributes & 0b00010000) > 0) ? OBP1 : OBP0;
                    frameBuffer[i][LY] = ApplyPalette(palette, colorID);
                    break;
                }
                else //8x8 mode
                {
                    Tile tile = GetTileData(sprite.tileIndex,true);

                    uint8_t xPixelColor = spriteCol;
                    uint8_t yPixelColor = yPos % 8;

                    if((sprite.attributes & 0b00100000) > 0) {xPixelColor = 7 - xPixelColor;} //X flip
                    if((sprite.attributes & 0b01000000) > 0) {yPixelColor = 7 - yPixelColor;} //y flip

                    uint8_t colorID = ComputePixelColorID(tile[yPixelColor],xPixelColor);
                    if(colorID == 0) continue;
                    if((sprite.attributes & 0b10000000) > 0 && bgLine[i] != 0) break;
                    uint8_t palette = ((sprite.attributes & 0b00010000) > 0) ? OBP1 : OBP0;
                    frameBuffer[i][LY] = ApplyPalette(palette, colorID);
                    break;
                }
            }
        }
    }
    spriteLineBuffer.clear();
}

const Tile& Ppu::GetTileData(uint8_t tileIndex, bool isSprite)
{
    if(((LCDC & 0b00010000) > 0) || isSprite) //$8000 addressing mode
    {
        return tileData[tileIndex];
    }
    else //$8800 addressing mode
    {
        int8_t signedAddr = static_cast<int8_t>(tileIndex);
        return tileData[signedAddr + 256]; 
    }
};

uint8_t Ppu::ComputePixelColorID(const std::pair<uint8_t,uint8_t>& row, uint8_t bit)
{
    uint8_t n  = 7 - bit;                 // bit = pixel column inside the tile (0-7)
    uint8_t lo = (row.first  >> n) & 1;
    uint8_t hi = (row.second >> n) & 1;
    return (hi << 1) | lo;                // 0..3
}
