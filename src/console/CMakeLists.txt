cmake_minimum_required(VERSION 3.16)

if(TEST_ONLY)
    add_executable(jazz-console console.cpp)

    target_link_libraries(jazz-console PRIVATE
        jazz::libjazz
        jazz::granular
    )

    target_compile_definitions(jazz-console PRIVATE UNIT_TEST)

    set_target_properties(jazz-console PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
    )
else()
    add_executable(console console.cpp)

    target_link_libraries(console PRIVATE
        jazz::libjazz
        daisy
        DaisySP
    )

    set_target_properties(console PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
    )
endif()
