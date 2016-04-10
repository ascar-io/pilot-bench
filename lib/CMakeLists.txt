find_package (Threads)

###############################################################################
## Twitter BreakoutDetection LIBRARY

ExternalProject_Add (twitter_breakoutdetection
  GIT_REPOSITORY https://github.com/twitter/BreakoutDetection.git
  GIT_TAG 7ae3fc4

  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/twitter_breakoutdetection
  PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_LIST_DIR}/twitter_breakoutdetection_pilot.patch
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(twitter_breakoutdetection SOURCE_DIR)
set (TWITTER_BREAKOUTDETECTION_INCLUDE_DIR ${SOURCE_DIR})
# let the twitter_bd source files depend on the library so CMake will fetch the
# externalproject before building
add_custom_command(OUTPUT ${TWITTER_BREAKOUTDETECTION_INCLUDE_DIR}/src/edm-multi.cpp
                   DEPENDS twitter_breakoutdetection COMMAND "")
add_custom_command(OUTPUT ${TWITTER_BREAKOUTDETECTION_INCLUDE_DIR}/src/helper.cpp
                   DEPENDS twitter_breakoutdetection COMMAND "")


# add the binary tree directory to the search path 
# for include files
include_directories (SYSTEM ${CMAKE_CURRENT_BINARY_DIR}/../gtest/src/gtest/include)
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (interface_include)
include_directories (priv_include)
include_directories (SYSTEM ${CDK_INCLUDE_DIR})

# the main library
add_library (libpilot libpilot.cc workload.cc
             ${TWITTER_BREAKOUTDETECTION_INCLUDE_DIR}/src/edm-multi.cpp
             ${TWITTER_BREAKOUTDETECTION_INCLUDE_DIR}/src/helper.cpp)
add_dependencies (libpilot cdk twitter_breakoutdetection)

# needed by boost 1.59+
add_definitions ("-std=gnu++11")
# we should use target_compile_features() when everyone upgrades to cmake 3.1+

# tests
set (PILOT_TESTS_LIBRARIES libpilot ${Boost_LIBRARIES} ${GTEST_BINARY_DIR}/libgtest.a ${GTEST_BINARY_DIR}/libgtest_main.a ${CMAKE_THREAD_LIBS_INIT} ${CDK_LIBRARIES})

add_executable (func_test_seq_write test/func_test_seq_write.cc)
target_link_libraries (func_test_seq_write ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_run_workload test/unit_test_run_workload.cc)
target_link_libraries (unit_test_run_workload ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_statistics test/unit_test_statistics.cc)
target_link_libraries (unit_test_statistics ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_session_duration test/unit_test_session_duration.cc)
target_link_libraries (unit_test_session_duration ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_unit_readings_iter test/unit_test_unit_readings_iter.cc)
target_link_libraries (unit_test_unit_readings_iter ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_readings_warmup_removal test/unit_test_readings_warmup_removal.cc)
target_link_libraries (unit_test_readings_warmup_removal ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_compare_results test/unit_test_compare_results.cc)
target_link_libraries (unit_test_compare_results ${PILOT_TESTS_LIBRARIES})


add_test (NAME unit_test_statistics
		  COMMAND unit_test_statistics)
add_test (NAME unit_test_run_workload
		  COMMAND unit_test_run_workload)
add_test (NAME unit_test_run_workload_check_export
		  COMMAND diff /tmp/unit_test_run_workload_export/unit_readings.csv ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_run_workload_export_expected.csv)
add_test (NAME unit_test_session_duration
		  COMMAND unit_test_session_duration)
add_test (NAME unit_test_unit_readings_iter
		  COMMAND unit_test_unit_readings_iter)
add_test (NAME unit_test_readings_warmup_removal
          COMMAND unit_test_readings_warmup_removal)
add_test (NAME unit_test_compare_results
          COMMAND unit_test_compare_results ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_compare_results_input.csv)
add_test (NAME func_test_seq_write
		  COMMAND func_test_seq_write -o /tmp/test_output_file --no-tui)

install (DIRECTORY interface_include DESTINATION include/pilot)
