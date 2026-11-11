#pragma once

#include <array>
#include <cstdint>
#include <string>

class Chip8 {
public:
    static constexpr int kMemorySize = 4096;
    static constexpr int kScreenWidth = 64;
    static constexpr int kScreenHeight = 32;
    static constexpr std::uint16_t kProgramStart = 0x200;
    static constexpr std::uint16_t kFontStart = 0x050;

    Chip8();

    // Copies a .ch8 program file into memory at 0x200.
    // Returns false (and prints why) if the file can't be loaded.
    bool loadRom(const std::string& path);

    std::uint8_t readMemory(std::uint16_t address) const { return memory_[address]; }
    std::uint16_t pc() const { return pc_; }

private:
    std::array<std::uint8_t, kMemorySize> memory_{};   // 4 KB of RAM
    std::array<std::uint8_t, 16> v_{};                 // registers V0..VF
    std::uint16_t i_ = 0;                              // index register
    std::uint16_t pc_ = kProgramStart;                 // program counter
    std::array<std::uint16_t, 16> stack_{};            // return addresses
    std::uint8_t sp_ = 0;                              // stack pointer
    std::uint8_t delayTimer_ = 0;
    std::uint8_t soundTimer_ = 0;
    std::array<std::uint8_t, kScreenWidth * kScreenHeight> display_{};  // 1 = pixel on
    std::array<bool, 16> keys_{};                      // keypad 0..F
};