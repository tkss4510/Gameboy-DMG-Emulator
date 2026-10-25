#include "lcd.hpp"
#include <array>
#include <cstdint>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>

constexpr int SCALE = 4;
constexpr SDL_Scancode DPAD_UP    = SDL_SCANCODE_W;
constexpr SDL_Scancode DPAD_DOWN  = SDL_SCANCODE_S;
constexpr SDL_Scancode DPAD_LEFT  = SDL_SCANCODE_A;
constexpr SDL_Scancode DPAD_RIGHT  = SDL_SCANCODE_D;

constexpr SDL_Scancode BUTTON_SELECT  = SDL_SCANCODE_SPACE;
constexpr SDL_Scancode BUTTON_START  = SDL_SCANCODE_RETURN;
constexpr SDL_Scancode BUTTON_A  = SDL_SCANCODE_J;
constexpr SDL_Scancode BUTTON_B  = SDL_SCANCODE_L;


LCD::LCD(const char* title)
{  
    if (!SDL_Init(SDL_INIT_VIDEO)) {SDL_Log("SDL_Init failed: %s", SDL_GetError()); }
    window = SDL_CreateWindow(title, WIDTH * SCALE, HEIGHT * SCALE, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window,nullptr);
    if (!window || !renderer) 
    {
        SDL_Log("Setup failed: %s", SDL_GetError());
        quitRequested = true;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
    keys = SDL_GetKeyboardState(nullptr); //Since we use sdl here we will also use the lcd for the joypad inputs
}

LCD::~LCD()
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

uint8_t LCD::PollEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_EVENT_QUIT) quitRequested = true;

    uint8_t state = 0;   // 1 = pressed unlike joypad

    if (keys[DPAD_RIGHT])    state |= 1 << 0;
    if (keys[DPAD_LEFT])     state |= 1 << 1;
    if (keys[DPAD_UP])       state |= 1 << 2;
    if (keys[DPAD_DOWN])     state |= 1 << 3;
    if (keys[BUTTON_A])      state |= 1 << 4;
    if (keys[BUTTON_B])      state |= 1 << 5;
    if (keys[BUTTON_SELECT]) state |= 1 << 6;
    if (keys[BUTTON_START])  state |= 1 << 7;

    if ((state & 0x03) == 0x03) state &= ~0x03;  
    if ((state & 0x0C) == 0x0C) state &= ~0x0C;   
    return state;
}

void LCD::Step()
{
    SDL_UpdateTexture(texture, nullptr, frameBuffer.data(), WIDTH * sizeof(uint32_t)); // copy array into texture
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);// draw texture in window
    SDL_RenderPresent(renderer);// show it
}

void LCD::SetFrameBuffer(const std::array<std::array<uint8_t,144> ,160>& frame) 
{
    for(uint8_t y = 0; y < HEIGHT;y++)
    {
        for(uint8_t x = 0; x < WIDTH;x++)
        {
            frameBuffer[y * WIDTH + x] = shades[frame[x][y] & 3]; //Making sure every value is within 0-3
        }
    }
}

void LCD::Throttle()
{
    constexpr uint64_t FRAME_NS = 16'742'706;   // 70224 cycles at 4.194304 MHz
    uint64_t now = SDL_GetTicksNS();
    if (nextFrameNs == 0) nextFrameNs = now;
    nextFrameNs += FRAME_NS;
    if (nextFrameNs > now) SDL_DelayPrecise(nextFrameNs - now);
    else nextFrameNs = now;                     // fell behind, resync
}
