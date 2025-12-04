cmake_minimum_required(VERSION 3.16)

# Check if we're building for tests only
if(TEST_ONLY)
    # Create reverse library for testing (without Daisy dependencies)
    add_library(reverse STATIC reverse.cpp)
    
    # Link against libjazz library only
    target_link_libraries(reverse PUBLIC jazz::libjazz)
    
    # Expose include directory so tests can include headers from this module
    target_include_directories(reverse PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/reverse>
    )
    
    # Define UNIT_TEST to conditionally compile out Daisy-specific code
    target_compile_definitions(reverse PUBLIC UNIT_TEST)
    
    set_target_properties(reverse PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
    )
    
    # Create alias for consistent naming
    add_library(jazz::reverse ALIAS reverse)
else()
    # Note: This executable requires the Daisy platform libraries
    # To build this, you would need to:
    # 1. Install the Daisy development environment
    # 2. Add appropriate find_package or library paths for Daisy
    # 3. Link against the Daisy libraries

    # Create reverse executable (requires Daisy libraries)
    add_executable(reverse reverse.cpp)

    # Link against libjazz library and Daisy libraries
    target_link_libraries(reverse PRIVATE 
        jazz::libjazz
        daisy
        DaisySP
    )

    set_target_properties(reverse PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
    )
endif()