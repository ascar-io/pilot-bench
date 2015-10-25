enable_testing ()

# add the binary tree directory to the search path 
# for include files
include_directories (${CMAKE_CURRENT_BINARY_DIR})

find_package (Boost COMPONENTS program_options REQUIRED)
if (Boost_FOUND)
    add_definitions ("-DHAS_BOOST")
    include_directories (${Boost_INCLUDE_DIRS})
    link_directories (${Boost_LIBRARY_DIRS})
endif ()

# the main library
add_library (libpilot libpilot.cc)

# tests
add_executable(func_test_seq_write test/func_test_seq_write.cc)
target_link_libraries(func_test_seq_write ${Boost_LIBRARIES})
 
install (DIRECTORY include DESTINATION include/pilot)
