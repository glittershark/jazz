cmake_minimum_required(VERSION 3.16)

# Check if we're building for tests only
if(TEST_ONLY)
    # Create fridge library for testing (without Daisy dependencies)
    add_library(fridge STATIC fridge.cc pwm.cc color.cc)
    
    # Link against libjazz library only
    target_link_libraries(fridge PUBLIC jazz::libjazz)
    
    # Expose include directory so tests can include headers from this module
    target_include_directories(fridge PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/fridge>
    )
    
    # Define UNIT_TEST to conditionally compile out Daisy-specific code
    target_compile_definitions(fridge PUBLIC UNIT_TEST)
    
    set_target_properties(fridge PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
    )
    
    # Create alias for consistent naming
    add_library(jazz::fridge ALIAS fridge)
else()
    # Note: This executable requires the Daisy platform libraries
    # To build this, you would need to:
    # 1. Install the Daisy development environment
    # 2. Add appropriate find_package or library paths for Daisy
    # 3. Link against the Daisy libraries

    # Create fridge executable (requires Daisy libraries)
    add_executable(fridge fridge.cc pwm.cc color.cc)

    # Link against libjazz library and Daisy libraries
    target_link_libraries(fridge PRIVATE
        jazz::libjazz
        daisy
        DaisySP
    )

    set_target_properties(fridge PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
    )
endif()
