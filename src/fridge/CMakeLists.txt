jazz_add_firmware(fridge SOURCES
  fridge.cpp
  pwm.cpp
  color.cpp
  io.cpp
  lfo.cpp
  head_transition.cpp
  config_transform.cpp
  sound.cpp
  ui.cpp
  value_display.cpp
  led.cpp
  rgb_led.cpp
  engine.cpp
)

if(NOT CMAKE_CROSSCOMPILING AND TARGET fridge)
  add_executable(fridge-lfo-cli lfo_cli.cpp)
  target_link_libraries(fridge-lfo-cli PRIVATE jazz::fridge)
  target_compile_features(fridge-lfo-cli PRIVATE cxx_std_20)
endif()
