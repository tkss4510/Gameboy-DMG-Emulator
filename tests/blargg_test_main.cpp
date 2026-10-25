#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <cstddef>

#include "gameboy.hpp"

static constexpr long MAX_STEPS = 200'000'000;
static constexpr long STUCK_CHECK_WINDOW = 2'000'000;
static constexpr size_t STUCK_MIN_UNIQUE_PCS = 4;
static constexpr uint16_t SB_ADDR = 0xFF01;
static constexpr uint16_t SC_ADDR = 0xFF02;

struct TraceEntry
{
    uint16_t pc;
    uint8_t op;
};

static int RunTest(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: gb_tests <path_to_rom>\n";
        return 1;
    }

    const std::string romPath = argv[1];
    std::ifstream file(romPath, std::ios::binary);
    if (!file)
    {
        std::cerr << "ERROR: could not open ROM file: " << romPath << "\n";
        return 1;
    }

    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    if (rom.empty())
    {
        std::cerr << "ERROR: ROM file read as empty: " << romPath << "\n";
        return 1;
    }

    std::cout << "Loaded ROM: " << romPath << " (" << rom.size() << " bytes)\n";

    // GameBoy builds and connects every component, loads the ROM (throws on an
    // invalid one) and applies the post-boot register state (no boot ROM is run).
    GameBoy gb(rom, romPath.c_str());
    Cpu& cpu = gb.GetCpu();

    // The Cpu constructor prints PC in hex and leaves cout in hex mode.
    std::cout << std::dec << "\n";
    if (cpu.GetPC() != 0x100)
    {
        std::cout << "WARNING: PC did not initialize to 0x100.\n";
    }

    std::string serialOutput;
    bool done = false;
    bool stuck = false;
    std::vector<uint16_t> windowPCs;
    windowPCs.reserve(STUCK_CHECK_WINDOW);
    size_t lastWindowSerialLen = 0;

    // Rolling log of the last 64 distinct PCs (recorded only when PC changes,
    // so a CPU parked on one address doesn't flood it).
    std::array<TraceEntry, 64> trace{};
    size_t traceIdx = 0;
    int lastPc = -1;

    long step = 0;
    for (; step < MAX_STEPS && !done; step++)
    {
        windowPCs.push_back(cpu.GetPC());

        uint16_t tracePc = cpu.GetPC();
        if (tracePc != lastPc)
        {
            trace[traceIdx++ % trace.size()] = { tracePc, gb.ReadFromMemory(tracePc) };
            lastPc = tracePc;
        }

        int mCycles = cpu.Step();
        gb.TimerAdvance(mCycles * 4);
        gb.SyncTimerInterruptIntoIF();
        gb.PpuStep(mCycles * 4);
        gb.PpuClearFrameReady();

        if (gb.ReadFromMemory(SC_ADDR) == 0x81)
        {
            char c = static_cast<char>(gb.ReadFromMemory(SB_ADDR));
            serialOutput += c;
            gb.WriteToMemory(SC_ADDR, 0x00);
            std::cout << c;
            std::cout.flush();

            if (serialOutput.find("Passed") != std::string::npos ||
                serialOutput.find("Failed") != std::string::npos)
            {
                done = true;
            }
        }

        if (step > 0 && step % STUCK_CHECK_WINDOW == 0)
        {
            std::sort(windowPCs.begin(), windowPCs.end());
            windowPCs.erase(std::unique(windowPCs.begin(), windowPCs.end()), windowPCs.end());

            bool noNewSerial = serialOutput.size() == lastWindowSerialLen;
            if (windowPCs.size() < STUCK_MIN_UNIQUE_PCS && noNewSerial)
            {
                std::cout << "\n*** STUCK at step " << step << ", PC=0x" << std::hex << cpu.GetPC()
                          << " IME=" << cpu.GetIME() << std::dec << " ***\n";

                auto rd = [&](uint16_t a) { return static_cast<int>(gb.ReadFromMemory(a)); };
                uint16_t pc = cpu.GetPC();
                std::cout << std::hex
                          << "bytes PC-3..PC+2: "
                          << rd(pc - 3) << ' ' << rd(pc - 2) << ' ' << rd(pc - 1) << ' '
                          << rd(pc) << ' ' << rd(pc + 1) << ' ' << rd(pc + 2) << "\n"
                          << "IF=" << rd(0xFF0F) << " IE=" << rd(0xFFFF)
                          << " LCDC=" << rd(0xFF40) << " STAT=" << rd(0xFF41) << " LY=" << rd(0xFF44)
                          << " TAC=" << rd(0xFF07) << " TIMA=" << rd(0xFF05) << " SC=" << rd(0xFF02) << "\n"
                          << "AF=" << cpu.GetAF() << " BC=" << cpu.GetBC() << " DE=" << cpu.GetDE()
                          << " HL=" << cpu.GetHL() << " SP=" << cpu.GetSP() << "\n";

                std::cout << "trace (oldest first, pc:opcode):\n";
                for (size_t i = 0; i < trace.size(); i++)
                {
                    const auto& t = trace[(traceIdx + i) % trace.size()];
                    std::cout << t.pc << ":" << static_cast<int>(t.op) << " ";
                }
                std::cout << std::dec << "\n";

                stuck = true;
                done = true;
            }

            windowPCs.clear();
            lastWindowSerialLen = serialOutput.size();
        }
    }

    std::cout << "\n\n=== Run Summary ===\n";
    std::cout << "Steps: " << step << "\n";
    std::cout << "Final PC: 0x" << std::hex << cpu.GetPC() << std::dec << "\n";

    if (serialOutput.find("Passed") != std::string::npos)
    {
        std::cout << "[TEST PASSED]\n";
        return 0;
    }
    if (stuck || step >= MAX_STEPS)
    {
        std::cout << "[TEST INCONCLUSIVE] stuck or timed out\n";
        return 2;
    }
    std::cout << "[TEST FAILED]\n";
    return 1;
}

int main(int argc, char** argv)
{
    // The GameBoy constructor throws std::runtime_error if the ROM is invalid.
    try
    {
        return RunTest(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
