cmake_minimum_required(VERSION 3.16)

# Create la_sort executable (requires Daisy libraries)
add_executable(la_sort la_sort.cpp)

# Link against libjazz library and Daisy libraries
target_link_libraries(la_sort PRIVATE 
    jazz::libjazz
    daisy
    DaisySP
)

set_target_properties(la_sort PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)


