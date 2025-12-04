cmake_minimum_required(VERSION 3.16)

# Create highpass_filter executable (requires Daisy libraries)
add_executable(highpass_filter highpass_filter.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(highpass_filter PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(highpass_filter PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

