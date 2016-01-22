find_package (Threads)
# add the binary tree directory to the search path 
# for include files
include_directories (${CMAKE_CURRENT_BINARY_DIR}/../gtest/src/gtest/include)
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (interface_include)
include_directories (priv_include)
include_directories (${CDK_INCLUDE_DIR})
include_directories (${DLIB_INCLUDE_DIR})

# the main library
add_library (libpilot libpilot.cc workload.cc)
add_dependencies (libpilot cdk)
# used by boost 1.59+
target_compile_features (libpilot PRIVATE cxx_nonstatic_member_init
                                          cxx_generalized_initializers
                                          cxx_auto_type)

# tests
set (PILOT_TESTS_LIBRARIES libpilot ${Boost_LIBRARIES} ${GTEST_BINARY_DIR}/libgtest.a ${GTEST_BINARY_DIR}/libgtest_main.a ${CMAKE_THREAD_LIBS_INIT} ${CDK_LIBRARIES})

add_executable (func_test_seq_write test/func_test_seq_write.cc)
target_compile_features (func_test_seq_write PRIVATE cxx_generalized_initializers)
target_link_libraries (func_test_seq_write ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_run_workload test/unit_test_run_workload.cc)
target_compile_features (unit_test_run_workload PRIVATE cxx_generalized_initializers)
target_link_libraries (unit_test_run_workload ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_statistics test/unit_test_statistics.cc)
target_compile_features (unit_test_statistics PRIVATE cxx_generalized_initializers)
target_link_libraries (unit_test_statistics ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_session_duration test/unit_test_session_duration.cc)
target_compile_features (unit_test_session_duration PRIVATE cxx_generalized_initializers)
target_link_libraries (unit_test_session_duration ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_unit_readings_iter test/unit_test_unit_readings_iter.cc)
target_compile_features (unit_test_unit_readings_iter PRIVATE cxx_generalized_initializers)
target_link_libraries (unit_test_unit_readings_iter ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_readings_warmup_removal test/unit_test_readings_warmup_removal.cc)
target_compile_features (unit_test_readings_warmup_removal PRIVATE cxx_generalized_initializers cxx_lambdas)
target_link_libraries (unit_test_readings_warmup_removal ${PILOT_TESTS_LIBRARIES})



add_test (NAME unit_test_statistics
		  COMMAND unit_test_statistics)
add_test (NAME unit_test_run_workload
		  COMMAND unit_test_run_workload)
add_test (NAME unit_test_run_workload_check_export
		  COMMAND diff unit_test_run_workload_export.csv ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_run_workload_export_expected.csv)
add_test (NAME unit_test_session_duration
		  COMMAND unit_test_session_duration)
add_test (NAME unit_test_unit_readings_iter
		  COMMAND unit_test_unit_readings_iter)
add_test (NAME unit_test_readings_warmup_removal
          COMMAND unit_test_readings_warmup_removal)
add_test (NAME func_test_seq_write
		  COMMAND func_test_seq_write -o /tmp/test_output_file --no-tui)

install (DIRECTORY interface_include DESTINATION include/pilot)
