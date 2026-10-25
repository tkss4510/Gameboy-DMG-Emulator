#ifndef LCD_HPP
#define LCD_HPP

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_render.h>
#include <array>
#include <cstdint>

constexpr int WIDTH{160};
constexpr int HEIGHT{144};

class LCD {
public:
    LCD(const char* title);
    LCD(LCD &&) = delete;
    LCD(const LCD &) = delete;
    LCD &operator=(LCD &&) = delete;
    LCD &operator=(const LCD &) = delete;
    ~LCD();

    void Step();
    void SetFrameBuffer(const std::array<std::array<uint8_t,144> ,160>& frame);
    bool ShouldQuit() const { return quitRequested; }
    uint8_t PollEvents();
    void Throttle(); //limit fps
private:
    bool quitRequested = false;
    SDL_Window* window{}; 
    SDL_Renderer* renderer{};
    const uint32_t shades[4] = 
    {
        0xFFFFFFFF,  // 0 white
        0xFFAAAAAA,  // 1 light gray
        0xFF555555,  // 2 dark gray
        0xFF000000   // 3 black
    }; 
    std::array<uint32_t, WIDTH * HEIGHT> frameBuffer{}; //SDL needs flat arrays
    SDL_Texture* texture{}; //what the framebuffer gets passed into
    const bool* keys{}; //Used for keyboard inputs for joypad
    uint64_t nextFrameNs{}; //fps throttling
};

#endif // !LCD_HPP
