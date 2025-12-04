cmake_minimum_required(VERSION 3.16)

# Check if we're building for tests only
if(TEST_ONLY)
    # Create rachel library for testing (without Daisy dependencies)
    add_library(rachel STATIC rachel.cpp)
    
    # Link against libjazz library only
    target_link_libraries(rachel PUBLIC jazz::libjazz)
    
    # Expose include directory so tests can include headers from this module
    target_include_directories(rachel PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/rachel>
    )
    
    # Define UNIT_TEST to conditionally compile out Daisy-specific code
    target_compile_definitions(rachel PUBLIC UNIT_TEST)
    
    set_target_properties(rachel PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
    )
    
    # Create alias for consistent naming
    add_library(jazz::rachel ALIAS rachel)
else()
    # Note: This executable requires the Daisy platform libraries
    # To build this, you would need to:
    # 1. Install the Daisy development environment
    # 2. Add appropriate find_package or library paths for Daisy
    # 3. Link against the Daisy libraries

    # Create rachel executable (requires Daisy libraries)
    add_executable(rachel rachel.cpp)

    # Link against libjazz library and Daisy libraries
    target_link_libraries(rachel PRIVATE 
        jazz::libjazz
        daisy
        DaisySP
    )

    set_target_properties(rachel PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
    )
endif()