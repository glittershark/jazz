cmake_minimum_required(VERSION 3.16)

# Note: This executable requires the Daisy platform libraries
# To build this, you would need to:
# 1. Install the Daisy development environment
# 2. Add appropriate find_package or library paths for Daisy
# 3. Link against the Daisy libraries

# Create granular executable (requires Daisy libraries)
add_executable(granular granular.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(granular PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(granular PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)