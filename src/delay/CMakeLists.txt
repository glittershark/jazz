cmake_minimum_required(VERSION 3.16)

# Note: This executable requires the Daisy platform libraries
# To build this, you would need to:
# 1. Install the Daisy development environment
# 2. Add appropriate find_package or library paths for Daisy
# 3. Link against the Daisy libraries

# Create delay executable (requires Daisy libraries)
add_executable(delay delay.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(delay PRIVATE 
    jazz::libjazz
    # Add Daisy libraries here when available:
    # daisy_core
    # daisysp
)

set_target_properties(delay PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)