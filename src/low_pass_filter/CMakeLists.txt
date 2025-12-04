cmake_minimum_required(VERSION 3.16)

# Create lowpass_filter executable (requires Daisy libraries)
add_executable(lowpass_filter lowpass_filter.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(lowpass_filter PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(lowpass_filter PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

