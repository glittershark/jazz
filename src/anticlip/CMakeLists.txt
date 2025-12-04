cmake_minimum_required(VERSION 3.16)

# Note: This executable requires the Daisy platform libraries
# To build this, you would need to:
# 1. Install the Daisy development environment
# 2. Add appropriate find_package or library paths for Daisy
# 3. Link against the Daisy libraries

# Create anticlip executable (requires Daisy libraries)
add_executable(anticlip anticlip.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(anticlip PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(anticlip PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

