cmake_minimum_required(VERSION 3.16)

# Note: This executable requires the Daisy platform libraries
# To build this, you would need to:
# 1. Install the Daisy development environment
# 2. Add appropriate find_package or library paths for Daisy
# 3. Link against the Daisy libraries

# Create clip executable (requires Daisy libraries)
add_executable(clip clip.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(clip PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(clip PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

