cmake_minimum_required(VERSION 3.16)

# Create identity executable (requires Daisy libraries)
add_executable(identity identity.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(identity PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(identity PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

