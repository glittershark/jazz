# Merge all .a files under BINARY_DIR into a single combined archive at OUTPUT.
# Excludes googletest (we link system gtest separately).
#
# Usage: cmake -DBINARY_DIR=<dir> -DOUTPUT=<path/to/out.a> -P merge_archives.cmake

file(GLOB_RECURSE ARCHIVES "${BINARY_DIR}/*.a")
list(FILTER ARCHIVES EXCLUDE REGEX "/googlemock|/googletest")

if(NOT ARCHIVES)
  message(FATAL_ERROR "No .a files found under ${BINARY_DIR}")
endif()

set(MRI "${BINARY_DIR}/merge.mri")
file(WRITE  "${MRI}" "CREATE ${OUTPUT}\n")
foreach(ARCHIVE ${ARCHIVES})
  file(APPEND "${MRI}" "ADDLIB ${ARCHIVE}\n")
endforeach()
file(APPEND "${MRI}" "SAVE\nEND\n")

execute_process(
  COMMAND ar -M
  INPUT_FILE "${MRI}"
  RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "ar failed to bundle fuzztest archives into ${OUTPUT}")
endif()
