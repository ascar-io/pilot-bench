set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

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
                       "-DHAVE_LIBREADLINE -DHAVE_READLINE_HISTORY -DHAVE_IOCTL")

set (LIBPILOT_LIBRARIES libpilot)

include_directories (${CDK_INCLUDE_DIR})
include_directories (${LUA_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/lib/interface_include)
include_directories (${LUAPROMPT_INCLUDE_DIR})

add_executable (pilot pilot.cc run_program.cc)
add_dependencies (pilot cdk luaprompt)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

target_link_libraries (pilot ${CDK_LIBRARIES} ${LIBPILOT_LIBRARIES}
                       ${Boost_LIBRARIES} ${READLINE_LIBRARY} libluaprompt
                       ${LUA_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

# tests
add_custom_command(TARGET pilot POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_lua_prompt.sh $<TARGET_FILE_DIR:pilot>)
add_custom_command(TARGET pilot POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_pilot_opt_parsing.sh $<TARGET_FILE_DIR:pilot>)
add_custom_command(TARGET pilot POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       ${CMAKE_CURRENT_LIST_DIR}/test/mock_benchmark.sh $<TARGET_FILE_DIR:pilot>)

add_test (NAME unit_test_lua_prompt
          COMMAND unit_test_lua_prompt.sh)
add_test (NAME unit_test_pilot_opt_parsing
          COMMAND unit_test_pilot_opt_parsing.sh)

install (TARGETS pilot DESTINATION bin)
