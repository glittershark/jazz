cmake_minimum_required(VERSION 3.16)
project(jazz-test)

find_package(Threads REQUIRED)
find_package(rapidcheck REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "" FORCE)
# Save flags before fuzztest's setup_fuzzing_flags() propagates ASAN into our scope
set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
set(_saved_c_flags "${CMAKE_C_FLAGS}")
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/fuzztest fuzztest EXCLUDE_FROM_ALL)
# Restore: ASAN flags should only apply to fuzz test targets, not all gtests
set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")
set(CMAKE_C_FLAGS "${_saved_c_flags}")

enable_testing()

set(JAZZ_TEST_LIBS
  jazz::libjazz
  jazz::delay
  jazz::reverse
  jazz::granular
  jazz::rachel
  jazz::fridge
  Threads::Threads
  rapidcheck
)

set(JAZZ_TEST_PROPS
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED YES
  C_STANDARD 11
  C_STANDARD_REQUIRED YES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

function(add_gtest source_file)
  cmake_path(GET source_file STEM test_name)
  add_executable(${test_name} ${source_file})
  target_link_libraries(${test_name} PUBLIC ${JAZZ_TEST_LIBS} gtest gtest_main)
  if(FUZZTEST_FUZZING_MODE)
    target_link_options(${test_name} PRIVATE -fsanitize=address)
  endif()
  set_target_properties(${test_name} PROPERTIES ${JAZZ_TEST_PROPS})
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

function(add_fuzz_test source_file)
  cmake_path(GET source_file STEM test_name)
  add_executable(${test_name} ${source_file})
  target_link_libraries(${test_name} PUBLIC ${JAZZ_TEST_LIBS} gtest)
  link_fuzztest(${test_name})
  if(FUZZTEST_FUZZING_MODE)
    target_compile_options(${test_name} PRIVATE -fsanitize=address,fuzzer-no-link)
    target_link_options(${test_name} PRIVATE -fsanitize=address)
  endif()
  set_target_properties(${test_name} PROPERTIES ${JAZZ_TEST_PROPS})
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

function(autogen_tests test_folder)
  file(GLOB_RECURSE gtests ${test_folder}/*_gtest.cpp)
  foreach(spec IN LISTS gtests)
    add_gtest(${spec})
  endforeach()

  file(GLOB_RECURSE fuzz_tests ${test_folder}/*_fuzz_test.cpp)
  foreach(spec IN LISTS fuzz_tests)
    add_fuzz_test(${spec})
  endforeach()
endfunction()

autogen_tests(${CMAKE_CURRENT_LIST_DIR})
