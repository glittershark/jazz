cmake_minimum_required(VERSION 3.16)

# Create cursed_highpass_filter executable (requires Daisy libraries)
add_executable(cursed_highpass_filter cursed_highpass_filter.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(cursed_highpass_filter PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(cursed_highpass_filter PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

