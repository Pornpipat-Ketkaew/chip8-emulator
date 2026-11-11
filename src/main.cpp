#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <array>
#include <cstdint>
#include <iostream>

#include "chip8.h"

namespace {

constexpr int kScale = 10;               // each CHIP-8 pixel becomes a 10x10 square
constexpr int kInstructionsPerFrame = 10; // 10 x 60 frames = 600 instructions per second
constexpr Uint32 kFrameMs = 1000 / 60;    // one frame = about 16 ms (60 frames per second)

constexpr std::uint32_t kColorOn = 0xFFE0E0E0;   // light grey (format: 0xAARRGGBB)
constexpr std::uint32_t kColorOff = 0xFF141428;  // dark blue
// The CHIP-8 keypad is a 4x4 grid. We map it to the left side of a PC keyboard:
//   CHIP-8 keypad      PC keyboard
//   1 2 3 C            1 2 3 4
//   4 5 6 D            Q W E R
//   7 8 9 E            A S D F
//   A 0 B F            Z X C V
// We use "scancodes" (physical key positions), so it also works with a Thai keyboard layout.
int mapKey(SDL_Scancode code) {
    switch (code) {
        case SDL_SCANCODE_1: return 0x1;
        case SDL_SCANCODE_2: return 0x2;
        case SDL_SCANCODE_3: return 0x3;
        case SDL_SCANCODE_4: return 0xC;
        case SDL_SCANCODE_Q: return 0x4;
        case SDL_SCANCODE_W: return 0x5;
        case SDL_SCANCODE_E: return 0x6;
        case SDL_SCANCODE_R: return 0xD;
        case SDL_SCANCODE_A: return 0x7;
        case SDL_SCANCODE_S: return 0x8;
        case SDL_SCANCODE_D: return 0x9;
        case SDL_SCANCODE_F: return 0xE;
        case SDL_SCANCODE_Z: return 0xA;
        case SDL_SCANCODE_X: return 0x0;
        case SDL_SCANCODE_C: return 0xB;
        case SDL_SCANCODE_V: return 0xF;
        default: return -1;
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: chip8 <rom-file>\n";
        return 1;
    }

    Chip8 chip8;
    if (!chip8.loadRom(argv[1])) {
        return 1;
    }

    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("CHIP-8 Emulator",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          Chip8::kScreenWidth * kScale,
                                          Chip8::kScreenHeight * kScale, 0);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // A small 64x32 image that we update every frame. SDL stretches it to fill the window.
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             Chip8::kScreenWidth, Chip8::kScreenHeight);
    if (texture == nullptr) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::array<std::uint32_t, Chip8::kScreenWidth * Chip8::kScreenHeight> pixels{};

    bool running = true;
    while (running) {
        const Uint32 frameStart = SDL_GetTicks();

        // 1. Handle events (close button, Esc key, keypad keys).
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                const bool pressed = (event.type == SDL_KEYDOWN);
                if (pressed && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                }
                chip8.setKey(mapKey(event.key.keysym.scancode), pressed);
            }
        }

        // 2. Run the CPU for a few instructions.
        for (int i = 0; i < kInstructionsPerFrame; ++i) {
            chip8.cycle();
                    
           
        }  chip8.tickTimers();  // once per frame = 60 times per second

        // 3. Turn the CHIP-8 display (0s and 1s) into colors and show it.
        const auto& display = chip8.display();
        for (std::size_t i = 0; i < display.size(); ++i) {
            pixels[i] = display[i] ? kColorOn : kColorOff;
        }
        SDL_UpdateTexture(texture, nullptr, pixels.data(),
                          Chip8::kScreenWidth * sizeof(std::uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // 4. Wait so that we run at about 60 frames per second.
        const Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < kFrameMs) {
            SDL_Delay(kFrameMs - elapsed);
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}