if(CMAKE_CROSSCOMPILING)
  jazz_add_firmware(fridge-benchmark SOURCES
    fridge/seed.cpp
    ../src/fridge/lfo.cpp
    ../src/fridge/mod.cpp
    ../src/fridge/config.cpp
    ../src/fridge/regions.cpp
    ../src/fridge/sound.cpp
  )
  target_include_directories(fridge-benchmark PRIVATE ../src/fridge)
else()
  add_executable(fridge-benchmark fridge/host.cpp)
  target_link_libraries(fridge-benchmark PRIVATE jazz::fridge)
  target_compile_features(fridge-benchmark PRIVATE cxx_std_20)
  add_test(NAME fridge_throughput COMMAND fridge-benchmark 8)
  set_tests_properties(fridge_throughput PROPERTIES LABELS "performance" TIMEOUT 120)
endif()
