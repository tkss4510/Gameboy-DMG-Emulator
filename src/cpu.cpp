#include "cpu.hpp"
#include <cstdint>
#include <iostream>
#include "gameboy.hpp"

Cpu::Cpu(GameBoy& gb) : gameboy(gb)
{
    uint8_t checksum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; addr++) 
    {
        checksum = checksum - gameboy.ReadFromMemory(addr) - 1;
    }
    Init(checksum != 0);
    std::cout << std::hex << PC;
}

void Cpu::Init(bool isChecksumHeaderNonZero)
{
    AF.bytes.Hi = 0b00000001;
    if(isChecksumHeaderNonZero)
    {
        AF.bytes.Lo = 0b10110000;
    }
    else 
    {
        AF.bytes.Lo = 0b10000000;
    }

    BC.bytes.Hi = 0b00000000;
    BC.bytes.Lo = 0b00010011;

    DE.bytes.Hi = 0b00000000;
    DE.bytes.Lo = 0b11011000;

    HL.bytes.Hi = 0b00000001;
    HL.bytes.Lo = 0b01001101;

    PC = 0b0000000100000000;
    SP = 0b1111111111111110;
    IME = false;
    gameboy.WriteToMemory(IF_addr,0x00);
    gameboy.WriteToMemory(IE_addr,0x00);
    halted = false;
    stopped = false;
    halt_bug_active = false;
}

uint8_t Cpu::Step()
{
    if(stopped)
    {
        if (gameboy.ReadFromMemory(IF_addr) & 0x10) stopped = false;  // button press wakes
        else return 0;
    }
    if (IME)
    {
        uint8_t IF = gameboy.ReadFromMemory(IF_addr);
        uint8_t IE = gameboy.ReadFromMemory(IE_addr);
        uint8_t pending = IF & IE;

        uint16_t vector = 0;
        uint8_t bit = 0;
        if (pending & 0b00000001)      { vector = 0x40; bit = 0b00000001; }
        else if (pending & 0b00000010) { vector = 0x48; bit = 0b00000010; }
        else if (pending & 0b00000100) { vector = 0x50; bit = 0b00000100; }
        else if (pending & 0b00001000) { vector = 0x58; bit = 0b00001000; }
        else if (pending & 0b00010000) { vector = 0x60; bit = 0b00010000; }

        if (vector != 0)
        {
            IME = false;
            halted = false;
            gameboy.WriteToMemory(IF_addr, IF & ~bit);
            DispatchInterrupt(vector);
            return 5;
        }
    }
    if (halted)
    {
        uint8_t IF = gameboy.ReadFromMemory(IF_addr);
        uint8_t IE = gameboy.ReadFromMemory(IE_addr);
        uint8_t pending = IF & IE;

        if (pending != 0)
        {
            halted = false;
            return 1; // woke up this cycle, interrupt (if any) services next Step()
        }
        return 1; //still need to advance clock
    }

    bool apply_ime_now = ime_scheduled; // was EI executed last instruction?
    ime_scheduled = false;
    uint8_t opcode = fetch();
    uint8_t m_cycles = [&]()
    {
        switch (opcode) 
        {
            case 0x00: return NOP();break;
            case 0x01: return LDr16n16(BC.HiLo);break;
            case 0x02: return LDa16r8(BC.HiLo, AF.bytes.Hi);break;
            case 0x03: return INCr16(BC.HiLo);break;
            case 0x04: return INCr8(BC.bytes.Hi);break;
            case 0x05: return DECr8(BC.bytes.Hi);break;
            case 0x06: return LDr8n8(BC.bytes.Hi, fetch());break;
            case 0x07: return RLCA();break;
            case 0x08: return LDa16SP(SP);break;
            case 0x09: return ADDr16r16(HL.HiLo, BC.HiLo);break;
            case 0x0A: return LDr8a16(AF.bytes.Hi,BC.HiLo);break;
            case 0x0B: return DECr16(BC.HiLo);break;
            case 0x0C: return INCr8(BC.bytes.Lo);break;
            case 0x0D: return DECr8(BC.bytes.Lo);break;
            case 0x0E: return LDr8n8(BC.bytes.Lo, fetch()); break;
            case 0x0F: return RRCA();break;
            case 0x10: return STOP();break; 
            case 0x11: return LDr16n16(DE.HiLo);break;
            case 0x12: return LDa16r8(DE.HiLo, AF.bytes.Hi);break;
            case 0x13: return INCr16(DE.HiLo);break;
            case 0x14: return INCr8(DE.bytes.Hi);break;
            case 0x15: return DECr8(DE.bytes.Hi);break;
            case 0x16: return LDr8n8(DE.bytes.Hi, fetch());break;
            case 0x17: return RLA();break;
            case 0x18: return JRe8();break;
            case 0x19: return ADDr16r16(HL.HiLo,DE.HiLo);break;
            case 0x1A: return LDr8a16(AF.bytes.Hi,DE.HiLo);break;
            case 0x1B: return DECr16(DE.HiLo);break;
            case 0x1C: return INCr8(DE.bytes.Lo);break;
            case 0x1D: return DECr8(DE.bytes.Lo);break;
            case 0x1E: return LDr8n8(DE.bytes.Lo,fetch()); break;
            case 0x1F: return RRA();break;
            case 0x20: return JRNZe8();break;
            case 0x21: return LDr16n16(HL.HiLo);break;
            case 0x22: return LDIa16r8(HL.HiLo, AF.bytes.Hi);break;
            case 0x23: return INCr16(HL.HiLo);break;
            case 0x24: return INCr8(HL.bytes.Hi);break;
            case 0x25: return DECr8(HL.bytes.Hi);break;
            case 0x26: return LDr8n8(HL.bytes.Hi, fetch()); break;
            case 0x27: return DAA();break;
            case 0x28: return JRZe8();break;
            case 0x29: return ADDr16r16(HL.HiLo,HL.HiLo);break;
            case 0x2A: return LDIr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0x2B: return DECr16(HL.HiLo);break;
            case 0x2C: return INCr8(HL.bytes.Lo);break;
            case 0x2D: return DECr8(HL.bytes.Lo);break;
            case 0x2E: return LDr8n8(HL.bytes.Lo,fetch()); break;
            case 0x2F: return CPL();break;
            case 0x30: return JRNCe8();break;
            case 0x31: return LDr16n16(SP);break;
            case 0x32: return LDDa16r8(HL.HiLo, AF.bytes.Hi);break;
            case 0x33: return INCr16(SP);break;
            case 0x34: return INCa16(HL.HiLo);break;
            case 0x35: return DECa16(HL.HiLo);break;
            case 0x36: return LDa16n8(HL.HiLo, fetch());break;
            case 0x37: return SCF();break;
            case 0x38: return JRCe8();break;
            case 0x39: return ADDr16r16(HL.HiLo,SP);break;
            case 0x3A: return LDDr8a16(AF.bytes.Hi,HL.HiLo);break;
            case 0x3B: return DECr16(SP);break;
            case 0x3C: return INCr8(AF.bytes.Hi);break;
            case 0x3D: return DECr8(AF.bytes.Hi);break;
            case 0x3E: return LDr8n8(AF.bytes.Hi, fetch());break;
            case 0x3F: return CCF();break;
            case 0x40: return LDr8r8(BC.bytes.Hi, BC.bytes.Hi);break;
            case 0x41: return LDr8r8(BC.bytes.Hi, BC.bytes.Lo);break;
            case 0x42: return LDr8r8(BC.bytes.Hi, DE.bytes.Hi);break;
            case 0x43: return LDr8r8(BC.bytes.Hi, DE.bytes.Lo);break;
            case 0x44: return LDr8r8(BC.bytes.Hi, HL.bytes.Hi);break;
            case 0x45: return LDr8r8(BC.bytes.Hi, HL.bytes.Lo);break;
            case 0x46: return LDr8a16(BC.bytes.Hi, HL.HiLo);break;
            case 0x47: return LDr8r8(BC.bytes.Hi, AF.bytes.Hi);break;
            case 0x48: return LDr8r8(BC.bytes.Lo, BC.bytes.Hi);break;
            case 0x49: return LDr8r8(BC.bytes.Lo, BC.bytes.Lo);break;
            case 0x4A: return LDr8r8(BC.bytes.Lo, DE.bytes.Hi);break;
            case 0x4B: return LDr8r8(BC.bytes.Lo, DE.bytes.Lo);break;
            case 0x4C: return LDr8r8(BC.bytes.Lo, HL.bytes.Hi);break;
            case 0x4D: return LDr8r8(BC.bytes.Lo, HL.bytes.Lo);break;
            case 0x4E: return LDr8a16(BC.bytes.Lo, HL.HiLo);break;
            case 0x4F: return LDr8r8(BC.bytes.Lo, AF.bytes.Hi);break;
            case 0x50: return LDr8r8(DE.bytes.Hi, BC.bytes.Hi);break;
            case 0x51: return LDr8r8(DE.bytes.Hi, BC.bytes.Lo);break;
            case 0x52: return LDr8r8(DE.bytes.Hi, DE.bytes.Hi);break;
            case 0x53: return LDr8r8(DE.bytes.Hi, DE.bytes.Lo);break;
            case 0x54: return LDr8r8(DE.bytes.Hi, HL.bytes.Hi);break;
            case 0x55: return LDr8r8(DE.bytes.Hi, HL.bytes.Lo);break;
            case 0x56: return LDr8a16(DE.bytes.Hi, HL.HiLo);break;
            case 0x57: return LDr8r8(DE.bytes.Hi, AF.bytes.Hi);break;
            case 0x58: return LDr8r8(DE.bytes.Lo, BC.bytes.Hi);break;
            case 0x59: return LDr8r8(DE.bytes.Lo, BC.bytes.Lo);break;
            case 0x5A: return LDr8r8(DE.bytes.Lo, DE.bytes.Hi);break;
            case 0x5B: return LDr8r8(DE.bytes.Lo, DE.bytes.Lo);break;
            case 0x5C: return LDr8r8(DE.bytes.Lo, HL.bytes.Hi);break;
            case 0x5D: return LDr8r8(DE.bytes.Lo, HL.bytes.Lo);break;
            case 0x5E: return LDr8a16(DE.bytes.Lo, HL.HiLo);break;
            case 0x5F: return LDr8r8(DE.bytes.Lo, AF.bytes.Hi);break;
            case 0x60: return LDr8r8(HL.bytes.Hi, BC.bytes.Hi);break;
            case 0x61: return LDr8r8(HL.bytes.Hi, BC.bytes.Lo);break;
            case 0x62: return LDr8r8(HL.bytes.Hi, DE.bytes.Hi);break;
            case 0x63: return LDr8r8(HL.bytes.Hi, DE.bytes.Lo);break;
            case 0x64: return LDr8r8(HL.bytes.Hi, HL.bytes.Hi);break;
            case 0x65: return LDr8r8(HL.bytes.Hi, HL.bytes.Lo);break;
            case 0x66: return LDr8a16(HL.bytes.Hi, HL.HiLo);break;
            case 0x67: return LDr8r8(HL.bytes.Hi, AF.bytes.Hi);break;
            case 0x68: return LDr8r8(HL.bytes.Lo, BC.bytes.Hi);break;
            case 0x69: return LDr8r8(HL.bytes.Lo, BC.bytes.Lo);break;
            case 0x6A: return LDr8r8(HL.bytes.Lo, DE.bytes.Hi);break;
            case 0x6B: return LDr8r8(HL.bytes.Lo, DE.bytes.Lo);break;
            case 0x6C: return LDr8r8(HL.bytes.Lo, HL.bytes.Hi);break;
            case 0x6D: return LDr8r8(HL.bytes.Lo, HL.bytes.Lo);break;
            case 0x6E: return LDr8a16(HL.bytes.Lo, HL.HiLo);break;
            case 0x6F: return LDr8r8(HL.bytes.Lo, AF.bytes.Hi);break;
            case 0x70: return LDa16r8(HL.HiLo,BC.bytes.Hi);break;
            case 0x71: return LDa16r8(HL.HiLo,BC.bytes.Lo);break;
            case 0x72: return LDa16r8(HL.HiLo,DE.bytes.Hi);break;
            case 0x73: return LDa16r8(HL.HiLo,DE.bytes.Lo);break;
            case 0x74: return LDa16r8(HL.HiLo,HL.bytes.Hi);break;
            case 0x75: return LDa16r8(HL.HiLo,HL.bytes.Lo);break;
            case 0x76: return HALT(); break;
            case 0x77: return LDa16r8(HL.HiLo,AF.bytes.Hi);break;
            case 0x78: return LDr8r8(AF.bytes.Hi,BC.bytes.Hi);break;
            case 0x79: return LDr8r8(AF.bytes.Hi,BC.bytes.Lo);break;
            case 0x7A: return LDr8r8(AF.bytes.Hi,DE.bytes.Hi);break;
            case 0x7B: return LDr8r8(AF.bytes.Hi,DE.bytes.Lo);break;
            case 0x7C: return LDr8r8(AF.bytes.Hi,HL.bytes.Hi);break;
            case 0x7D: return LDr8r8(AF.bytes.Hi,HL.bytes.Lo);break;
            case 0x7E: return LDr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0x7F: return LDr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0x80: return ADDr8r8(AF.bytes.Hi, BC.bytes.Hi);break;
            case 0x81: return ADDr8r8(AF.bytes.Hi, BC.bytes.Lo);break;
            case 0x82: return ADDr8r8(AF.bytes.Hi, DE.bytes.Hi);break;
            case 0x83: return ADDr8r8(AF.bytes.Hi, DE.bytes.Lo);break;
            case 0x84: return ADDr8r8(AF.bytes.Hi, HL.bytes.Hi);break;
            case 0x85: return ADDr8r8(AF.bytes.Hi, HL.bytes.Lo);break;
            case 0x86: return ADDr8a16(AF.bytes.Hi,HL.HiLo);break;
            case 0x87: return ADDr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0x88: return ADCr8r8(AF.bytes.Hi, BC.bytes.Hi);break;
            case 0x89: return ADCr8r8(AF.bytes.Hi, BC.bytes.Lo);break;
            case 0x8A: return ADCr8r8(AF.bytes.Hi, DE.bytes.Hi);break;
            case 0x8B: return ADCr8r8(AF.bytes.Hi, DE.bytes.Lo);break;
            case 0x8C: return ADCr8r8(AF.bytes.Hi, HL.bytes.Hi);break;
            case 0x8D: return ADCr8r8(AF.bytes.Hi, HL.bytes.Lo);break;
            case 0x8E: return ADCr8a16(AF.bytes.Hi,HL.HiLo);break;
            case 0x8F: return ADCr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0x90: return SUBr8r8(AF.bytes.Hi,BC.bytes.Hi);break;
            case 0x91: return SUBr8r8(AF.bytes.Hi,BC.bytes.Lo);break;
            case 0x92: return SUBr8r8(AF.bytes.Hi,DE.bytes.Hi);break;
            case 0x93: return SUBr8r8(AF.bytes.Hi,DE.bytes.Lo);break;
            case 0x94: return SUBr8r8(AF.bytes.Hi,HL.bytes.Hi);break;
            case 0x95: return SUBr8r8(AF.bytes.Hi,HL.bytes.Lo);break;
            case 0x96: return SUBr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0x97: return SUBr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0x98: return SBCr8r8(AF.bytes.Hi,BC.bytes.Hi);break;
            case 0x99: return SBCr8r8(AF.bytes.Hi,BC.bytes.Lo);break;
            case 0x9A: return SBCr8r8(AF.bytes.Hi,DE.bytes.Hi);break;
            case 0x9B: return SBCr8r8(AF.bytes.Hi,DE.bytes.Lo);break;
            case 0x9C: return SBCr8r8(AF.bytes.Hi,HL.bytes.Hi);break;
            case 0x9D: return SBCr8r8(AF.bytes.Hi,HL.bytes.Lo);break;
            case 0x9E: return SBCr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0x9F: return SBCr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0xA0: return ANDr8r8(AF.bytes.Hi,BC.bytes.Hi);break;
            case 0xA1: return ANDr8r8(AF.bytes.Hi,BC.bytes.Lo);break;
            case 0xA2: return ANDr8r8(AF.bytes.Hi,DE.bytes.Hi);break;
            case 0xA3: return ANDr8r8(AF.bytes.Hi,DE.bytes.Lo);break;
            case 0xA4: return ANDr8r8(AF.bytes.Hi,HL.bytes.Hi);break;
            case 0xA5: return ANDr8r8(AF.bytes.Hi,HL.bytes.Lo);break;
            case 0xA6: return ANDr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0xA7: return ANDr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0xA8: return XORr8r8(AF.bytes.Hi,BC.bytes.Hi);break;
            case 0xA9: return XORr8r8(AF.bytes.Hi,BC.bytes.Lo);break;
            case 0xAA: return XORr8r8(AF.bytes.Hi,DE.bytes.Hi);break;
            case 0xAB: return XORr8r8(AF.bytes.Hi,DE.bytes.Lo);break;
            case 0xAC: return XORr8r8(AF.bytes.Hi,HL.bytes.Hi);break;
            case 0xAD: return XORr8r8(AF.bytes.Hi,HL.bytes.Lo);break;
            case 0xAE: return XORr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0xAF: return XORr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0xB0: return ORr8r8(AF.bytes.Hi,BC.bytes.Hi);break;
            case 0xB1: return ORr8r8(AF.bytes.Hi,BC.bytes.Lo);break;
            case 0xB2: return ORr8r8(AF.bytes.Hi,DE.bytes.Hi);break;
            case 0xB3: return ORr8r8(AF.bytes.Hi,DE.bytes.Lo);break;
            case 0xB4: return ORr8r8(AF.bytes.Hi,HL.bytes.Hi);break;
            case 0xB5: return ORr8r8(AF.bytes.Hi,HL.bytes.Lo);break;
            case 0xB6: return ORr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0xB7: return ORr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0xB8: return CPr8r8(AF.bytes.Hi, BC.bytes.Hi);break;
            case 0xB9: return CPr8r8(AF.bytes.Hi, BC.bytes.Lo);break;
            case 0xBA: return CPr8r8(AF.bytes.Hi, DE.bytes.Hi);break;
            case 0xBB: return CPr8r8(AF.bytes.Hi, DE.bytes.Lo);break;
            case 0xBC: return CPr8r8(AF.bytes.Hi, HL.bytes.Hi);break;
            case 0xBD: return CPr8r8(AF.bytes.Hi, HL.bytes.Lo);break;
            case 0xBE: return CPr8a16(AF.bytes.Hi, HL.HiLo);break;
            case 0xBF: return CPr8r8(AF.bytes.Hi,AF.bytes.Hi);break;
            case 0xC0: return RETNZ();break;
            case 0xC1: return POPr16(BC.HiLo);break;
            case 0xC2: return JPNZn16();break;
            case 0xC3: return JPn16();break;
            case 0xC4: return CALLNZn16();break;
            case 0xC5: return PUSHr16(BC.HiLo);break;
            case 0xC6: return ADDr8n8(AF.bytes.Hi);break;
            case 0xC7: return RST(0x00);break;
            case 0xC8: return RETZ();break;
            case 0xC9: return RET();break;
            case 0xCA: return JPZn16();break;
            case 0xCB: 
                       {
                           uint8_t cbOpcode = fetch();
                           switch (cbOpcode) 
                           {
                               case 0x00: return RLCr8(BC.bytes.Hi);break;
                               case 0x01: return RLCr8(BC.bytes.Lo);break;
                               case 0x02: return RLCr8(DE.bytes.Hi);break;
                               case 0x03: return RLCr8(DE.bytes.Lo);break;
                               case 0x04: return RLCr8(HL.bytes.Hi);break;
                               case 0x05: return RLCr8(HL.bytes.Lo);break;
                               case 0x06: return RLCa16(HL.HiLo);break;
                               case 0x07: return RLCr8(AF.bytes.Hi);break;
                               case 0x08: return RRCr8(BC.bytes.Hi);break;
                               case 0x09: return RRCr8(BC.bytes.Lo);break;
                               case 0x0A: return RRCr8(DE.bytes.Hi);break;
                               case 0x0B: return RRCr8(DE.bytes.Lo);break;
                               case 0x0C: return RRCr8(HL.bytes.Hi);break;
                               case 0x0D: return RRCr8(HL.bytes.Lo);break;
                               case 0x0E: return RRCa16(HL.HiLo);break;
                               case 0x0F: return RRCr8(AF.bytes.Hi);break;
                               case 0x10: return RLr8(BC.bytes.Hi);break;
                               case 0x11: return RLr8(BC.bytes.Lo);break;
                               case 0x12: return RLr8(DE.bytes.Hi);break;
                               case 0x13: return RLr8(DE.bytes.Lo);break;
                               case 0x14: return RLr8(HL.bytes.Hi);break;
                               case 0x15: return RLr8(HL.bytes.Lo);break;
                               case 0x16: return RLa16(HL.HiLo);break;
                               case 0x17: return RLr8(AF.bytes.Hi);break;
                               case 0x18: return RRr8(BC.bytes.Hi);break;
                               case 0x19: return RRr8(BC.bytes.Lo);break;
                               case 0x1A: return RRr8(DE.bytes.Hi);break;
                               case 0x1B: return RRr8(DE.bytes.Lo);break;
                               case 0x1C: return RRr8(HL.bytes.Hi);break;
                               case 0x1D: return RRr8(HL.bytes.Lo);break;
                               case 0x1E: return RRa16(HL.HiLo);break;
                               case 0x1F: return RRr8(AF.bytes.Hi);break;
                               case 0x20: return SLAr8(BC.bytes.Hi);break;
                               case 0x21: return SLAr8(BC.bytes.Lo);break;
                               case 0x22: return SLAr8(DE.bytes.Hi);break;
                               case 0x23: return SLAr8(DE.bytes.Lo);break;
                               case 0x24: return SLAr8(HL.bytes.Hi);break;
                               case 0x25: return SLAr8(HL.bytes.Lo);break;
                               case 0x26: return SLAa16(HL.HiLo);break;
                               case 0x27: return SLAr8(AF.bytes.Hi);break;
                               case 0x28: return SRAr8(BC.bytes.Hi);break;
                               case 0x29: return SRAr8(BC.bytes.Lo);break;
                               case 0x2A: return SRAr8(DE.bytes.Hi);break;
                               case 0x2B: return SRAr8(DE.bytes.Lo);break;
                               case 0x2C: return SRAr8(HL.bytes.Hi);break;
                               case 0x2D: return SRAr8(HL.bytes.Lo);break;
                               case 0x2E: return SRAa16(HL.HiLo);break;
                               case 0x2F: return SRAr8(AF.bytes.Hi);break;
                               case 0x30: return SWAPr8(BC.bytes.Hi);break;
                               case 0x31: return SWAPr8(BC.bytes.Lo);break;
                               case 0x32: return SWAPr8(DE.bytes.Hi);break;
                               case 0x33: return SWAPr8(DE.bytes.Lo);break;
                               case 0x34: return SWAPr8(HL.bytes.Hi);break;
                               case 0x35: return SWAPr8(HL.bytes.Lo);break;
                               case 0x36: return SWAPa16(HL.HiLo);break;
                               case 0x37: return SWAPr8(AF.bytes.Hi);break;
                               case 0x38: return SRLr8(BC.bytes.Hi);break;
                               case 0x39: return SRLr8(BC.bytes.Lo);break;
                               case 0x3A: return SRLr8(DE.bytes.Hi);break;
                               case 0x3B: return SRLr8(DE.bytes.Lo);break;
                               case 0x3C: return SRLr8(HL.bytes.Hi);break;
                               case 0x3D: return SRLr8(HL.bytes.Lo);break;
                               case 0x3E: return SRLa16(HL.HiLo);break;
                               case 0x3F: return SRLr8(AF.bytes.Hi);break;
                               case 0x40: return BITu3r8(0, BC.bytes.Hi); break;
                               case 0x41: return BITu3r8(0, BC.bytes.Lo); break;
                               case 0x42: return BITu3r8(0, DE.bytes.Hi); break;
                               case 0x43: return BITu3r8(0, DE.bytes.Lo); break;
                               case 0x44: return BITu3r8(0, HL.bytes.Hi); break;
                               case 0x45: return BITu3r8(0, HL.bytes.Lo); break;
                               case 0x46: return BITu3a16(0, HL.HiLo); break;
                               case 0x47: return BITu3r8(0, AF.bytes.Hi); break;
                               case 0x48: return BITu3r8(1, BC.bytes.Hi); break;
                               case 0x49: return BITu3r8(1, BC.bytes.Lo); break;
                               case 0x4A: return BITu3r8(1, DE.bytes.Hi); break;
                               case 0x4B: return BITu3r8(1, DE.bytes.Lo); break;
                               case 0x4C: return BITu3r8(1, HL.bytes.Hi); break;
                               case 0x4D: return BITu3r8(1, HL.bytes.Lo); break;
                               case 0x4E: return BITu3a16(1, HL.HiLo); break;
                               case 0x4F: return BITu3r8(1, AF.bytes.Hi); break;
                               case 0x50: return BITu3r8(2, BC.bytes.Hi); break;
                               case 0x51: return BITu3r8(2, BC.bytes.Lo); break;
                               case 0x52: return BITu3r8(2, DE.bytes.Hi); break;
                               case 0x53: return BITu3r8(2, DE.bytes.Lo); break;
                               case 0x54: return BITu3r8(2, HL.bytes.Hi); break;
                               case 0x55: return BITu3r8(2, HL.bytes.Lo); break;
                               case 0x56: return BITu3a16(2, HL.HiLo); break;
                               case 0x57: return BITu3r8(2, AF.bytes.Hi); break;
                               case 0x58: return BITu3r8(3, BC.bytes.Hi); break;
                               case 0x59: return BITu3r8(3, BC.bytes.Lo); break;
                               case 0x5A: return BITu3r8(3, DE.bytes.Hi); break;
                               case 0x5B: return BITu3r8(3, DE.bytes.Lo); break;
                               case 0x5C: return BITu3r8(3, HL.bytes.Hi); break;
                               case 0x5D: return BITu3r8(3, HL.bytes.Lo); break;
                               case 0x5E: return BITu3a16(3, HL.HiLo); break;
                               case 0x5F: return BITu3r8(3, AF.bytes.Hi); break;
                               case 0x60: return BITu3r8(4, BC.bytes.Hi); break;
                               case 0x61: return BITu3r8(4, BC.bytes.Lo); break;
                               case 0x62: return BITu3r8(4, DE.bytes.Hi); break;
                               case 0x63: return BITu3r8(4, DE.bytes.Lo); break;
                               case 0x64: return BITu3r8(4, HL.bytes.Hi); break;
                               case 0x65: return BITu3r8(4, HL.bytes.Lo); break;
                               case 0x66: return BITu3a16(4, HL.HiLo); break;
                               case 0x67: return BITu3r8(4, AF.bytes.Hi); break;
                               case 0x68: return BITu3r8(5, BC.bytes.Hi); break;
                               case 0x69: return BITu3r8(5, BC.bytes.Lo); break;
                               case 0x6A: return BITu3r8(5, DE.bytes.Hi); break;
                               case 0x6B: return BITu3r8(5, DE.bytes.Lo); break;
                               case 0x6C: return BITu3r8(5, HL.bytes.Hi); break;
                               case 0x6D: return BITu3r8(5, HL.bytes.Lo); break;
                               case 0x6E: return BITu3a16(5, HL.HiLo); break;
                               case 0x6F: return BITu3r8(5, AF.bytes.Hi); break;
                               case 0x70: return BITu3r8(6, BC.bytes.Hi); break;
                               case 0x71: return BITu3r8(6, BC.bytes.Lo); break;
                               case 0x72: return BITu3r8(6, DE.bytes.Hi); break;
                               case 0x73: return BITu3r8(6, DE.bytes.Lo); break;
                               case 0x74: return BITu3r8(6, HL.bytes.Hi); break;
                               case 0x75: return BITu3r8(6, HL.bytes.Lo); break;
                               case 0x76: return BITu3a16(6, HL.HiLo); break;
                               case 0x77: return BITu3r8(6, AF.bytes.Hi); break;
                               case 0x78: return BITu3r8(7, BC.bytes.Hi); break;
                               case 0x79: return BITu3r8(7, BC.bytes.Lo); break;
                               case 0x7A: return BITu3r8(7, DE.bytes.Hi); break;
                               case 0x7B: return BITu3r8(7, DE.bytes.Lo); break;
                               case 0x7C: return BITu3r8(7, HL.bytes.Hi); break;
                               case 0x7D: return BITu3r8(7, HL.bytes.Lo); break;
                               case 0x7E: return BITu3a16(7, HL.HiLo); break;
                               case 0x7F: return BITu3r8(7, AF.bytes.Hi); break;
                               case 0x80: return RESu3r8(0, BC.bytes.Hi); break;
                               case 0x81: return RESu3r8(0, BC.bytes.Lo); break;
                               case 0x82: return RESu3r8(0, DE.bytes.Hi); break;
                               case 0x83: return RESu3r8(0, DE.bytes.Lo); break;
                               case 0x84: return RESu3r8(0, HL.bytes.Hi); break;
                               case 0x85: return RESu3r8(0, HL.bytes.Lo); break;
                               case 0x86: return RESu3a16(0, HL.HiLo); break;
                               case 0x87: return RESu3r8(0, AF.bytes.Hi); break;
                               case 0x88: return RESu3r8(1, BC.bytes.Hi); break;
                               case 0x89: return RESu3r8(1, BC.bytes.Lo); break;
                               case 0x8A: return RESu3r8(1, DE.bytes.Hi); break;
                               case 0x8B: return RESu3r8(1, DE.bytes.Lo); break;
                               case 0x8C: return RESu3r8(1, HL.bytes.Hi); break;
                               case 0x8D: return RESu3r8(1, HL.bytes.Lo); break;
                               case 0x8E: return RESu3a16(1, HL.HiLo); break;
                               case 0x8F: return RESu3r8(1, AF.bytes.Hi); break;
                               case 0x90: return RESu3r8(2, BC.bytes.Hi); break;
                               case 0x91: return RESu3r8(2, BC.bytes.Lo); break;
                               case 0x92: return RESu3r8(2, DE.bytes.Hi); break;
                               case 0x93: return RESu3r8(2, DE.bytes.Lo); break;
                               case 0x94: return RESu3r8(2, HL.bytes.Hi); break;
                               case 0x95: return RESu3r8(2, HL.bytes.Lo); break;
                               case 0x96: return RESu3a16(2, HL.HiLo); break;
                               case 0x97: return RESu3r8(2, AF.bytes.Hi); break;
                               case 0x98: return RESu3r8(3, BC.bytes.Hi); break;
                               case 0x99: return RESu3r8(3, BC.bytes.Lo); break;
                               case 0x9A: return RESu3r8(3, DE.bytes.Hi); break;
                               case 0x9B: return RESu3r8(3, DE.bytes.Lo); break;
                               case 0x9C: return RESu3r8(3, HL.bytes.Hi); break;
                               case 0x9D: return RESu3r8(3, HL.bytes.Lo); break;
                               case 0x9E: return RESu3a16(3, HL.HiLo); break;
                               case 0x9F: return RESu3r8(3, AF.bytes.Hi); break;
                               case 0xA0: return RESu3r8(4, BC.bytes.Hi); break;
                               case 0xA1: return RESu3r8(4, BC.bytes.Lo); break;
                               case 0xA2: return RESu3r8(4, DE.bytes.Hi); break;
                               case 0xA3: return RESu3r8(4, DE.bytes.Lo); break;
                               case 0xA4: return RESu3r8(4, HL.bytes.Hi); break;
                               case 0xA5: return RESu3r8(4, HL.bytes.Lo); break;
                               case 0xA6: return RESu3a16(4, HL.HiLo); break;
                               case 0xA7: return RESu3r8(4, AF.bytes.Hi); break;
                               case 0xA8: return RESu3r8(5, BC.bytes.Hi); break;
                               case 0xA9: return RESu3r8(5, BC.bytes.Lo); break;
                               case 0xAA: return RESu3r8(5, DE.bytes.Hi); break;
                               case 0xAB: return RESu3r8(5, DE.bytes.Lo); break;
                               case 0xAC: return RESu3r8(5, HL.bytes.Hi); break;
                               case 0xAD: return RESu3r8(5, HL.bytes.Lo); break;
                               case 0xAE: return RESu3a16(5, HL.HiLo); break;
                               case 0xAF: return RESu3r8(5, AF.bytes.Hi); break;
                               case 0xB0: return RESu3r8(6, BC.bytes.Hi); break;
                               case 0xB1: return RESu3r8(6, BC.bytes.Lo); break;
                               case 0xB2: return RESu3r8(6, DE.bytes.Hi); break;
                               case 0xB3: return RESu3r8(6, DE.bytes.Lo); break;
                               case 0xB4: return RESu3r8(6, HL.bytes.Hi); break;
                               case 0xB5: return RESu3r8(6, HL.bytes.Lo); break;
                               case 0xB6: return RESu3a16(6, HL.HiLo); break;
                               case 0xB7: return RESu3r8(6, AF.bytes.Hi); break;
                               case 0xB8: return RESu3r8(7, BC.bytes.Hi); break;
                               case 0xB9: return RESu3r8(7, BC.bytes.Lo); break;
                               case 0xBA: return RESu3r8(7, DE.bytes.Hi); break;
                               case 0xBB: return RESu3r8(7, DE.bytes.Lo); break;
                               case 0xBC: return RESu3r8(7, HL.bytes.Hi); break;
                               case 0xBD: return RESu3r8(7, HL.bytes.Lo); break;
                               case 0xBE: return RESu3a16(7, HL.HiLo); break;
                               case 0xBF: return RESu3r8(7, AF.bytes.Hi); break;
                               case 0xC0: return SETu3r8(0, BC.bytes.Hi); break;
                               case 0xC1: return SETu3r8(0, BC.bytes.Lo); break;
                               case 0xC2: return SETu3r8(0, DE.bytes.Hi); break;
                               case 0xC3: return SETu3r8(0, DE.bytes.Lo); break;
                               case 0xC4: return SETu3r8(0, HL.bytes.Hi); break;
                               case 0xC5: return SETu3r8(0, HL.bytes.Lo); break;
                               case 0xC6: return SETu3a16(0, HL.HiLo); break;
                               case 0xC7: return SETu3r8(0, AF.bytes.Hi); break;
                               case 0xC8: return SETu3r8(1, BC.bytes.Hi); break;
                               case 0xC9: return SETu3r8(1, BC.bytes.Lo); break;
                               case 0xCA: return SETu3r8(1, DE.bytes.Hi); break;
                               case 0xCB: return SETu3r8(1, DE.bytes.Lo); break;
                               case 0xCC: return SETu3r8(1, HL.bytes.Hi); break;
                               case 0xCD: return SETu3r8(1, HL.bytes.Lo); break;
                               case 0xCE: return SETu3a16(1, HL.HiLo); break;
                               case 0xCF: return SETu3r8(1, AF.bytes.Hi); break;
                               case 0xD0: return SETu3r8(2, BC.bytes.Hi); break;
                               case 0xD1: return SETu3r8(2, BC.bytes.Lo); break;
                               case 0xD2: return SETu3r8(2, DE.bytes.Hi); break;
                               case 0xD3: return SETu3r8(2, DE.bytes.Lo); break;
                               case 0xD4: return SETu3r8(2, HL.bytes.Hi); break;
                               case 0xD5: return SETu3r8(2, HL.bytes.Lo); break;
                               case 0xD6: return SETu3a16(2, HL.HiLo); break;
                               case 0xD7: return SETu3r8(2, AF.bytes.Hi); break;
                               case 0xD8: return SETu3r8(3, BC.bytes.Hi); break;
                               case 0xD9: return SETu3r8(3, BC.bytes.Lo); break;
                               case 0xDA: return SETu3r8(3, DE.bytes.Hi); break;
                               case 0xDB: return SETu3r8(3, DE.bytes.Lo); break;
                               case 0xDC: return SETu3r8(3, HL.bytes.Hi); break;
                               case 0xDD: return SETu3r8(3, HL.bytes.Lo); break;
                               case 0xDE: return SETu3a16(3, HL.HiLo); break;
                               case 0xDF: return SETu3r8(3, AF.bytes.Hi); break;
                               case 0xE0: return SETu3r8(4, BC.bytes.Hi); break;
                               case 0xE1: return SETu3r8(4, BC.bytes.Lo); break;
                               case 0xE2: return SETu3r8(4, DE.bytes.Hi); break;
                               case 0xE3: return SETu3r8(4, DE.bytes.Lo); break;
                               case 0xE4: return SETu3r8(4, HL.bytes.Hi); break;
                               case 0xE5: return SETu3r8(4, HL.bytes.Lo); break;
                               case 0xE6: return SETu3a16(4, HL.HiLo); break;
                               case 0xE7: return SETu3r8(4, AF.bytes.Hi); break;
                               case 0xE8: return SETu3r8(5, BC.bytes.Hi); break;
                               case 0xE9: return SETu3r8(5, BC.bytes.Lo); break;
                               case 0xEA: return SETu3r8(5, DE.bytes.Hi); break;
                               case 0xEB: return SETu3r8(5, DE.bytes.Lo); break;
                               case 0xEC: return SETu3r8(5, HL.bytes.Hi); break;
                               case 0xED: return SETu3r8(5, HL.bytes.Lo); break;
                               case 0xEE: return SETu3a16(5, HL.HiLo); break;
                               case 0xEF: return SETu3r8(5, AF.bytes.Hi); break;
                               case 0xF0: return SETu3r8(6, BC.bytes.Hi); break;
                               case 0xF1: return SETu3r8(6, BC.bytes.Lo); break;
                               case 0xF2: return SETu3r8(6, DE.bytes.Hi); break;
                               case 0xF3: return SETu3r8(6, DE.bytes.Lo); break;
                               case 0xF4: return SETu3r8(6, HL.bytes.Hi); break;
                               case 0xF5: return SETu3r8(6, HL.bytes.Lo); break;
                               case 0xF6: return SETu3a16(6, HL.HiLo); break;
                               case 0xF7: return SETu3r8(6, AF.bytes.Hi); break;
                               case 0xF8: return SETu3r8(7, BC.bytes.Hi); break;
                               case 0xF9: return SETu3r8(7, BC.bytes.Lo); break;
                               case 0xFA: return SETu3r8(7, DE.bytes.Hi); break;
                               case 0xFB: return SETu3r8(7, DE.bytes.Lo); break;
                               case 0xFC: return SETu3r8(7, HL.bytes.Hi); break;
                               case 0xFD: return SETu3r8(7, HL.bytes.Lo); break;
                               case 0xFE: return SETu3a16(7, HL.HiLo); break;
                               case 0xFF: return SETu3r8(7, AF.bytes.Hi); break;
                               default: return InvalidOpcode(opcode); break;
                           }
                           break;
                       }
            case 0xCC: return CALLZn16();break;
            case 0xCD: return CALLn16();break;
            case 0xCE: return ADCr8n8(AF.bytes.Hi);break;
            case 0xCF: return RST(0x08);break;
            case 0xD0: return RETNC();break;
            case 0xD1: return POPr16(DE.HiLo);break;
            case 0xD2: return JPNCn16();break;
            case 0xD3: return InvalidOpcode(0xD3);break; //NOTHING
            case 0xD4: return CALLNCn16();break;
            case 0xD5: return PUSHr16(DE.HiLo);break;
            case 0xD6: return SUBr8n8(AF.bytes.Hi);break;
            case 0xD7: return RST(0x10);break;
            case 0xD8: return RETC();break;
            case 0xD9: return RETI();break; 
            case 0xDA: return JPCn16();break;
            case 0xDB: return InvalidOpcode(0xDB);break; //NOTHING
            case 0xDC: return CALLCn16();break;
            case 0xDD: return InvalidOpcode(0xDD);break; //NOTHING
            case 0xDE: return SBCr8n8(AF.bytes.Hi);break;
            case 0xDF: return RST(0x18);break; 
            case 0xE0: return LDHa8r8(fetch(), AF.bytes.Hi);break;
            case 0xE1: return POPr16(HL.HiLo);break; 
            case 0xE2: return LDHCr8(AF.bytes.Hi);break;
            case 0xE3: return InvalidOpcode(0xE3);break; //NOTHING
            case 0xE4: return InvalidOpcode(0xE4);break; //NOTHING
            case 0xE5: return PUSHr16(HL.HiLo);break;
            case 0xE6: return ANDr8n8(AF.bytes.Hi);break;
            case 0xE7: return RST(0x20);break;
            case 0xE8: return ADDr16e8(SP);break;
            case 0xE9: return JPHL();break;
            case 0xEA: return static_cast<uint8_t>(LDa16r8(n16Fetch(), AF.bytes.Hi) + 2);
            case 0xEB: return InvalidOpcode(0xEB);break; //NOTHING
            case 0xEC: return InvalidOpcode(0xEC);break; //NOTHING
            case 0xED: return InvalidOpcode(0xED);break; //NOTHING
            case 0xEE: return XORr8n8(AF.bytes.Hi);break;
            case 0xEF: return RST(0x28);break;
            case 0xF0: return LDHr8a8(AF.bytes.Hi, fetch());break;
            case 0xF1: return POPAF();break;
            case 0xF2: return LDHr8C(AF.bytes.Hi);break;
            case 0xF3: return DI();break;
            case 0xF4: return InvalidOpcode(0xF4);break; //NOTHING
            case 0xF5: return PUSHAF();break;
            case 0xF6: return ORr8n8(AF.bytes.Hi);break;
            case 0xF7: return RST(0x30);break;
            case 0xF8: return LDr16SPe8(HL.HiLo);break;
            case 0xF9: return LDr16r16(SP,HL.HiLo);break;
            case 0xFA: return static_cast<uint8_t>(LDr8a16(AF.bytes.Hi, n16Fetch()) + 2);
            case 0xFB: return EI();break;
            case 0xFC: return InvalidOpcode(0xFC);break; //NOTHING
            case 0xFD: return InvalidOpcode(0xFD);break; //NOTHING
            case 0xFE: return CPr8n8(AF.bytes.Hi);break;
            case 0xFF: return RST(0x38);break;
            default: return InvalidOpcode(opcode); break;
        }
    }();


    if(apply_ime_now)
    {
        IME = true;
        ime_scheduled = false;
    }
    return m_cycles;
}


// ----------------------------------------------------------------
// Flag accessors
// ----------------------------------------------------------------
void Cpu::SetZeroFlag()          {AF.bytes.Lo |= (1 << 7);}
void Cpu::ClearZeroFlag()        {AF.bytes.Lo &= ~(1 << 7);}
bool Cpu::GetZeroFlag()          {return (AF.bytes.Lo & 0b10000000) > 0;}

void Cpu::SetSubtractionFlag()   {AF.bytes.Lo |= (1 << 6);}
void Cpu::ClearSubtractionFlag() {AF.bytes.Lo &= ~(1 << 6);}
bool Cpu::GetSubtractionFlag()   {return (AF.bytes.Lo & 0b01000000) > 0;}

void Cpu::SetHalfCarryFlag()     {AF.bytes.Lo |= (1 << 5);}
void Cpu::ClearHalfCarryFlag()   {AF.bytes.Lo &= ~(1 << 5);}
bool Cpu::GetHalfCarryFlag()     {return (AF.bytes.Lo & 0b00100000) > 0;}

void Cpu::SetCarryFlag()         {AF.bytes.Lo |= (1 << 4);}
void Cpu::ClearCarryFlag()       {AF.bytes.Lo &= ~(1 << 4);}
bool Cpu::GetCarryFlag()         {return (AF.bytes.Lo & 0b00010000) > 0 ;}

// Small helper used by AND/OR/XOR/CP-family flag updates below, to avoid
// repeating "if(x) Set...(); else Clear...();" at every call site.
void Cpu::SetFlagIf(void (Cpu::*setFn)(), void (Cpu::*clearFn)(), bool cond)
{
    if (cond) (this->*setFn)();
    else      (this->*clearFn)();
}

uint8_t Cpu::fetch()
{
    uint8_t byte = gameboy.ReadFromMemory(PC);
    if (!halt_bug_active)
    {
        PC++;
    }
    else
    {
        halt_bug_active = false; // only suppress the increment once
    }
    return byte;
}

uint16_t Cpu::n16Fetch()
{
    uint16_t lo = fetch();
    uint16_t hi = fetch();
    return (hi << 8) | lo;
}

// ==================================================================
// 8/16-bit loads
// ==================================================================
uint8_t Cpu::NOP(){return 1;}

uint8_t Cpu::LDr8r8(uint8_t& dest, uint8_t src){dest = src; return 1;}
uint8_t Cpu::LDr8n8(uint8_t& dest, uint8_t val){dest = val; return 2;}

uint8_t Cpu::LDr16n16(uint16_t& dest)
{
    dest = n16Fetch();
    return 3;
}

uint8_t Cpu::LDr16r16(uint16_t& dest, uint16_t src){dest = src; return 2;}
uint8_t Cpu::LDa16r8(uint16_t addr, uint8_t reg) {gameboy.WriteToMemory(addr, reg); return 2;}
uint8_t Cpu::LDa16n8(uint16_t addr, uint8_t val) {gameboy.WriteToMemory(addr, val); return 3;}
uint8_t Cpu::LDr8a16(uint8_t& reg, uint16_t addr){reg = gameboy.ReadFromMemory(addr); return 2;}

uint8_t Cpu::LDIa16r8(uint16_t& addr, uint8_t reg)
{
    gameboy.WriteToMemory(addr, reg);
    addr++;
    return 2;
}

uint8_t Cpu::LDDa16r8(uint16_t& addr, uint8_t reg)
{
    gameboy.WriteToMemory(addr, reg);
    addr--;
    return 2;
}

uint8_t Cpu::LDIr8a16(uint8_t& reg,uint16_t& addr)
{
    reg = gameboy.ReadFromMemory(addr);
    addr++;
    return 2;
}

uint8_t Cpu::LDDr8a16(uint8_t& reg,uint16_t& addr)
{
    reg = gameboy.ReadFromMemory(addr);
    addr--;
    return 2;
}

uint8_t Cpu::LDa16SP(uint16_t reg)
{
    uint16_t addrLo  = fetch();
    uint16_t addr = (fetch() << 8) | addrLo;

    uint8_t loVal = reg & 0b0000000011111111;
    uint8_t hiVal = (reg >> 8) & 0xFF;
    gameboy.WriteToMemory(addr, loVal);
    gameboy.WriteToMemory(++addr, hiVal);
    return 5;
}

uint8_t Cpu::LDr16SPe8(uint16_t& dest)
{
    int8_t e8 = fetch();
    dest = SP + e8;
    int carry_out_of_bit3 = (((SP & 0xF)  +  (static_cast<uint8_t>(e8) & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((SP & 0xFF) + (static_cast<uint8_t>(e8) & 0xFF)) >> 8) & 1;
    ClearZeroFlag();
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3); //IF 3 overflowed then set H
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7); //IF 7 overflowed then set C
    return 3;
}

uint8_t Cpu::LDHa8r8(uint8_t lo, uint8_t val)
{
    uint16_t addr = static_cast<uint16_t>(0xFF00) | lo;
    gameboy.WriteToMemory(addr, val);
    return 3;
}

uint8_t Cpu::LDHCr8(uint8_t val)
{
    uint16_t addr = static_cast<uint16_t>(0xFF00) | BC.bytes.Lo;
    gameboy.WriteToMemory(addr, val);
    return 2;
}

uint8_t Cpu::LDHr8C(uint8_t& reg)
{
    uint16_t addr = static_cast<uint16_t>(0xFF00) | BC.bytes.Lo;
    reg = gameboy.ReadFromMemory(addr);
    return 2;
}

uint8_t Cpu::LDHr8a8(uint8_t& reg, uint8_t lo)
{
    uint16_t addr = static_cast<uint16_t>(0xFF00) | lo;
    reg = gameboy.ReadFromMemory(addr);
    return 3;
}

// ==================================================================
// 8-bit ALU: ADD / ADC / SUB / SBC / AND / OR / XOR / CP / INC / DEC
// ==================================================================
uint8_t Cpu::ADCr8r8(uint8_t& dest, uint8_t src)
{
    uint8_t carryIn = GetCarryFlag();
    uint8_t temp = dest + src + carryIn;
    int carry_out_of_bit3 = (((src & 0xF) + (carryIn & 0xF) +  (dest & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((src & 0xFF) + (carryIn & 0xFF) + (dest & 0xFF)) >> 8) & 1;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7);
    return 1;
}

uint8_t Cpu::ADCr8a16(uint8_t& dest, uint16_t addr)
{
    uint8_t src = gameboy.ReadFromMemory(addr);
    uint8_t carryIn = GetCarryFlag();
    uint8_t temp = dest + src + carryIn;
    int carry_out_of_bit3 = (((src & 0xF) + (carryIn & 0xF) +  (dest & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((src & 0xFF) + (carryIn & 0xFF) + (dest & 0xFF)) >> 8) & 1;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7);
    return 2;
}

uint8_t Cpu::ADCr8n8(uint8_t& dest)
{
    uint8_t src = fetch();
    uint8_t carryIn = GetCarryFlag();
    uint8_t temp = dest + src + carryIn;
    int carry_out_of_bit3 = (((src & 0xF) + (carryIn & 0xF) +  (dest & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((src & 0xFF) + (carryIn & 0xFF) + (dest & 0xFF)) >> 8) & 1;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7);
    return 2;
}

uint8_t Cpu::ADDr8r8(uint8_t& dest, uint8_t src)
{
    uint8_t temp = dest + src;
    int carry_out_of_bit3 = (((src & 0xF) + (dest & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((src & 0xFF) + (dest & 0xFF)) >> 8) & 1;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7);
    return 1;
}

uint8_t Cpu::ADDr8a16(uint8_t& dest, uint16_t addr)
{
    uint8_t src = gameboy.ReadFromMemory(addr);
    uint8_t temp = dest + src;
    int carry_out_of_bit3 = (((src & 0xF) +  (dest & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((src & 0xFF) + (dest & 0xFF)) >> 8) & 1;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7);
    return 2;
}

uint8_t Cpu::ADDr8n8(uint8_t& dest)
{
    uint8_t src = fetch();
    uint8_t temp = dest + src;
    int carry_out_of_bit3 = (((src & 0xF) +  (dest & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((src & 0xFF) + (dest & 0xFF)) >> 8) & 1;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7);
    return 2;
}

uint8_t Cpu::ADDr16r16(uint16_t& dest, uint16_t src)
{
    uint16_t temp = dest + src;
    uint16_t carry_out_of_bit11 = (((src & 0xFFF) + (dest & 0xFFF)) >> 12) & 1;
    uint32_t carry_out_of_bit15 = (((src & 0xFFFF) + (dest & 0xFFFF)) >> 16) & 1;
    dest = temp;
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit11);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit15);
    return 2;
}

uint8_t Cpu::ADDr16e8(uint16_t& reg)
{
    int8_t e8 = fetch();
    int carry_out_of_bit3 = (((reg & 0xF)  +  (static_cast<uint8_t>(e8) & 0xF)) >> 4) & 1;
    int carry_out_of_bit7 = (((reg & 0xFF) + (static_cast<uint8_t>(e8) & 0xFF)) >> 8) & 1;
    reg = reg + e8;
    ClearZeroFlag();
    ClearSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     carry_out_of_bit7);
    return 4;
}

uint8_t Cpu::SBCr8r8(uint8_t& dest, uint8_t src)
{
    uint8_t carry = GetCarryFlag();
    int full_result = static_cast<int>(dest) - static_cast<int>(src) - static_cast<int>(carry);
    uint8_t temp = static_cast<uint8_t>(full_result);

    int borrow_into_bit4 = ((dest & 0xF) < ((src & 0xF) + carry)) ? 1 : 0;
    int borrow = (full_result < 0) ? 1 : 0;

    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     borrow);
    return 1;
}

uint8_t Cpu::SBCr8a16(uint8_t& dest, uint16_t addr)
{
    uint8_t src = gameboy.ReadFromMemory(addr);
    uint8_t carry = GetCarryFlag();
    int full_result = static_cast<int>(dest) - static_cast<int>(src) - static_cast<int>(carry);
    uint8_t temp = static_cast<uint8_t>(full_result);

    int borrow_into_bit4 = ((dest & 0xF) < ((src & 0xF) + carry)) ? 1 : 0;
    int borrow = (full_result < 0) ? 1 : 0;

    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     borrow);
    return 2;
}

uint8_t Cpu::SBCr8n8(uint8_t& dest)
{
    uint8_t src = fetch();
    uint8_t carry = GetCarryFlag();
    int full_result = static_cast<int>(dest) - static_cast<int>(src) - static_cast<int>(carry);
    uint8_t temp = static_cast<uint8_t>(full_result);

    int borrow_into_bit4 = ((dest & 0xF) < ((src & 0xF) + carry)) ? 1 : 0;
    int borrow = (full_result < 0) ? 1 : 0;

    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     borrow);
    return 2;
}

uint8_t Cpu::SUBr8r8(uint8_t& dest, uint8_t src)
{
    uint8_t temp = dest - src;
    int borrow_into_bit4 = ((dest & 0xF) < (src & 0xF)) ? 1 : 0;
    int borrow = dest < src;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     borrow);
    return 1;
}

uint8_t Cpu::SUBr8a16(uint8_t& dest, uint16_t addr)
{
    uint8_t src = gameboy.ReadFromMemory(addr);
    uint8_t temp = dest - src;
    int borrow_into_bit4 = ((dest & 0xF) < (src & 0xF)) ? 1 : 0;
    int borrow = dest < src;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     borrow);
    return 2;
}

uint8_t Cpu::SUBr8n8(uint8_t& dest)
{
    uint8_t src = fetch();
    uint8_t temp = dest - src;
    int borrow_into_bit4 = ((dest & 0xF) < (src & 0xF)) ? 1 : 0;
    int borrow = dest < src;
    dest = temp;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     borrow);
    return 2;
}

// --- AND / OR / XOR ---
uint8_t Cpu::ANDr8r8(uint8_t& dest, uint8_t src)
{
    dest = dest & src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    SetHalfCarryFlag();
    ClearCarryFlag();
    return 1;
}

uint8_t Cpu::ANDr8a16(uint8_t& dest, uint16_t addr)
{
    uint8_t src = gameboy.ReadFromMemory(addr);
    dest = dest & src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    SetHalfCarryFlag();
    ClearCarryFlag();
    return 2;
}

uint8_t Cpu::ANDr8n8(uint8_t& dest)
{
    uint8_t src = fetch();
    dest = dest & src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    SetHalfCarryFlag();
    ClearCarryFlag();
    return 2;
}

uint8_t Cpu::XORr8r8(uint8_t& dest, uint8_t src)
{
    dest = dest ^ src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    return 1;
}

uint8_t Cpu::XORr8a16(uint8_t& dest, uint16_t addr)
{
    uint8_t src = gameboy.ReadFromMemory(addr);
    dest = dest ^ src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    return 2;
}

uint8_t Cpu::XORr8n8(uint8_t& dest)
{
    uint8_t src = fetch();
    dest = dest ^ src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    return 2;
}

uint8_t Cpu::ORr8r8(uint8_t& dest, uint8_t src)
{
    dest = dest | src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    return 1;
}

uint8_t Cpu::ORr8a16(uint8_t& dest, uint16_t addr)
{
    uint8_t src = gameboy.ReadFromMemory(addr);
    dest = dest | src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    return 2;
}

uint8_t Cpu::ORr8n8(uint8_t& dest)
{
    uint8_t src = fetch();
    dest = dest | src;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, dest == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    return 2;
}

uint8_t Cpu::CPr8r8(uint8_t a,uint8_t b)
{
    uint8_t result = a - b;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, result == 0);
    SetSubtractionFlag();
    int borrow_into_bit4 = ((a & 0xF) < (b & 0xF)) ? 1 : 0;
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     b > a);
    return 1;
}

uint8_t Cpu::CPr8a16(uint8_t a, uint16_t addr)
{
    uint8_t b = gameboy.ReadFromMemory(addr);
    uint8_t result = a - b;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, result == 0);
    SetSubtractionFlag();
    int borrow_into_bit4 = ((a & 0xF) < (b & 0xF)) ? 1 : 0;
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     b > a);
    return 2;
}

uint8_t Cpu::CPr8n8(uint8_t a)
{
    uint8_t b = fetch();
    uint8_t result = a - b;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, result == 0);
    SetSubtractionFlag();
    int borrow_into_bit4 = ((a & 0xF) < (b & 0xF)) ? 1 : 0;
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    SetFlagIf(&Cpu::SetCarryFlag,     &Cpu::ClearCarryFlag,     b > a);
    return 2;
}

uint8_t Cpu::INCr8(uint8_t& reg)
{
    uint8_t temp = reg + 1;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    int carry_out_of_bit3 = (((reg & 0xF)  +  1) >> 4) & 1;
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    reg = temp;
    return 1;
}

uint8_t Cpu::INCa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t temp = val + 1;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    ClearSubtractionFlag();
    int carry_out_of_bit3 = (((val & 0xF)  +  1) >> 4) & 1;
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, carry_out_of_bit3);
    gameboy.WriteToMemory(addr,temp);
    return 3;
}

uint8_t Cpu::INCr16(uint16_t& reg){reg = reg + 1; return 2;}

uint8_t Cpu::DECr8(uint8_t& reg)
{
    uint8_t temp = reg - 1;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    int borrow_into_bit4 = ((reg & 0xF) < (1 & 0xF)) ? 1 : 0;
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    reg = temp;
    return 1;
}

uint8_t Cpu::DECa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t temp = val - 1;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, temp == 0);
    SetSubtractionFlag();
    int borrow_into_bit4 = ((val & 0xF) < (1 & 0xF)) ? 1 : 0;
    SetFlagIf(&Cpu::SetHalfCarryFlag, &Cpu::ClearHalfCarryFlag, borrow_into_bit4);
    gameboy.WriteToMemory(addr,temp);
    return 3;
}

uint8_t Cpu::DECr16(uint16_t& reg) {reg = reg - 1; return 2;}

// --- Misc single-byte ALU/flag ops ---
uint8_t Cpu::CPL()
{
    AF.bytes.Hi = ~AF.bytes.Hi;
    SetSubtractionFlag();
    SetHalfCarryFlag();
    return 1;
}

uint8_t Cpu::DAA()
{
    uint8_t adjustment{};
    if(GetSubtractionFlag())
    {
        if(GetHalfCarryFlag()) {adjustment = adjustment + 0x6;}
        if(GetCarryFlag()) {adjustment = adjustment + 0x60;}
        AF.bytes.Hi = AF.bytes.Hi - adjustment;
    }
    else
    {
        if(GetHalfCarryFlag() || (AF.bytes.Hi & 0xF) > 0x9) {adjustment = adjustment + 0x6;}
        if(GetCarryFlag() || AF.bytes.Hi > 0x99) {adjustment = adjustment + 0x60; SetCarryFlag();}
        AF.bytes.Hi = AF.bytes.Hi + adjustment;
    }
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, AF.bytes.Hi == 0);
    ClearHalfCarryFlag();
    return 1;
}

uint8_t Cpu::SCF()
{
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetCarryFlag();
    return 1;
}

uint8_t Cpu::CCF()
{
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::ClearCarryFlag, &Cpu::SetCarryFlag, GetCarryFlag());
    return 1;
}

// ==================================================================
// Rotates / shifts / bit ops 
// ==================================================================
uint8_t Cpu::RLCA()
{
    uint8_t bit7 = (AF.bytes.Hi & 0b10000000) >> 7;
    AF.bytes.Hi = (AF.bytes.Hi << 1) | bit7;
    ClearZeroFlag();
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 1;
}

uint8_t Cpu::RLA()
{
    uint8_t c = GetCarryFlag();
    uint8_t bit7 = (AF.bytes.Hi & 0b10000000) >> 7;
    AF.bytes.Hi = (AF.bytes.Hi << 1) | c;
    ClearZeroFlag();
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 1;
}

uint8_t Cpu::RRA()
{
    uint8_t c = GetCarryFlag();
    uint8_t bit0 = (AF.bytes.Hi & 0b00000001);
    AF.bytes.Hi = (AF.bytes.Hi >> 1) | (c << 7);
    ClearZeroFlag();
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 1;
}

uint8_t Cpu::RRCA()
{
    uint8_t bit0 = (AF.bytes.Hi & 0b00000001);
    AF.bytes.Hi = (AF.bytes.Hi >> 1) | (bit0 << 7);
    ClearZeroFlag();
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 1;
}

uint8_t Cpu::RLCr8(uint8_t& reg)
{
    uint8_t bit7 = (reg & 0b10000000) >> 7;
    reg = (reg << 1) | bit7;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 2;
}

uint8_t Cpu::RLCa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t bit7 = (val & 0b10000000) >> 7;
    val = (val << 1) | bit7;
    gameboy.WriteToMemory(addr, val);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 4;
}

uint8_t Cpu::RRCr8(uint8_t& reg)
{
    uint8_t bit0 = (reg & 0b00000001) << 7;
    reg = (reg >> 1) | bit0;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 2;
}

uint8_t Cpu::RRCa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t bit0 = (val & 0b00000001) << 7;
    val = (val >> 1) | bit0;
    gameboy.WriteToMemory(addr, val);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 4;
}

uint8_t Cpu::RLr8(uint8_t& reg)
{
    uint8_t c = GetCarryFlag();
    uint8_t bit7 = (reg & 0b10000000) >> 7;
    reg = (reg << 1) | c;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 2;
}

uint8_t Cpu::RLa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t c = GetCarryFlag();
    uint8_t bit7 = (val & 0b10000000) >> 7;
    val = (val << 1) | c;
    gameboy.WriteToMemory(addr, val);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 4;
}

uint8_t Cpu::RRr8(uint8_t& reg)
{
    uint8_t c = GetCarryFlag() << 7;
    uint8_t bit0 = (reg & 0b00000001);
    reg = (reg >> 1) | c;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 2;
}

uint8_t Cpu::RRa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t c = GetCarryFlag() << 7;
    uint8_t bit0 = (val & 0b00000001);
    val = (val >> 1) | c;
    gameboy.WriteToMemory(addr, val);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 4;
}

uint8_t Cpu::SLAr8(uint8_t& reg)
{
    int bit7 = (reg >> 7) & 1;
    reg = reg << 1;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 2;
}

uint8_t Cpu::SLAa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    int bit7 = (val >> 7) & 1;
    val = val << 1;
    gameboy.WriteToMemory(addr, val);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit7);
    return 4;
}

uint8_t Cpu::SRAr8(uint8_t& reg)
{
    int bit0 = reg & 1;
    int bit7 = (reg >> 7) & 1;
    reg = reg >> 1;
    if(bit7) { reg |= 0b10000000; }
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 2;
}

uint8_t Cpu::SRAa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    int bit0 = val & 1;
    int bit7 = (val >> 7) & 1;
    val = val >> 1;
    if(bit7) { val |= 0b10000000; }
    gameboy.WriteToMemory(addr, val);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 4;
}

uint8_t Cpu::SRLr8(uint8_t& reg)
{
    int bit0 = reg & 1;
    reg = reg >> 1;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 2;
}

uint8_t Cpu::SRLa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    int bit0 = val & 1;
    val = val >> 1;
    gameboy.WriteToMemory(addr, val);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    SetFlagIf(&Cpu::SetCarryFlag, &Cpu::ClearCarryFlag, bit0);
    return 4;
}

uint8_t Cpu::SWAPr8(uint8_t& reg)
{
    uint8_t upper4bits = reg >> 4;
    reg = (reg << 4) | upper4bits;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, reg == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    return 2;
}

uint8_t Cpu::SWAPa16(uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t upper4bits = val >> 4;
    val = (val << 4) | upper4bits;
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, val == 0);
    ClearSubtractionFlag();
    ClearHalfCarryFlag();
    ClearCarryFlag();
    gameboy.WriteToMemory(addr, val);
    return 4;
}

uint8_t Cpu::BITu3r8(uint8_t bitPos, uint8_t reg)
{
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, ((reg >> bitPos) & 1) == 0);
    ClearSubtractionFlag();
    SetHalfCarryFlag();
    return 2;
}

uint8_t Cpu::BITu3a16(uint8_t bitPos, uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    SetFlagIf(&Cpu::SetZeroFlag, &Cpu::ClearZeroFlag, ((val >> bitPos) & 1) == 0);
    ClearSubtractionFlag();
    SetHalfCarryFlag();
    return 3;
}

uint8_t Cpu::SETu3r8(uint8_t bitPos, uint8_t& reg){reg = reg | (1 << bitPos); return 2;}

uint8_t Cpu::SETu3a16(uint8_t bitPos, uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    uint8_t result = val | (1 << bitPos);
    gameboy.WriteToMemory(addr,result);
    return 4;
}

uint8_t Cpu::RESu3r8(uint8_t bitPos, uint8_t& reg)
{
    reg &= ~(1 << bitPos);
    return 2;
}

uint8_t Cpu::RESu3a16(uint8_t bitPos, uint16_t addr)
{
    uint8_t val = gameboy.ReadFromMemory(addr);
    val &= ~(1 << bitPos);
    gameboy.WriteToMemory(addr, val);
    return 4;
}

// ==================================================================
// Control flow: JP / JR / CALL / RET / RST
// ==================================================================
uint8_t Cpu::JPn16()
{
    uint16_t addr = n16Fetch();
    PC = addr;
    return 4;
}

uint8_t Cpu::JPHL(){PC = HL.HiLo; return 1;}

uint8_t Cpu::JPNZn16()
{
    uint16_t addr = n16Fetch();
    if(!GetZeroFlag()) { PC = addr; return 4; }
    return 3;
}

uint8_t Cpu::JPNCn16()
{
    uint16_t addr = n16Fetch();
    if(!GetCarryFlag()) { PC = addr; return 4; }
    return 3;
}

uint8_t Cpu::JPZn16()
{
    uint16_t addr = n16Fetch();
    if(GetZeroFlag()) { PC = addr; return 4; }
    return 3;
}

uint8_t Cpu::JPCn16()
{
    uint16_t addr = n16Fetch();
    if(GetCarryFlag()) { PC = addr; return 4; }
    return 3;
}

uint8_t Cpu::JRe8()
{
    int8_t e8 = fetch();
    PC = PC + e8;
    return 3;
}

uint8_t Cpu::JRNZe8()
{
    int8_t e8 = fetch();
    if(!GetZeroFlag()) { PC = PC + e8; return 3; }
    return 2;
}

uint8_t Cpu::JRNCe8()
{
    int8_t e8 = fetch();
    if(!GetCarryFlag()) { PC = PC + e8; return 3; }
    return 2;
}

uint8_t Cpu::JRZe8()
{
    int8_t e8 = fetch();
    if(GetZeroFlag()) { PC = PC + e8; return 3; }
    return 2;
}

uint8_t Cpu::JRCe8()
{
    int8_t e8 = fetch();
    if(GetCarryFlag()) { PC = PC + e8; return 3; }
    return 2;
}

void Cpu::DispatchInterrupt(uint16_t addr)
{
    SP--;
    gameboy.WriteToMemory(SP, (PC >> 8) & 0xFF); //high byte of PC
    SP--;
    gameboy.WriteToMemory(SP, PC & 0xFF); //low byte of PC
    PC = addr;
}

uint8_t Cpu::CALLn16()
{
    uint16_t target = n16Fetch();
    SP--;
    gameboy.WriteToMemory(SP, (PC >> 8) & 0xFF);
    SP--;
    gameboy.WriteToMemory(SP, PC & 0xFF);
    PC = target;
    return 6;
}

uint8_t Cpu::CALLNZn16()
{
    uint16_t target = n16Fetch();
    if (!GetZeroFlag())
    {
        SP--; gameboy.WriteToMemory(SP, (PC >> 8) & 0xFF);
        SP--; gameboy.WriteToMemory(SP, PC & 0xFF);
        PC = target;
        return 6;
    }
    return 3;
}

uint8_t Cpu::CALLNCn16()
{
    uint16_t target = n16Fetch();
    if (!GetCarryFlag())
    {
        SP--; gameboy.WriteToMemory(SP, (PC >> 8) & 0xFF);
        SP--; gameboy.WriteToMemory(SP, PC & 0xFF);
        PC = target;
        return 6;
    }
    return 3;
}

uint8_t Cpu::CALLZn16()
{
    uint16_t target = n16Fetch();
    if (GetZeroFlag())
    {
        SP--; gameboy.WriteToMemory(SP, (PC >> 8) & 0xFF);
        SP--; gameboy.WriteToMemory(SP, PC & 0xFF);
        PC = target;
        return 6;
    }
    return 3;
}

uint8_t Cpu::CALLCn16()
{
    uint16_t target = n16Fetch();
    if (GetCarryFlag())
    {
        SP--; gameboy.WriteToMemory(SP, (PC >> 8) & 0xFF);
        SP--; gameboy.WriteToMemory(SP, PC & 0xFF);
        PC = target;
        return 6;
    }
    return 3;
}

uint8_t Cpu::RET()
{
    uint8_t lo = gameboy.ReadFromMemory(SP);
    SP++;
    uint8_t hi = gameboy.ReadFromMemory(SP);
    SP++;
    PC = (static_cast<uint16_t>(hi) << 8) | lo;
    return 4;
}

uint8_t Cpu::RETI()
{
    RET();
    IME = true;
    ime_scheduled = false;
    return 4;
}

uint8_t Cpu::RETZ()
{
    if(GetZeroFlag()) { return RET() + 1; } // 4 (RET) + 1 = 5
    return 2;
}

uint8_t Cpu::RETC()
{
    if(GetCarryFlag()) { return RET() + 1; }
    return 2;
}

uint8_t Cpu::RETNZ()
{
    if(!GetZeroFlag()) { return RET() + 1; }
    return 2;
}

uint8_t Cpu::RETNC()
{
    if(!GetCarryFlag()) { return RET() + 1; }
    return 2;
}

uint8_t Cpu::RST(uint16_t addr)
{
    SP--;
    gameboy.WriteToMemory(SP, (PC >> 8) & 0xFF);
    SP--;
    gameboy.WriteToMemory(SP, PC & 0xFF);
    PC = addr;
    return 4;
}

// ==================================================================
// Stack: PUSH / POP
// ==================================================================
uint8_t Cpu::POPr16(uint16_t& reg)
{
    uint8_t lo = gameboy.ReadFromMemory(SP);
    SP++;
    uint8_t hi = gameboy.ReadFromMemory(SP);
    SP++;
    reg = (static_cast<uint16_t>(hi) << 8) | lo;
    return 3;
}

uint8_t Cpu::PUSHr16(uint16_t reg)
{
    SP--;
    uint8_t lo = reg & 0x00FF;
    uint8_t hi = (reg >> 8);
    gameboy.WriteToMemory(SP, hi);
    SP--;
    gameboy.WriteToMemory(SP, lo);
    return 4;
}

uint8_t Cpu::POPAF()
{
    uint8_t lo = gameboy.ReadFromMemory(SP);
    SP++;
    uint8_t hi = gameboy.ReadFromMemory(SP);
    SP++;
    lo &= 0xF0; // low nibble of F is always 0
    AF.HiLo = (static_cast<uint16_t>(hi) << 8) | lo;
    return 3;
}

uint8_t Cpu::PUSHAF(){return PUSHr16(AF.HiLo);} // Here for symmetry with POPAF but does the same as PUSHr16().

// ==================================================================
// Misc control: DI / EI / HALT / STOP / invalid opcode
// ==================================================================
uint8_t Cpu::DI()
{
    IME = false;
    ime_scheduled = false;
    return 1;
}

uint8_t Cpu::EI()
{
    ime_scheduled = true;
    return 1;
}

uint8_t Cpu::HALT()
{
    bool interrupt_pending = (gameboy.ReadFromMemory(IF_addr) & gameboy.ReadFromMemory(IE_addr)) != 0;

    if (!IME && interrupt_pending)
    {
        halt_bug_active = true;
    }
    else
    {
        halted = true;
    }
    return 1;
}

uint8_t Cpu::STOP()
{
    stopped = true;
    // STOP is followed by a padding byte
    fetch();
    return 1;
}

uint8_t Cpu::InvalidOpcode(uint8_t opcode)
{
    std::cerr << "Invalid opcode encountered: 0x" << std::hex << (int)opcode << "\n";
    halted = true;
    return 1;
}

//used for DEBUG only
uint16_t Cpu::GetPC() {return PC;}
uint16_t Cpu::GetSP() {return SP;}
uint16_t Cpu::GetAF() {return AF.HiLo;}
uint16_t Cpu::GetBC() {return BC.HiLo;}
uint16_t Cpu::GetDE() {return DE.HiLo;}
uint16_t Cpu::GetHL() {return HL.HiLo;}
bool Cpu::GetIME() {return IME;}
