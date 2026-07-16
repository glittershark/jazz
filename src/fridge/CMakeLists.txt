jazz_add_firmware(fridge SOURCES
  fridge.cpp
  pwm.cpp
  io.cpp
  mod.cpp
  sound.cpp
  ui.cpp
  value_display.cpp
  led.cpp
  rgb_led.cpp
  engine.cpp
)

if(NOT CMAKE_CROSSCOMPILING AND TARGET fridge)
  # color.hpp pulls in rapidcheck generators for the host/test build
  find_package(rapidcheck REQUIRED)
  target_link_libraries(fridge PUBLIC rapidcheck)

  add_executable(fridge-lfo-cli lfo_cli.cpp)
  target_link_libraries(fridge-lfo-cli PRIVATE jazz::fridge)
  target_compile_features(fridge-lfo-cli PRIVATE cxx_std_20)
endif()
