cmake_minimum_required (VERSION 2.6)
project (pilot)
enable_testing ()
set (PILOT_VERSION_MAJOR 0)
set (PILOT_VERSION_MINOR 1)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/include/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories("${PROJECT_BINARY_DIR}")

# libpilot
include_directories ("${PROJECT_SOURCE_DIR}/lib/include")
add_subdirectory (lib)

add_test (NAME func_test_seq_write
		  COMMAND func_test_seq_write /tmp/test_output_file)

# pilot command line tool
add_subdirectory (cli)

