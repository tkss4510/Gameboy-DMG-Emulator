#ifndef CPU_HPP
#define CPU_HPP

#include <cstdint>
#include <stdint.h>
#include <sys/stat.h>

//Used to glue memory to cpu without direct include
class GameBoy;

union Register16
{
    uint16_t HiLo{};
    struct { uint8_t Lo{}; uint8_t Hi{}; } bytes;
};

class Cpu
{
private:
    GameBoy& gameboy;

    // ----------------------------------------------------------------
    // Registers
    // ----------------------------------------------------------------
    Register16 AF;  //Accumulator & Flags
                    //Flags Register(lower 8 bits of AF):
                    //bit 7: Zero flag
                    //bit 6: Subtraction flag(BCD)
                    //bit 5: Half Carry flag(BCD)
                    //bit 4: Carry flag
    Register16 BC;
    Register16 DE;
    Register16 HL;
    uint16_t   SP;  //Stack Pointer
    uint16_t   PC;  //Program Counter

    // ----------------------------------------------------------------
    // CPU / interrupt state
    // ----------------------------------------------------------------
    bool IME{}; //Interrupt Master Enable --> All interrupts are enabled ONLY when set
    bool halted{}; // Used to check if cpu is halted/ ran into invalid opcodes
    bool stopped{};
    bool ime_scheduled{};
    bool halt_bug_active{};

    const uint16_t IE_addr = 0xFFFF; //Interrupt enable
    const uint16_t IF_addr = 0xFF0F; //Interrupt flag
    //bit 0: VBlank | bit 1: LCD | bit 2: Timer | bit 3: Serial | bit 4: joypad
    //Any set bits in the IF register are only requesting an interrupt.
    //Execution of the interrupt handler happens only if both the IME flag and the corresponding bit in the IE register are set.
    const uint16_t VBlank_handler = 0x40;
    const uint16_t STAT_handler   = 0x48;
    const uint16_t Timer_handler  = 0x50;
    const uint16_t Serial_handler = 0x58;

    // ----------------------------------------------------------------
    // Flag accessors
    // ----------------------------------------------------------------
    void SetZeroFlag();
    void ClearZeroFlag();
    bool GetZeroFlag();

    void SetSubtractionFlag();
    void ClearSubtractionFlag();
    bool GetSubtractionFlag();

    void SetHalfCarryFlag();
    void ClearHalfCarryFlag();
    bool GetHalfCarryFlag();

    void SetCarryFlag();
    void ClearCarryFlag();
    bool GetCarryFlag();

    // Small helper used by AND/OR/XOR/CP-family flag updates below, to avoid
    // repeating "if(x) Set...(); else Clear...();" at every call site.
    void SetFlagIf(void (Cpu::*setFn)(), void (Cpu::*clearFn)(), bool cond);

    // ----------------------------------------------------------------
    // Fetch helpers
    // ----------------------------------------------------------------
    uint8_t fetch();

    uint16_t n16Fetch();

    // ==================================================================
    // 8/16-bit loads
    // ==================================================================
    uint8_t NOP();

    uint8_t LDr8r8(uint8_t& dest, uint8_t src);
    uint8_t LDr8n8(uint8_t& dest, uint8_t val);
    uint8_t LDr16n16(uint16_t& dest);
    uint8_t LDr16r16(uint16_t& dest, uint16_t src);
    uint8_t LDa16r8(uint16_t addr, uint8_t reg);
    uint8_t LDa16n8(uint16_t addr, uint8_t val);
    uint8_t LDr8a16(uint8_t& reg, uint16_t addr);

    uint8_t LDIa16r8(uint16_t& addr, uint8_t reg);
    uint8_t LDDa16r8(uint16_t& addr, uint8_t reg);
    uint8_t LDIr8a16(uint8_t& reg,uint16_t& addr);
    uint8_t LDDr8a16(uint8_t& reg,uint16_t& addr);
    uint8_t LDa16SP(uint16_t reg);
    uint8_t LDr16SPe8(uint16_t& dest);

    // High-page loads (LDH)
    uint8_t LDHa8r8(uint8_t lo, uint8_t val);
    uint8_t LDHCr8(uint8_t val);
    uint8_t LDHr8C(uint8_t& reg);
    uint8_t LDHr8a8(uint8_t& reg, uint8_t lo);

    // ==================================================================
    // 8-bit ALU: ADD / ADC / SUB / SBC / AND / OR / XOR / CP / INC / DEC
    // ==================================================================
    uint8_t ADCr8r8(uint8_t& dest, uint8_t src);
    uint8_t ADCr8a16(uint8_t& dest, uint16_t addr);
    uint8_t ADCr8n8(uint8_t& dest);

    uint8_t ADDr8r8(uint8_t& dest, uint8_t src);
    uint8_t ADDr8a16(uint8_t& dest, uint16_t addr);
    uint8_t ADDr8n8(uint8_t& dest);
    uint8_t ADDr16r16(uint16_t& dest, uint16_t src);
    uint8_t ADDr16e8(uint16_t& reg);

    uint8_t SBCr8r8(uint8_t& dest, uint8_t src);
    uint8_t SBCr8a16(uint8_t& dest, uint16_t addr);
    uint8_t SBCr8n8(uint8_t& dest);

    uint8_t SUBr8r8(uint8_t& dest, uint8_t src);
    uint8_t SUBr8a16(uint8_t& dest, uint16_t addr);
    uint8_t SUBr8n8(uint8_t& dest);

    // --- AND / OR / XOR ---
    uint8_t ANDr8r8(uint8_t& dest, uint8_t src);
    uint8_t ANDr8a16(uint8_t& dest, uint16_t addr);
    uint8_t ANDr8n8(uint8_t& dest);

    uint8_t XORr8r8(uint8_t& dest, uint8_t src);
    uint8_t XORr8a16(uint8_t& dest, uint16_t addr);
    uint8_t XORr8n8(uint8_t& dest);

    uint8_t ORr8r8(uint8_t& dest, uint8_t src);
    uint8_t ORr8a16(uint8_t& dest, uint16_t addr);
    uint8_t ORr8n8(uint8_t& dest);

    uint8_t CPr8r8(uint8_t a,uint8_t b);
    uint8_t CPr8a16(uint8_t a, uint16_t addr);
    uint8_t CPr8n8(uint8_t a);

    uint8_t INCr8(uint8_t& reg);
    uint8_t INCa16(uint16_t addr);
    uint8_t INCr16(uint16_t& reg);

    uint8_t DECr8(uint8_t& reg);
    uint8_t DECa16(uint16_t addr);
    uint8_t DECr16(uint16_t& reg);

    // --- Misc single-byte ALU/flag ops ---
    uint8_t CPL();
    uint8_t DAA();
    uint8_t SCF();
    uint8_t CCF();

    // ==================================================================
    // Rotates / shifts / bit ops (CB-prefixed family + accumulator forms)
    // ==================================================================
    uint8_t RLCA();
    uint8_t RLA();
    uint8_t RRA();
    uint8_t RRCA();

    uint8_t RLCr8(uint8_t& reg);
    uint8_t RLCa16(uint16_t addr);
    uint8_t RRCr8(uint8_t& reg);
    uint8_t RRCa16(uint16_t addr);
    uint8_t RLr8(uint8_t& reg);
    uint8_t RLa16(uint16_t addr);
    uint8_t RRr8(uint8_t& reg);
    uint8_t RRa16(uint16_t addr);

    uint8_t SLAr8(uint8_t& reg);
    uint8_t SLAa16(uint16_t addr);
    uint8_t SRAr8(uint8_t& reg);
    uint8_t SRAa16(uint16_t addr);
    uint8_t SRLr8(uint8_t& reg);
    uint8_t SRLa16(uint16_t addr);
    uint8_t SWAPr8(uint8_t& reg);
    uint8_t SWAPa16(uint16_t addr);

    uint8_t BITu3r8(uint8_t bitPos, uint8_t reg);
    uint8_t BITu3a16(uint8_t bitPos, uint16_t addr);
    uint8_t SETu3r8(uint8_t bitPos, uint8_t& reg);
    uint8_t SETu3a16(uint8_t bitPos, uint16_t addr);
    uint8_t RESu3r8(uint8_t bitPos, uint8_t& reg);
    uint8_t RESu3a16(uint8_t bitPos, uint16_t addr);

    // ==================================================================
    // Control flow: JP / JR / CALL / RET / RST
    // ==================================================================
    uint8_t JPn16();
    uint8_t JPHL();
    uint8_t JPNZn16();
    uint8_t JPNCn16();
    uint8_t JPZn16();
    uint8_t JPCn16();

    uint8_t JRe8();
    uint8_t JRNZe8();
    uint8_t JRNCe8();
    uint8_t JRZe8();
    uint8_t JRCe8();

    void DispatchInterrupt(uint16_t addr);

    uint8_t CALLn16();
    uint8_t CALLNZn16();
    uint8_t CALLNCn16();
    uint8_t CALLZn16();
    uint8_t CALLCn16();

    uint8_t RET();
    uint8_t RETI();
    uint8_t RETZ();
    uint8_t RETC();
    uint8_t RETNZ();
    uint8_t RETNC();

    uint8_t RST(uint16_t addr);

    // ==================================================================
    // Stack: PUSH / POP
    // ==================================================================
    uint8_t POPr16(uint16_t& reg);
    uint8_t PUSHr16(uint16_t reg);
    uint8_t POPAF();
    uint8_t PUSHAF();

    // ==================================================================
    // Misc control: DI / EI / HALT / STOP / invalid opcode
    // ==================================================================
    uint8_t DI();
    uint8_t EI();
    uint8_t HALT();
    uint8_t STOP();
    uint8_t InvalidOpcode(uint8_t opcode);

    void Init(bool isChecksumHeaderNonZero);

public:
    Cpu(GameBoy& gb);
    uint8_t Step();

    //used for DEBUG only
    uint16_t GetPC();
    uint16_t GetSP();
    uint16_t GetAF();
    uint16_t GetBC();
    uint16_t GetDE();
    uint16_t GetHL();
    bool GetIME();
};

#endif
