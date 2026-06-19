add_library(jazz-console-fridge STATIC
  ../fridge/lfo.cpp
  ../fridge/head_transition.cpp
  ../fridge/config_transform.cpp
  ../fridge/sound.cpp
)
target_link_libraries(jazz-console-fridge PUBLIC jazz::libjazz)
target_include_directories(jazz-console-fridge PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/fridge>
)
target_compile_features(jazz-console-fridge PUBLIC cxx_std_20)

add_executable(jazz-console console.cpp)
target_link_libraries(jazz-console PRIVATE jazz::libjazz jazz::granular jazz::touch jazz-console-fridge)
target_compile_definitions(jazz-console PRIVATE UNIT_TEST)
target_compile_features(jazz-console PRIVATE cxx_std_20)
