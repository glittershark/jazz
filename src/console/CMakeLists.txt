add_executable(jazz-console console.cpp)
target_link_libraries(jazz-console PRIVATE jazz::libjazz jazz::granular)
target_compile_definitions(jazz-console PRIVATE UNIT_TEST)
target_compile_features(jazz-console PRIVATE cxx_std_20)
