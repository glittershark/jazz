cmake_minimum_required(VERSION 3.16)
project(libjazz VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the library
add_library(libjazz STATIC
    src/value.cpp
)

# Set include directories
target_include_directories(libjazz 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set target properties
set_target_properties(libjazz PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

# Add alias for consistent naming
add_library(jazz::libjazz ALIAS libjazz)