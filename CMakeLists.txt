cmake_minimum_required(VERSION 3.24)
project(chip8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- The emulator core: pure C++, no SDL. Used by both the app and the tests. ---
add_library(chip8_core src/chip8.cpp)
target_include_directories(chip8_core PUBLIC src)
target_compile_options(chip8_core PRIVATE -Wall -Wextra -Wpedantic)

# --- The app: core + SDL window, keyboard and drawing. ---
find_package(SDL2 REQUIRED)
add_executable(chip8 src/main.cpp)
target_link_libraries(chip8 PRIVATE chip8_core SDL2::SDL2)
target_compile_options(chip8 PRIVATE -Wall -Wextra -Wpedantic)

# --- Unit tests with GoogleTest (downloaded automatically the first time). ---
option(CHIP8_BUILD_TESTS "Build the unit tests" ON)
if(CHIP8_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(chip8_tests tests/chip8_tests.cpp)
    target_link_libraries(chip8_tests PRIVATE chip8_core GTest::gtest_main)
    target_compile_options(chip8_tests PRIVATE -Wall -Wextra -Wpedantic)

    include(GoogleTest)
    gtest_discover_tests(chip8_tests)
endif()