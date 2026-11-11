#include "chip8.h"
#include <cstdio>
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

void Chip8::cycle() {
    // FETCH: an instruction is 2 bytes. Join them into one 16-bit number.
    const std::uint16_t opcode =
        static_cast<std::uint16_t>((memory_[pc_ & 0xFFF] << 8) | memory_[(pc_ + 1) & 0xFFF]);
    pc_ += 2;

    // DECODE: cut the opcode into the pieces instructions use.
    const std::uint8_t x = (opcode >> 8) & 0x0F;   // 2nd nibble: a register number
    const std::uint8_t y = (opcode >> 4) & 0x0F;   // 3rd nibble: a register number
    const std::uint8_t n = opcode & 0x0F;          // 4th nibble: a 4-bit number
    const std::uint8_t nn = opcode & 0xFF;         // last 2 nibbles: an 8-bit number
    const std::uint16_t nnn = opcode & 0x0FFF;     // last 3 nibbles: a 12-bit address

    // EXECUTE: the first nibble says which kind of instruction it is.
    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) {  // 00E0: clear the screen
                display_.fill(0);
            } else {
                std::fprintf(stderr, "Unknown opcode %04X at %03X\n", opcode, pc_ - 2);
            }
            break;
        case 0x1000:  // 1NNN: jump to address NNN
            pc_ = nnn;
            break;
        case 0x6000:  // 6XNN: VX = NN
            v_[x] = nn;
            break;
        case 0x7000:  // 7XNN: VX += NN
            v_[x] += nn;
            break;
        case 0xA000:  // ANNN: I = NNN
            i_ = nnn;
            break;
        case 0xD000:  // DXYN: draw an N-row sprite at (VX, VY)
            drawSprite(v_[x], v_[y], n);
            break;
        default:
            std::fprintf(stderr, "Unknown opcode %04X at %03X\n", opcode, pc_ - 2);
            break;
    }
}

void Chip8::drawSprite(std::uint8_t vx, std::uint8_t vy, std::uint8_t height) {
    const int startX = vx % kScreenWidth;   // the start position wraps around...
    const int startY = vy % kScreenHeight;
    v_[0xF] = 0;

    for (int row = 0; row < height; ++row) {
        const int py = startY + row;
        if (py >= kScreenHeight) break;     // ...but the sprite itself is clipped at the edge
        const std::uint8_t spriteByte = memory_[(i_ + row) & 0xFFF];

        for (int col = 0; col < 8; ++col) {
            const int px = startX + col;
            if (px >= kScreenWidth) break;
            if ((spriteByte & (0x80 >> col)) == 0) continue;  // this sprite pixel is off

            std::uint8_t& pixel = display_[py * kScreenWidth + px];
            if (pixel == 1) {
                v_[0xF] = 1;  // collision: we're turning an "on" pixel off
            }
            pixel ^= 1;       // XOR: flip the pixel
        }
    }
}