find_package (Threads)
# add the binary tree directory to the search path 
# for include files
include_directories (${CMAKE_CURRENT_BINARY_DIR}/../gtest/src/gtest/include)
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (interface_include)
include_directories (priv_include)

# default is off (using dynamic libraries)
#set (Boost_USE_STATIC_LIBS        ON)
#set (Boost_USE_STATIC_RUNTIME     ON)

set (Boost_USE_MULTITHREADED      ON)
find_package (Boost COMPONENTS log program_options system timer chrono thread REQUIRED)
if (Boost_FOUND)
    add_definitions ("-DHAS_BOOST")
    if (NOT Boost_USE_STATIC_LIBS)
		add_definitions ("-DBOOST_ALL_DYN_LINK")
    endif ()
    include_directories (${Boost_INCLUDE_DIRS})
    link_directories (${Boost_LIBRARY_DIRS})
endif ()

# the main library
add_library (libpilot libpilot.cc workload.cc)
# used by boost 1.59+
target_compile_features(libpilot PUBLIC cxx_nonstatic_member_init)

# tests
set (PILOT_TESTS_LIBRARIES libpilot ${Boost_LIBRARIES} ${binary_dir}/libgtest.a ${binary_dir}/libgtest_main.a ${CMAKE_THREAD_LIBS_INIT})
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
add_test (NAME func_test_seq_write
		  COMMAND func_test_seq_write -o /tmp/test_output_file)

install (DIRECTORY interface_include DESTINATION include/pilot)
