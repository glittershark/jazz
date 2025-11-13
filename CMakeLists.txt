cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)
project(jazz)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the libjazz library
add_subdirectory(lib/libjazz)

# Add src directories (commented out for now as they require Daisy platform)
# add_subdirectory(src/delay)
# add_subdirectory(src/reverse)

enable_testing()
add_subdirectory(tests)
