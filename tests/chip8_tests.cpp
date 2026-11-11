#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "chip8.h"

namespace {

// Builds a Chip8 with a small program written as a list of opcodes.
// Example: makeChip8({0x600A, 0x7005}) means "V0 = 0x0A, then V0 += 5".
Chip8 makeChip8(std::initializer_list<std::uint16_t> opcodes) {
    std::vector<std::uint8_t> bytes;
    for (std::uint16_t op : opcodes) {
        bytes.push_back(static_cast<std::uint8_t>(op >> 8));    // high byte first
        bytes.push_back(static_cast<std::uint8_t>(op & 0xFF));  // then low byte
    }
    Chip8 chip8;
    EXPECT_TRUE(chip8.loadProgram(bytes));
    return chip8;
}

void runCycles(Chip8& chip8, int count) {
    for (int i = 0; i < count; ++i) {
        chip8.cycle();
    }
}

}  // namespace

// ---------- Startup and loading ----------

TEST(Chip8Startup, StartsAtProgramAddressWithFontLoaded) {
    Chip8 chip8;
    EXPECT_EQ(chip8.pc(), 0x200);
    EXPECT_EQ(chip8.readMemory(Chip8::kFontStart), 0xF0);  // first row of the "0" glyph
}

TEST(Chip8Loading, RejectsEmptyAndTooBigPrograms) {
    Chip8 chip8;
    EXPECT_FALSE(chip8.loadProgram({}));
    EXPECT_FALSE(chip8.loadProgram(std::vector<std::uint8_t>(4096 - 0x200 + 1, 0)));
    EXPECT_TRUE(chip8.loadProgram(std::vector<std::uint8_t>(4096 - 0x200, 0)));
}

TEST(Chip8Loading, RejectsMissingFile) {
    Chip8 chip8;
    EXPECT_FALSE(chip8.loadRom("this_file_does_not_exist.ch8"));
}

// ---------- Registers and math ----------

TEST(Chip8Registers, SetAndAddImmediate) {
    Chip8 chip8 = makeChip8({0x6A0F, 0x7A01});  // VA = 0x0F, VA += 1
    runCycles(chip8, 2);
    EXPECT_EQ(chip8.v(0xA), 0x10);
}

TEST(Chip8Registers, AddImmediateWrapsAndDoesNotTouchVF) {
    Chip8 chip8 = makeChip8({0x60FF, 0x6F07, 0x7002});  // V0 = 255, VF = 7, V0 += 2
    runCycles(chip8, 3);
    EXPECT_EQ(chip8.v(0x0), 0x01);  // 255 + 2 = 257 -> wraps to 1
    EXPECT_EQ(chip8.v(0xF), 0x07);  // 7XNN never changes VF
}

TEST(Chip8Math, AddSetsCarryFlag) {
    Chip8 chip8 = makeChip8({0x60F0, 0x6120, 0x8014});  // V0 = 0xF0, V1 = 0x20, V0 += V1
    runCycles(chip8, 3);
    EXPECT_EQ(chip8.v(0x0), 0x10);
    EXPECT_EQ(chip8.v(0xF), 1);
}

TEST(Chip8Math, SubtractSetsNoBorrowFlag) {
    Chip8 chip8 = makeChip8({0x6005, 0x6103, 0x8015});  // 5 - 3
    runCycles(chip8, 3);
    EXPECT_EQ(chip8.v(0x0), 2);
    EXPECT_EQ(chip8.v(0xF), 1);  // no borrow

    Chip8 borrow = makeChip8({0x6003, 0x6105, 0x8015});  // 3 - 5
    runCycles(borrow, 3);
    EXPECT_EQ(borrow.v(0x0), 0xFE);  // wraps around
    EXPECT_EQ(borrow.v(0xF), 0);     // borrow happened
}

TEST(Chip8Math, FlagWinsWhenVFIsAlsoTheResultRegister) {
    Chip8 chip8 = makeChip8({0x6FF0, 0x6120, 0x8F14});  // VF = 0xF0, V1 = 0x20, VF += V1
    runCycles(chip8, 3);
    EXPECT_EQ(chip8.v(0xF), 1);  // the carry flag is written last
}

TEST(Chip8Math, ShiftsUseVYAndSetFlag) {
    Chip8 chip8 = makeChip8({0x6105, 0x8016, 0x6281, 0x832E});  // V0 = V1 >> 1, V3 = V2 << 1
    runCycles(chip8, 2);
    EXPECT_EQ(chip8.v(0x0), 0x02);
    EXPECT_EQ(chip8.v(0xF), 1);  // bit shifted out of 0b101
    runCycles(chip8, 2);
    EXPECT_EQ(chip8.v(0x3), 0x02);
    EXPECT_EQ(chip8.v(0xF), 1);  // top bit of 0x81
}

TEST(Chip8Math, LogicOpsResetVF) {
    Chip8 chip8 = makeChip8({0x6F01, 0x600C, 0x610A, 0x8011});  // VF = 1, V0 |= V1
    runCycles(chip8, 4);
    EXPECT_EQ(chip8.v(0x0), 0x0E);
    EXPECT_EQ(chip8.v(0xF), 0);
}

// ---------- Jumps, skips and subroutines ----------

TEST(Chip8Flow, JumpSetsPC) {
    Chip8 chip8 = makeChip8({0x1234});
    chip8.cycle();
    EXPECT_EQ(chip8.pc(), 0x234);
}

TEST(Chip8Flow, SkipIfEqualSkipsOneInstruction) {
    Chip8 chip8 = makeChip8({0x6042, 0x3042});  // V0 = 0x42, skip if V0 == 0x42
    runCycles(chip8, 2);
    EXPECT_EQ(chip8.pc(), 0x206);  // 0x204 + 2 (skipped)
}

TEST(Chip8Flow, CallAndReturn) {
    // 0x200: call 0x206 | 0x202: V0 = 1 | 0x204: (unused) | 0x206: V1 = 2 | 0x208: return
    Chip8 chip8 = makeChip8({0x2206, 0x6001, 0x0000, 0x6102, 0x00EE});
    runCycles(chip8, 3);  // call, V1 = 2, return
    EXPECT_EQ(chip8.pc(), 0x202);
    EXPECT_EQ(chip8.v(0x1), 2);
    chip8.cycle();
    EXPECT_EQ(chip8.v(0x0), 1);
}

TEST(Chip8Flow, StackOverflowDoesNotCrash) {
    Chip8 chip8 = makeChip8({0x2200});  // calls itself forever
    runCycles(chip8, 16);               // 16 calls fill the whole stack
    EXPECT_EQ(chip8.pc(), 0x200);
    chip8.cycle();                      // the 17th call must be refused, not overflow
    EXPECT_EQ(chip8.pc(), 0x202);
}

TEST(Chip8Flow, ReturnWithEmptyStackDoesNotCrash) {
    Chip8 chip8 = makeChip8({0x00EE});
    chip8.cycle();
    EXPECT_EQ(chip8.pc(), 0x202);
}

// ---------- Display ----------

TEST(Chip8Display, DrawsFontSpriteAndDetectsCollision) {
    // V0 = 0, I = font "0", draw 5 rows at (0,0), then draw again at the same place
    Chip8 chip8 = makeChip8({0x6000, 0xF029, 0xD005, 0xD005});
    runCycles(chip8, 3);
    const auto& d = chip8.display();
    EXPECT_EQ(d[0], 1);                                    // top-left pixel of "0" is on
    EXPECT_EQ(d[Chip8::kScreenWidth * 1 + 1], 0);          // inside the "0" is off
    EXPECT_EQ(chip8.v(0xF), 0);                            // nothing was erased

    chip8.cycle();                                          // XOR draw again -> erases it
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(chip8.v(0xF), 1);                            // collision!
}

TEST(Chip8Display, SpritesAreClippedAtTheEdge) {
    // V0 = 62 (2 pixels from the right edge), V1 = 0, I = font "0", draw
    Chip8 chip8 = makeChip8({0x603E, 0x6100, 0x6200, 0xF229, 0xD015});
    runCycles(chip8, 5);
    const auto& d = chip8.display();
    EXPECT_EQ(d[62], 1);
    EXPECT_EQ(d[63], 1);
    EXPECT_EQ(d[0], 0);  // must NOT wrap around to the left side
}

TEST(Chip8Display, ClearScreen) {
    Chip8 chip8 = makeChip8({0x6000, 0xF029, 0xD005, 0x00E0});
    runCycles(chip8, 4);
    for (auto pixel : chip8.display()) {
        EXPECT_EQ(pixel, 0);
    }
}

// ---------- Memory instructions ----------

TEST(Chip8Memory, BinaryCodedDecimal) {
    Chip8 chip8 = makeChip8({0x60FE, 0xA300, 0xF033});  // V0 = 254, I = 0x300, BCD
    runCycles(chip8, 3);
    EXPECT_EQ(chip8.readMemory(0x300), 2);
    EXPECT_EQ(chip8.readMemory(0x301), 5);
    EXPECT_EQ(chip8.readMemory(0x302), 4);
}

TEST(Chip8Memory, StoreAndLoadRegisters) {
    // V0=1, V1=2, V2=3, I=0x300, store V0..V2, clear V0..V2, I=0x300, load V0..V2
    Chip8 chip8 = makeChip8({0x6001, 0x6102, 0x6203, 0xA300, 0xF255,
                             0x6000, 0x6100, 0x6200, 0xA300, 0xF265});
    runCycles(chip8, 5);
    EXPECT_EQ(chip8.readMemory(0x302), 3);
    EXPECT_EQ(chip8.indexRegister(), 0x303);  // original CHIP-8 moves I forward
    runCycles(chip8, 5);
    EXPECT_EQ(chip8.v(0x0), 1);
    EXPECT_EQ(chip8.v(0x1), 2);
    EXPECT_EQ(chip8.v(0x2), 3);
}

// ---------- Timers and keys ----------

TEST(Chip8Timers, CountDownAndStopAtZero) {
    Chip8 chip8 = makeChip8({0x6002, 0xF015, 0xF018});  // delay = sound = 2
    runCycles(chip8, 3);
    EXPECT_EQ(chip8.delayTimer(), 2);
    EXPECT_TRUE(chip8.isSoundOn());
    chip8.tickTimers();
    chip8.tickTimers();
    chip8.tickTimers();  // one extra tick: must stay at 0, not wrap to 255
    EXPECT_EQ(chip8.delayTimer(), 0);
    EXPECT_FALSE(chip8.isSoundOn());
}

TEST(Chip8Keys, SkipIfKeyPressed) {
    Chip8 chip8 = makeChip8({0x6005, 0xE09E});  // V0 = 5, skip if key 5 is down
    chip8.setKey(5, true);
    runCycles(chip8, 2);
    EXPECT_EQ(chip8.pc(), 0x206);
}

TEST(Chip8Keys, WaitForKeyNeedsPressAndRelease) {
    Chip8 chip8 = makeChip8({0xF30A});  // V3 = next key
    runCycles(chip8, 5);
    EXPECT_EQ(chip8.pc(), 0x200);  // still waiting

    chip8.setKey(0xB, true);
    runCycles(chip8, 5);
    EXPECT_EQ(chip8.pc(), 0x200);  // pressed, but not released yet

    chip8.setKey(0xB, false);
    chip8.cycle();
    EXPECT_EQ(chip8.pc(), 0x202);
    EXPECT_EQ(chip8.v(0x3), 0xB);
}

TEST(Chip8Keys, IgnoresInvalidKeyNumbers) {
    Chip8 chip8;
    chip8.setKey(-1, true);  // must not crash
    chip8.setKey(16, true);
    SUCCEED();
}