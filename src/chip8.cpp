#include "chip8.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {

// Built-in font: the digits 0-F, each 5 bytes tall (4 pixels wide).
constexpr std::array<std::uint8_t, 80> kFont = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
};

}  // namespace

Chip8::Chip8() {
    for (std::size_t i = 0; i < kFont.size(); ++i) {
        memory_[kFontStart + i] = kFont[i];
    }
}

bool Chip8::loadRom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Could not open ROM: " << path << '\n';
        return false;
    }

    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());

    const std::size_t maxSize = kMemorySize - kProgramStart;
    if (bytes.empty() || bytes.size() > maxSize) {
        std::cerr << "ROM size " << bytes.size() << " bytes is invalid (max " << maxSize << ")\n";
        return false;
    }

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        memory_[kProgramStart + i] = bytes[i];
    }
    std::cout << "Loaded " << bytes.size() << " bytes from " << path << '\n';
    return true;
}