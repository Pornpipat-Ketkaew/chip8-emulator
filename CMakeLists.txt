cmake_minimum_required(VERSION 3.24)
project(chip8 VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- The emulator core: pure C++, no SDL. Used by the app, the web version and the tests. ---
add_library(chip8_core src/chip8.cpp)
target_include_directories(chip8_core PUBLIC src)
target_compile_options(chip8_core PRIVATE -Wall -Wextra -Wpedantic)

# --- The app: core + SDL window, keyboard, drawing and sound. ---
add_executable(chip8 src/main.cpp)
target_link_libraries(chip8 PRIVATE chip8_core)
target_compile_options(chip8 PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(chip8 PRIVATE CHIP8_VERSION="${PROJECT_VERSION}")

if(EMSCRIPTEN)
    # Web version: Emscripten compiles C++ to WebAssembly and provides its own SDL2.
    target_compile_options(chip8 PRIVATE -sUSE_SDL=2)
    target_link_options(chip8 PRIVATE
        -sUSE_SDL=2
        -sEXPORTED_RUNTIME_METHODS=ccall,FS
        --preload-file ${CMAKE_SOURCE_DIR}/roms/br8kout.ch8@/roms/br8kout.ch8
        --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
    )
    # Produces index.html + index.js + index.wasm + index.data
    set_target_properties(chip8 PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    # Rebuild the page when the HTML template changes.
    set_property(TARGET chip8 APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
else()
    find_package(SDL2 REQUIRED)
    target_link_libraries(chip8 PRIVATE SDL2::SDL2)
endif()

# --- Unit tests with GoogleTest (downloaded automatically the first time). ---
option(CHIP8_BUILD_TESTS "Build the unit tests" ON)
if(CHIP8_BUILD_TESTS AND NOT EMSCRIPTEN)
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
