#define SDL_MAIN_HANDLED
#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

#include "chip8.h"

#ifndef CHIP8_VERSION
#define CHIP8_VERSION "dev"
#endif

namespace {

constexpr const char* kTitle = "CHIP-8 Emulator v" CHIP8_VERSION;

constexpr int kScale = 10;                // each CHIP-8 pixel becomes a 10x10 square
constexpr int kInstructionsPerFrame = 10; // 10 x 60 frames = 600 instructions per second
#ifndef __EMSCRIPTEN__
constexpr Uint32 kFrameMs = 1000 / 60;    // one frame = about 16 ms (60 frames per second)
#endif

constexpr std::uint32_t kColorOn = 0xFFE0E0E0;   // light grey (format: 0xAARRGGBB)
constexpr std::uint32_t kColorOff = 0xFF141428;  // dark blue

// --- Sound: a simple 440 Hz "beep" (a square wave) ---
constexpr int kSampleRate = 44100;     // samples per second
constexpr int kBeepFrequency = 440;    // the musical note A4
constexpr Sint16 kBeepVolume = 3000;   // max is 32767, so this is quite quiet

// SDL calls this function by itself whenever the speaker needs more sound data.
// We fill the buffer with a square wave: +volume, -volume, +volume, ...
void audioCallback(void* userdata, Uint8* stream, int length) {
    int& sampleIndex = *static_cast<int*>(userdata);
    Sint16* samples = reinterpret_cast<Sint16*>(stream);
    const int count = length / static_cast<int>(sizeof(Sint16));
    const int halfPeriod = kSampleRate / (kBeepFrequency * 2);

    for (int i = 0; i < count; ++i) {
        samples[i] = ((sampleIndex / halfPeriod) % 2 == 0) ? kBeepVolume : -kBeepVolume;
        sampleIndex = (sampleIndex + 1) % (halfPeriod * 2);
    }
}

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

// Shows an error both in the terminal and in a pop-up window.
// The pop-up matters for people who double-click the .exe and never see a terminal.
void showError(const std::string& message) {
    std::cerr << message << '\n';
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, message.c_str(), nullptr);
}

// Everything the running emulator needs, kept together in one place.
struct App {
    Chip8 chip8;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_AudioDeviceID audio = 0;
    int sampleIndex = 0;
    bool running = true;
    std::array<std::uint32_t, Chip8::kScreenWidth * Chip8::kScreenHeight> pixels{};
};

// Starts a new game. The old game is only replaced if the new file loads correctly.
void loadGame(App& app, const std::string& path) {
    Chip8 fresh;
    if (fresh.loadRom(path)) {
        app.chip8 = fresh;
    } else {
        showError("Could not load ROM:\n" + path);
    }
}

// Creates the window, renderer, texture and (if possible) the sound device.
bool initSdl(App& app) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        showError(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    app.window = SDL_CreateWindow(kTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  Chip8::kScreenWidth * kScale, Chip8::kScreenHeight * kScale, 0);
    if (app.window == nullptr) {
        showError(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    app.renderer = SDL_CreateRenderer(app.window, -1, 0);
    if (app.renderer == nullptr) {
        showError(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        return false;
    }

    // A small 64x32 image that we update every frame. SDL stretches it to fill the window.
    app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    Chip8::kScreenWidth, Chip8::kScreenHeight);
    if (app.texture == nullptr) {
        showError(std::string("SDL_CreateTexture failed: ") + SDL_GetError());
        return false;
    }

    // Open the speaker. If there is no sound device, keep running without sound.
    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;   // 16-bit signed samples
    want.channels = 1;            // mono
    want.samples = 512;           // small buffer = low delay
    want.callback = audioCallback;
    want.userdata = &app.sampleIndex;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        app.audio = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    }
    if (app.audio == 0) {
        std::cerr << "No sound: " << SDL_GetError() << '\n';
    }
    return true;
}

void shutdownSdl(App& app) {
    if (app.audio != 0) SDL_CloseAudioDevice(app.audio);
    if (app.texture != nullptr) SDL_DestroyTexture(app.texture);
    if (app.renderer != nullptr) SDL_DestroyRenderer(app.renderer);
    if (app.window != nullptr) SDL_DestroyWindow(app.window);
    SDL_Quit();
}

// One frame of the emulator: input -> CPU -> timers -> sound -> picture.
void runFrame(App& app) {
    // 1. Handle events (close button, Esc key, keypad keys, dropped files).
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            app.running = false;
        } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            const bool pressed = (event.type == SDL_KEYDOWN);
            if (pressed && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                app.running = false;
            }
            app.chip8.setKey(mapKey(event.key.keysym.scancode), pressed);
        } else if (event.type == SDL_DROPFILE) {
            // A file was dragged onto the window: start that game from scratch.
            char* path = event.drop.file;
            loadGame(app, path);
            SDL_free(path);
        }
    }

    // 2. Run the CPU for a few instructions, then tick the timers once (60 Hz).
    for (int i = 0; i < kInstructionsPerFrame; ++i) {
        app.chip8.cycle();
    }
    app.chip8.tickTimers();

    // 3. Beep while the sound timer is above 0 (pause = silence).
    if (app.audio != 0) {
        SDL_PauseAudioDevice(app.audio, app.chip8.isSoundOn() ? 0 : 1);
    }

    // 4. Turn the CHIP-8 display (0s and 1s) into colors and show it.
    const auto& display = app.chip8.display();
    for (std::size_t i = 0; i < display.size(); ++i) {
        app.pixels[i] = display[i] ? kColorOn : kColorOff;
    }
    SDL_UpdateTexture(app.texture, nullptr, app.pixels.data(),
                      Chip8::kScreenWidth * sizeof(std::uint32_t));
    SDL_RenderClear(app.renderer);
    SDL_RenderCopy(app.renderer, app.texture, nullptr, nullptr);
    SDL_RenderPresent(app.renderer);
}

#ifdef __EMSCRIPTEN__
App* g_app = nullptr;

void webFrame() {
    runFrame(*g_app);
}
#endif

}  // namespace

#ifdef __EMSCRIPTEN__
// JavaScript in the web page calls this after the user picks a .ch8 file.
extern "C" EMSCRIPTEN_KEEPALIVE void loadRomFromWeb(const char* path) {
    if (g_app != nullptr) {
        loadGame(*g_app, path);
    }
}
#endif

int main(int argc, char* argv[]) {
#ifdef __EMSCRIPTEN__
    // On the web there is no command line: start with the game built into the page.
    const std::string romPath = (argc >= 2) ? argv[1] : "/roms/br8kout.ch8";
#else
    if (argc < 2) {
        const char* help =
            "How to play:\n"
            "Drag a .ch8 game file onto chip8.exe,\n"
            "or run: chip8 <rom-file>";
        std::cerr << help << '\n';
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, kTitle, help, nullptr);
        return 1;
    }
    const std::string romPath = argv[1];
#endif

    // static: on the web, the App must stay alive even after main() has finished.
    static App app;
    if (!app.chip8.loadRom(romPath)) {
        showError("Could not load ROM:\n" + romPath);
        return 1;
    }

    SDL_SetMainReady();
    if (!initSdl(app)) {
        shutdownSdl(app);
        return 1;
    }

#ifdef __EMSCRIPTEN__
    // A web page must never block the browser with an endless while loop.
    // Instead we ask the browser to call webFrame() 60 times per second.
    g_app = &app;
    emscripten_set_main_loop(webFrame, 60, true);
#else
    while (app.running) {
        const Uint32 frameStart = SDL_GetTicks();
        runFrame(app);

        // Wait so that we run at about 60 frames per second.
        const Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < kFrameMs) {
            SDL_Delay(kFrameMs - elapsed);
        }
    }
    shutdownSdl(app);
#endif
    return 0;
}
