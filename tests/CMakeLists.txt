cmake_minimum_required(VERSION 3.16)
project(jazz-test)

find_package(Threads REQUIRED)
find_package(rapidcheck REQUIRED)

set(CMAKE_CXX_STANDARD 20)
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

# ---------------------------------------------------------------------------
# Fuzztest: built as an isolated ExternalProject so its targets never appear
# in our cmake build graph (no "Built target absl_base" noise, no warnings).
# ---------------------------------------------------------------------------
include(ExternalProject)

set(FUZZTEST_BUNDLE_DIR ${CMAKE_BINARY_DIR}/fuzztest-bundle)
set(FUZZTEST_BUNDLE_LIB ${FUZZTEST_BUNDLE_DIR}/libfuzztest_all.a)

if(FUZZTEST_FUZZING_MODE)
  set(_ft_cxx_flags "-w -fsanitize=address,fuzzer-no-link")
  set(_ft_link_flags "-fsanitize=address")
else()
  set(_ft_cxx_flags "-w")
  set(_ft_link_flags "")
endif()

ExternalProject_Add(fuzztest_bundle
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/fuzztest
  BINARY_DIR ${FUZZTEST_BUNDLE_DIR}
  CMAKE_ARGS
    -DCMAKE_CXX_STANDARD=20
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DFUZZTEST_FUZZING_MODE=${FUZZTEST_FUZZING_MODE}
    -DANTLR_BUILD_CPP_TESTS=OFF
    "-DCMAKE_CXX_FLAGS=${_ft_cxx_flags}"
    "-DCMAKE_C_FLAGS=-w"
    "-DCMAKE_EXE_LINKER_FLAGS=${_ft_link_flags}"
    "-DCMAKE_SHARED_LINKER_FLAGS=${_ft_link_flags}"
  INSTALL_COMMAND
    ${CMAKE_COMMAND}
      -DBINARY_DIR=${FUZZTEST_BUNDLE_DIR}
      -DOUTPUT=${FUZZTEST_BUNDLE_LIB}
      -P ${CMAKE_SOURCE_DIR}/cmake/merge_archives.cmake
  BUILD_BYPRODUCTS ${FUZZTEST_BUNDLE_LIB}
)

# Create placeholder dirs so cmake accepts them as include paths before the
# ExternalProject has run for the first time and populated them.
file(MAKE_DIRECTORY ${FUZZTEST_BUNDLE_DIR}/_deps/abseil-cpp-src)
file(MAKE_DIRECTORY ${FUZZTEST_BUNDLE_DIR}/_deps/re2-src)

add_library(fuzztest_all STATIC IMPORTED GLOBAL)
set_target_properties(fuzztest_all PROPERTIES
  IMPORTED_LOCATION ${FUZZTEST_BUNDLE_LIB}
  INTERFACE_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/vendor/fuzztest;${FUZZTEST_BUNDLE_DIR}/_deps/abseil-cpp-src;${FUZZTEST_BUNDLE_DIR}/_deps/re2-src"
)
add_dependencies(fuzztest_all fuzztest_bundle)

function(link_fuzztest name)
  target_link_libraries(${name} PRIVATE fuzztest_all)
endfunction()

# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

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
