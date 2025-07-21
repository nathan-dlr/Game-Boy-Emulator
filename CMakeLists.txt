cmake_minimum_required(VERSION 3.27)
project(gb_emu)

set(CMAKE_C_STANDARD 23)


if(GB_EMU_VENDORED)
    # This assumes you have added SDL as a submodule in vendored/SDL
    add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
else()
    # 1. Look for a SDL3 package,
    # 2. look for the SDL3-shared component, and
    # 3. fail if the shared component cannot be found.
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif()

add_executable(gb_emu
        src/gb.c
        src/cpu.c
        src/decode.c
        src/queue.c
        src/lcd.c
        src/ppu.c
)

target_include_directories(gb_emu PUBLIC inc)
target_link_libraries(gb_emu PRIVATE SDL3::SDL3)