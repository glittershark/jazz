cmake_minimum_required(VERSION 3.16)

# Check if we're building for tests only
if(TEST_ONLY)
    # Create delay library for testing (without Daisy dependencies)
    add_library(delay STATIC delay.cpp)
    
    # Link against libjazz library only
    target_link_libraries(delay PUBLIC jazz::libjazz)
    
    # Expose include directory so tests can include headers from this module
    target_include_directories(delay PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/delay>
    )
    
    # Define UNIT_TEST to conditionally compile out Daisy-specific code
    target_compile_definitions(delay PUBLIC UNIT_TEST)
    
    set_target_properties(delay PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
    )
    
    # Create alias for consistent naming
    add_library(jazz::delay ALIAS delay)
else()
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
        daisy
        DaisySP
    )

    set_target_properties(delay PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
    )
endif()