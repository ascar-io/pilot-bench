set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package (Threads REQUIRED)

if (WITH_LUA)
find_package (Readline REQUIRED)
find_package (Lua REQUIRED)

###############################################################################
## luaprompt LIBRARY

ExternalProject_Add (luaprompt
  GIT_REPOSITORY https://github.com/dpapavas/luaprompt.git
  GIT_TAG ef260d2

  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/luaprompt
  PATCH_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(luaprompt SOURCE_DIR)
set (LUAPROMPT_INCLUDE_DIR ${SOURCE_DIR})
set (LUAPROMPT_SOURCE_DIR ${SOURCE_DIR})
# let the luaprompt source files depend on the library so CMake will fetch the
# externalproject before building
add_custom_command(OUTPUT ${LUAPROMPT_INCLUDE_DIR}/prompt.h
                   DEPENDS luaprompt COMMAND "")
add_custom_command(OUTPUT ${LUAPROMPT_SOURCE_DIR}/prompt.c
                   DEPENDS luaprompt COMMAND "")
# we compile luaprompt to a standalone library because it needs to be compiled
# with customization macros
add_library(libluaprompt ${LUAPROMPT_SOURCE_DIR}/prompt.c)
set_target_properties (libluaprompt PROPERTIES COMPILE_FLAGS
                       "-DHAVE_LIBREADLINE -DHAVE_READLINE_HISTORY -DHAVE_IOCTL -D_GNU_SOURCE")
include_directories (${LUA_INCLUDE_DIR})
include_directories (${LUAPROMPT_INCLUDE_DIR})
endif (WITH_LUA)

include_directories (SYSTEM ${GTEST_INCLUDE_DIR})
include_directories (${CDK_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/lib/interface_include)

add_executable (bench pilot.cc run_program.cc detect_changepoint_edm.cc analyze.cc)
if (WITH_LUA)
  add_dependencies (bench cdk luaprompt)
endif (WITH_LUA)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

target_link_libraries (bench pilot_staticlib ${CDK_STATIC_LIBRARIES}
                       ${CDK_LIBRARIES} ${Boost_LIBRARIES}
                       ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_THREAD_LIBS_INIT})
if (WITH_LUA)
  target_link_libraries (bench libluaprompt ${LUA_LIBRARY} ${READLINE_LIBRARY})
endif (WITH_LUA)

# "pilot" is an optional alias for the "bench" command
add_custom_command(TARGET bench POST_BUILD
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMAND if [ ! -e pilot ]\; then ln -s bench pilot\; fi)

# tests
add_custom_target(copy_test_files ALL
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_lua_prompt.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_pilot_opt_parsing.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_analyze.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_analyze_input.csv $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_analyze_input_2col.csv $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_analyze_input_3col.csv $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_analyze_input_3col_with_header.csv $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_analyze_input_3col_with_malformed_header.csv $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_benchmark.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_benchmark_binomial_proportion.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_benchmark_long_lines.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/mock_benchmark.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/mock_benchmark_binomial_proportion.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/mock_benchmark_long_lines.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/mock_benchmark_with_work_amount.sh $<TARGET_FILE_DIR:bench>
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_pilot_opt_parsing_expected_work_amount.log $<TARGET_FILE_DIR:bench>)

# tests
set (PILOT_TESTS_LIBRARIES pilot_sharedlib ${Boost_LIBRARIES} ${GTEST_BINARY_DIR}/libgtest.a ${GTEST_BINARY_DIR}/libgtest_main.a ${CMAKE_THREAD_LIBS_INIT} ${CDK_LIBRARIES})

add_executable (unit_test_cli test/unit_test_cli.cc run_program.cc)
target_link_libraries (unit_test_cli ${PILOT_TESTS_LIBRARIES})

add_test (NAME unit_test_analyze
		  COMMAND unit_test_analyze.sh)
add_test (NAME unit_test_benchmark
          COMMAND unit_test_benchmark.sh)
add_test (NAME unit_test_benchmark_binomial_proportion
        COMMAND unit_test_benchmark_binomial_proportion.sh)
add_test (NAME unit_test_cli
		  COMMAND unit_test_cli)
if (WITH_LUA)
  add_test (NAME unit_test_lua_prompt
            COMMAND unit_test_lua_prompt.sh)
endif (WITH_LUA)
add_test (NAME unit_test_pilot_opt_parsing
          COMMAND unit_test_pilot_opt_parsing.sh)

install (TARGETS bench DESTINATION bin)
install (PROGRAMS $<TARGET_FILE_DIR:bench>/pilot DESTINATION bin)
