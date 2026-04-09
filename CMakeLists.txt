cmake_minimum_required(VERSION 3.20)
project(jazz)

include(cmake/jazz.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(lib/libjazz)

if(CMAKE_CROSSCOMPILING)
  add_subdirectory(vendor/libDaisy)
  add_subdirectory(vendor/DaisySP)
endif()

# Firmware projects (build for both ARM and host tests)
add_subdirectory(src/delay)
add_subdirectory(src/reverse)
add_subdirectory(src/granular)
add_subdirectory(src/fridge)
add_subdirectory(src/rachel)

# ARM-only projects
add_subdirectory(src/identity)
add_subdirectory(src/anticlip)
add_subdirectory(src/clip)
add_subdirectory(src/low_pass_filter)
add_subdirectory(src/high_pass_filter)
add_subdirectory(src/cursed_low_pass_filter)
add_subdirectory(src/cursed_high_pass_filter)
add_subdirectory(src/la_sort)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  add_subdirectory(tests)
  add_subdirectory(src/console)
endif()
