find_package (Threads)
# add the binary tree directory to the search path 
# for include files
include_directories (${CMAKE_CURRENT_BINARY_DIR}/../gtest/src/gtest/include)
include_directories (interface_include)
include_directories (priv_include)

find_package (Boost COMPONENTS log program_options system timer REQUIRED)
if (Boost_FOUND)
    add_definitions ("-DHAS_BOOST")
    include_directories (${Boost_INCLUDE_DIRS})
    link_directories (${Boost_LIBRARY_DIRS})
endif ()

# the main library
add_library (libpilot libpilot.cc)
# used by boost 1.59+
target_compile_features(libpilot PUBLIC cxx_nonstatic_member_init)

# tests
set (PILOT_TEST_LIBRARIES libpilot ${Boost_LIBRARIES} ${binary_dir}/libgtest.a ${binary_dir}/libgtest_main.a ${CMAKE_THREAD_LIBS_INIT})
add_executable (func_test_seq_write test/func_test_seq_write.cc)
target_link_libraries (func_test_seq_write ${PILOT_TEST_LIBRARIES})
add_executable (unit_test_statistics test/unit_test_statistics.cc)
target_link_libraries (unit_test_statistics ${PILOT_TEST_LIBRARIES})

add_test (NAME unit_test_statistics
		  COMMAND unit_test_statistics)
#add_test (NAME func_test_seq_write
#		  COMMAND func_test_seq_write /tmp/test_output_file)

install (DIRECTORY interface_include DESTINATION include/pilot)
