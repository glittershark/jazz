cmake_minimum_required(VERSION 3.16)

# Check if we're building for tests only
if(TEST_ONLY)
    # Create granular library for testing (without Daisy dependencies)
    add_library(granular STATIC granular.cpp)
    
    # Link against libjazz library only
    target_link_libraries(granular PUBLIC jazz::libjazz)
    
    # Expose include directory so tests can include headers from this module
    target_include_directories(granular PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/granular>
    )
    
    # Define UNIT_TEST to conditionally compile out Daisy-specific code
    target_compile_definitions(granular PUBLIC UNIT_TEST)
    
    set_target_properties(granular PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
    )
    
    # Create alias for consistent naming
    add_library(jazz::granular ALIAS granular)
else()
    # Note: This executable requires the Daisy platform libraries
    # To build this, you would need to:
    # 1. Install the Daisy development environment
    # 2. Add appropriate find_package or library paths for Daisy
    # 3. Link against the Daisy libraries

    # Create granular executable (requires Daisy libraries)
    add_executable(granular granular.cpp)

    # Link against libjazz library and Daisy libraries
    target_link_libraries(granular PRIVATE 
        jazz::libjazz
        daisy
        DaisySP
    )

    set_target_properties(granular PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
    )
endif()
