find_package (Threads)

if (WITH_PYTHON)
  find_package (PythonLibs REQUIRED)
endif (WITH_PYTHON)

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
add_custom_command(OUTPUT ${TWITTER_BREAKOUTDETECTION_INCLUDE_DIR}/src/edm-per.cpp
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
if (WITH_PYTHON)
  include_directories (SYSTEM ${PYTHON_INCLUDE_PATH})
endif (WITH_PYTHON)

# object library for libpilot
set (PILOT_LIBSRC libpilot.cc workload.cc
             ${TWITTER_BREAKOUTDETECTION_INCLUDE_DIR}/src/edm-per.cpp
             ${TWITTER_BREAKOUTDETECTION_INCLUDE_DIR}/src/helper.cpp)

# Everytime we call add_library with .cc files they are compiled once. We call
# add_library twice, once for the shared lib here once for the static lib
# below. Object files need to be compiled twice. With PIC for shared lib,
# without PIC for static lib.
#
# python_binding.cc is only needed for the shared library.
if (WITH_PYTHON)
  add_library (pilot_sharedlib SHARED ${PILOT_LIBSRC} python_binding.cc)
else (WITH_PYTHON)
  add_library (pilot_sharedlib SHARED ${PILOT_LIBSRC})
endif (WITH_PYTHON)
set_target_properties(pilot_sharedlib PROPERTIES OUTPUT_NAME pilot)
set_target_properties(pilot_sharedlib PROPERTIES SOVERSION ${PILOT_VERSION_MAJOR})
set_target_properties(pilot_sharedlib PROPERTIES VERSION
                      ${PILOT_VERSION_MAJOR}.${PILOT_VERSION_MINOR})
set_target_properties(pilot_sharedlib PROPERTIES MACOSX_RPATH true)
target_link_libraries (pilot_sharedlib ${CDK_STATIC_LIBRARIES} ${Boost_LIBRARIES}
             ${CMAKE_THREAD_LIBS_INIT} ${CDK_LIBRARIES})
if (WITH_PYTHON)
  target_link_libraries (pilot_sharedlib ${PYTHON_LIBRARIES})
  add_custom_target(create_python_module_symlink ALL
                    COMMAND ${CMAKE_COMMAND} -E create_symlink
                    $<TARGET_FILE_NAME:pilot_sharedlib> $<TARGET_FILE_DIR:pilot_sharedlib>/pilot_bench.so)
endif (WITH_PYTHON)

add_dependencies (pilot_sharedlib cdk twitter_breakoutdetection)

# static library
add_library (pilot_staticlib STATIC ${PILOT_LIBSRC})
set_target_properties(pilot_staticlib PROPERTIES OUTPUT_NAME pilot)
add_dependencies (pilot_staticlib cdk twitter_breakoutdetection)

# installation
install (TARGETS pilot_sharedlib pilot_staticlib
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib
         RUNTIME DESTINATION bin)
file(GLOB HEADER_FILES "interface_include/pilot/*.h*")
file(GLOB COMMON_HEADER_FILES "${CMAKE_SOURCE_DIR}/include/*.h")
install (FILES ${HEADER_FILES} DESTINATION include/pilot)
install (FILES ${COMMON_HEADER_FILES} DESTINATION include/pilot)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/../config.h DESTINATION include/pilot)
if (WITH_PYTHON)
  install (FILES $<TARGET_FILE_DIR:pilot_sharedlib>/pilot_bench.so
           DESTINATION lib)
endif(WITH_PYTHON)

# needed by boost 1.59+
add_definitions ("-std=gnu++11")
# we should use target_compile_features() when everyone upgrades to cmake 3.1+

# tests
set (PILOT_TESTS_LIBRARIES pilot_sharedlib ${Boost_LIBRARIES} ${GTEST_BINARY_DIR}/libgtest.a ${GTEST_BINARY_DIR}/libgtest_main.a ${CMAKE_THREAD_LIBS_INIT} ${CDK_LIBRARIES})

add_executable (func_test_seq_write test/func_test_seq_write.cc)
target_link_libraries (func_test_seq_write ${PILOT_TESTS_LIBRARIES})
install (FILES test/func_test_seq_write.cc DESTINATION share/pilot/examples/using_api)
install (TARGETS func_test_seq_write DESTINATION share/pilot/examples/using_api)

add_executable (unit_test_misc test/unit_test_misc.cc)
target_link_libraries (unit_test_misc ${PILOT_TESTS_LIBRARIES})

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

add_executable (unit_test_wps test/unit_test_wps.cc)
target_link_libraries (unit_test_wps ${PILOT_TESTS_LIBRARIES})

add_executable (unit_test_macros test/unit_test_macros.cc)
target_link_libraries (unit_test_macros ${PILOT_TESTS_LIBRARIES})


add_test (NAME unit_test_misc
		  COMMAND unit_test_misc)
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
add_test (NAME unit_test_wps
          COMMAND unit_test_wps)
if (WITH_PYTHON)
  add_test (NAME unit_test_python_statistics
            COMMAND bash -c "PYTHONPATH=$<TARGET_FILE_DIR:pilot_sharedlib> python ${CMAKE_CURRENT_LIST_DIR}/test/unit_test_python_statistics.py")
endif(WITH_PYTHON)
# Disabled this test because it is too long.
# add_test (NAME unit_test_macros
#		  COMMAND unit_test_macros)
add_test (NAME func_test_seq_write
		  COMMAND func_test_seq_write -o /tmp/test_output_file --no-tui -d 240)

