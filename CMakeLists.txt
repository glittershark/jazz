cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)

# Check if this is a test-only build (host platform)
option(TEST_ONLY "Build only tests for host platform" OFF)

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
else()
    # Test-only build
    enable_testing()
    add_subdirectory(tests)
endif()
