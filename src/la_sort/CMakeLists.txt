jazz_add_firmware(la_sort SOURCES la_sort.cpp panel.cpp firmware.cpp)

if(CMAKE_CROSSCOMPILING)
  # Reuse the Fridge panel drivers without linking its audio engine or UI.
  target_sources(la_sort PRIVATE
    ../fridge/io.cpp
    ../fridge/led.cpp
    ../fridge/rgb_led.cpp
    ../fridge/value_display.cpp
  )
endif()
