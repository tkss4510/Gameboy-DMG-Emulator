#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>
#include <exception>
#include "gameboy.hpp"

int main(int argc, char* argv[])
{
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) { std::cerr << "Could not open ROM\n"; return 1; }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    try
    {
        GameBoy gameboy{rom, "Gameboy"};
        gameboy.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
