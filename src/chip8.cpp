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
            if (opcode == 0x00E0) {          // 00E0: clear the screen
                display_.fill(0);
            } else if (opcode == 0x00EE) {   // 00EE: return from a subroutine
                if (sp_ == 0) {
                    std::fprintf(stderr, "Stack underflow at %03X\n", pc_ - 2);
                    break;
                }
                --sp_;
                pc_ = stack_[sp_];
            } else {
                unknownOpcode(opcode);
            }
            break;
        case 0x1000:  // 1NNN: jump to NNN
            pc_ = nnn;
            break;
        case 0x2000:  // 2NNN: call the subroutine at NNN
            if (sp_ >= stack_.size()) {
                std::fprintf(stderr, "Stack overflow at %03X\n", pc_ - 2);
                break;
            }
            stack_[sp_] = pc_;
            ++sp_;
            pc_ = nnn;
            break;
        case 0x3000:  // 3XNN: skip the next instruction if VX == NN
            if (v_[x] == nn) pc_ += 2;
            break;
        case 0x4000:  // 4XNN: skip if VX != NN
            if (v_[x] != nn) pc_ += 2;
            break;
        case 0x5000:  // 5XY0: skip if VX == VY
            if (v_[x] == v_[y]) pc_ += 2;
            break;
        case 0x6000:  // 6XNN: VX = NN
            v_[x] = nn;
            break;
        case 0x7000:  // 7XNN: VX += NN (no carry flag)
            v_[x] += nn;
            break;
        case 0x8000:
            executeArithmetic(x, y, n);
            break;
        case 0x9000:  // 9XY0: skip if VX != VY
            if (v_[x] != v_[y]) pc_ += 2;
            break;
        case 0xA000:  // ANNN: I = NNN
            i_ = nnn;
            break;
        case 0xB000:  // BNNN: jump to NNN + V0
            pc_ = static_cast<std::uint16_t>(nnn + v_[0]);
            break;
        case 0xC000: {  // CXNN: VX = random number AND NN
            std::uniform_int_distribution<int> dist(0, 255);
            v_[x] = static_cast<std::uint8_t>(dist(rng_) & nn);
            break;
        }
        case 0xD000:  // DXYN: draw an N-row sprite at (VX, VY)
            drawSprite(v_[x], v_[y], n);
            break;
        case 0xE000:
            if (nn == 0x9E) {         // EX9E: skip if key VX is pressed
                if (keys_[v_[x] & 0xF]) pc_ += 2;
            } else if (nn == 0xA1) {  // EXA1: skip if key VX is NOT pressed
                if (!keys_[v_[x] & 0xF]) pc_ += 2;
            } else {
                unknownOpcode(opcode);
            }
            break;
        case 0xF000:
            executeMisc(opcode, x, nn);
            break;
    }
}

void Chip8::executeArithmetic(std::uint8_t x, std::uint8_t y, std::uint8_t n) {
    std::uint8_t& vx = v_[x];
    const std::uint8_t vy = v_[y];

    switch (n) {
        case 0x0:  // 8XY0: VX = VY
            vx = vy;
            break;
        case 0x1:  // 8XY1: VX |= VY (OR)
            vx |= vy;
            v_[0xF] = 0;
            break;
        case 0x2:  // 8XY2: VX &= VY (AND)
            vx &= vy;
            v_[0xF] = 0;
            break;
        case 0x3:  // 8XY3: VX ^= VY (XOR)
            vx ^= vy;
            v_[0xF] = 0;
            break;
        case 0x4: {  // 8XY4: VX += VY, VF = 1 if the result didn't fit in 8 bits (carry)
            const int sum = vx + vy;
            vx = static_cast<std::uint8_t>(sum);
            v_[0xF] = sum > 0xFF ? 1 : 0;
            break;
        }
        case 0x5: {  // 8XY5: VX -= VY, VF = 1 if there was NO borrow
            const std::uint8_t flag = vx >= vy ? 1 : 0;
            vx = static_cast<std::uint8_t>(vx - vy);
            v_[0xF] = flag;
            break;
        }
        case 0x6: {  // 8XY6: VX = VY >> 1, VF = the bit that was shifted out
            const std::uint8_t flag = vy & 0x1;
            vx = static_cast<std::uint8_t>(vy >> 1);
            v_[0xF] = flag;
            break;
        }
        case 0x7: {  // 8XY7: VX = VY - VX, VF = 1 if there was NO borrow
            const std::uint8_t flag = vy >= vx ? 1 : 0;
            vx = static_cast<std::uint8_t>(vy - vx);
            v_[0xF] = flag;
            break;
        }
        case 0xE: {  // 8XYE: VX = VY << 1, VF = the bit that was shifted out
            const std::uint8_t flag = (vy >> 7) & 0x1;
            vx = static_cast<std::uint8_t>(vy << 1);
            v_[0xF] = flag;
            break;
        }
        default:
            unknownOpcode(static_cast<std::uint16_t>(0x8000 | (x << 8) | (y << 4) | n));
            break;
    }
}

void Chip8::executeMisc(std::uint16_t opcode, std::uint8_t x, std::uint8_t nn) {
    switch (nn) {
        case 0x07:  // FX07: VX = delay timer
            v_[x] = delayTimer_;
            break;
        case 0x0A:  // FX0A: wait until a key is pressed AND released, put it in VX
            if (waitingKey_ == -1) {
                for (int k = 0; k < 16; ++k) {
                    if (keys_[k]) {
                        waitingKey_ = k;
                        break;
                    }
                }
                pc_ -= 2;  // run this same instruction again next cycle
            } else if (keys_[waitingKey_]) {
                pc_ -= 2;  // still held down: keep waiting for release
            } else {
                v_[x] = static_cast<std::uint8_t>(waitingKey_);
                waitingKey_ = -1;
            }
            break;
        case 0x15:  // FX15: delay timer = VX
            delayTimer_ = v_[x];
            break;
        case 0x18:  // FX18: sound timer = VX
            soundTimer_ = v_[x];
            break;
        case 0x1E:  // FX1E: I += VX
            i_ = static_cast<std::uint16_t>((i_ + v_[x]) & 0xFFF);
            break;
        case 0x29:  // FX29: I = address of the font character for digit VX
            i_ = static_cast<std::uint16_t>(kFontStart + (v_[x] & 0xF) * 5);
            break;
        case 0x33:  // FX33: store VX as 3 decimal digits (e.g. 254 -> 2, 5, 4)
            memory_[i_ & 0xFFF] = v_[x] / 100;
            memory_[(i_ + 1) & 0xFFF] = (v_[x] / 10) % 10;
            memory_[(i_ + 2) & 0xFFF] = v_[x] % 10;
            break;
        case 0x55:  // FX55: save V0..VX into memory starting at I
            for (int r = 0; r <= x; ++r) {
                memory_[i_ & 0xFFF] = v_[r];
                ++i_;
            }
            break;
        case 0x65:  // FX65: load V0..VX from memory starting at I
            for (int r = 0; r <= x; ++r) {
                v_[r] = memory_[i_ & 0xFFF];
                ++i_;
            }
            break;
        default:
            unknownOpcode(opcode);
            break;
    }
}

void Chip8::tickTimers() {
    if (delayTimer_ > 0) --delayTimer_;
    if (soundTimer_ > 0) --soundTimer_;
}

void Chip8::setKey(int key, bool pressed) {
    if (key >= 0 && key < 16) {
        keys_[key] = pressed;
    }
}

void Chip8::unknownOpcode(std::uint16_t opcode) const {
    std::fprintf(stderr, "Unknown opcode %04X at %03X\n", opcode, pc_ - 2);
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