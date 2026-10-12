add_library(jazz-console-fridge STATIC
  ../fridge/mod.cpp
  ../fridge/sound.cpp
)
target_link_libraries(jazz-console-fridge PUBLIC jazz::libjazz)
target_include_directories(jazz-console-fridge PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/fridge>
)
target_compile_features(jazz-console-fridge PUBLIC cxx_std_20)

add_library(jazz-console-woodchipper STATIC ../woodchipper/woodchipper.cpp)
target_compile_definitions(jazz-console-woodchipper PRIVATE UNIT_TEST)

add_executable(jazz-console console.cpp)
target_link_libraries(jazz-console PRIVATE jazz::libjazz jazz::granular jazz-console-fridge
  jazz-console-woodchipper)
target_compile_features(jazz-console PRIVATE cxx_std_20)
