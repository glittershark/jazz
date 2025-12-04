cmake_minimum_required(VERSION 3.16)

# Create cursed_lowpass_filter executable (requires Daisy libraries)
add_executable(cursed_lowpass_filter cursed_lowpass_filter.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(cursed_lowpass_filter PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(cursed_lowpass_filter PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

