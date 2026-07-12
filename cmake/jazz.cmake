# Helper to add a firmware project that builds for both ARM and host
function(jazz_add_firmware name)
  cmake_parse_arguments(ARG "ARM_ONLY" "" "SOURCES" ${ARGN})

  if(CMAKE_CROSSCOMPILING)
    # ARM: build executable
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE jazz::libjazz daisy DaisySP)
    # Link with the Daisy linker script for correct memory layout
    set(LDSCRIPT "${CMAKE_SOURCE_DIR}/vendor/libDaisy/core/STM32H750IB_flash.lds")
    target_link_options(${name} PRIVATE "-T${LDSCRIPT}")
    # Generate .bin for flashing
    add_custom_command(TARGET ${name} POST_BUILD
      COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> ${name}.bin
      WORKING_DIRECTORY $<TARGET_FILE_DIR:${name}>
    )
  elseif(NOT ARG_ARM_ONLY)
    # Host: build static library for tests
    add_library(${name} STATIC ${ARG_SOURCES})
    target_link_libraries(${name} PUBLIC jazz::libjazz)
    target_compile_definitions(${name} PRIVATE UNIT_TEST)
    target_include_directories(${name} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )
    add_library(jazz::${name} ALIAS ${name})
  endif()

  if(TARGET ${name})
    target_compile_features(${name} PUBLIC cxx_std_20)
  endif()
endfunction()
