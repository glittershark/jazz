cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)

# Check if this is a test-only build (host platform)
option(TEST_ONLY "Build only tests for host platform" OFF)
option(BUILD_TESTS "Build host tests when TEST_ONLY is enabled" ON)

# Default: Use ARM toolchain for hardware projects
if(NOT TEST_ONLY)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-toolchain.cmake")
    message(STATUS "Building for ARM Cortex-M7 (Daisy Seed)")
else()
    message(STATUS "Building tests only for host platform")
endif()

project(jazz)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Always add the libjazz library
add_subdirectory(lib/libjazz)

if(NOT TEST_ONLY)
    # Add vendor libraries
    add_subdirectory(vendor/libDaisy)
    add_subdirectory(vendor/DaisySP)
    
    # Add src directories
    add_subdirectory(src/delay)
    add_subdirectory(src/reverse)
    add_subdirectory(src/granular)
    add_subdirectory(src/fridge)
    add_subdirectory(src/identity)
    add_subdirectory(src/rachel)
    add_subdirectory(src/anticlip)
    add_subdirectory(src/clip)
    add_subdirectory(src/low_pass_filter)
    add_subdirectory(src/high_pass_filter)
    add_subdirectory(src/cursed_low_pass_filter)
    add_subdirectory(src/cursed_high_pass_filter)
    add_subdirectory(src/la_sort)
else()
    # Test-only build
    enable_testing()
    
    # Add src directories for testing (without hardware dependencies)
    add_subdirectory(src/delay)
    add_subdirectory(src/reverse)
    add_subdirectory(src/granular)
    add_subdirectory(src/fridge)
    add_subdirectory(src/rachel)

    if(BUILD_TESTS)
        add_subdirectory(tests)
    endif()
endif()
