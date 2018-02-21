cmake_minimum_required (VERSION 2.6)
project (pilot)

###############################################################################
## VERSION

set (PILOT_VERSION_MAJOR 0)
set (PILOT_VERSION_MINOR 12)

###############################################################################
## OPTIONS
option (WITH_LUA    "Build with Lua support" OFF)
option (WITH_PYTHON "Build Python binding"   OFF)
option (ENABLE_ASAN_DEBUG "Enable AddressSanitizer and UBSan for Debug Build" ON)
option (ENABLE_TSAN_DEBUG "Enable ThreadSanitizer for Debug Build, can't be used with ENABLE_ASAN_DEBUG" OFF)

###############################################################################
# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/include/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

# add the binary tree to the search path for include files
# so that we will find config.h
include_directories("${PROJECT_BINARY_DIR}")


###############################################################################
## CONFIG BUILD TYPE

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif (NOT CMAKE_BUILD_TYPE)

# Apply visibility settings to all targets including .a files. This sets
# policy CMP0063 to NEW.
cmake_policy (VERSION 3.5.2)
# Don't expose useless symbols. This is also needed if Boost is compiled with
# -fvisibility=hidden.
set (CMAKE_CXX_VISIBILITY_PRESET hidden)
set (CMAKE_C_VISIBILITY_PRESET hidden)

# Boost uses -O3 by default. We use -O3 here too. Commenting them out
# would give you back CMake's default -O2. 
string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

if (ENABLE_ASAN_DEBUG)
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
  set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
  # Up till Apple clang-703.0.29 it doesn't support -fsanitize=undefined
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.8.999")
    # Boost triggers vptr false alarms in UBSan so vptr has to be disabled.
    # See https://github.com/scylladb/seastar/issues/78
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined -fno-sanitize=vptr")
    set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=undefined -fno-sanitize=vptr")
  endif (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.8.999")
endif (ENABLE_ASAN_DEBUG)

if (ENABLE_TSAN_DEBUG)
  if (ENABLE_ASAN_DEBUG)
    message( FATAL_ERROR "ASAN and TSAN cannot be enabled together." )
  endif (ENABLE_ASAN_DEBUG)
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
  set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=thread")
endif (ENABLE_TSAN_DEBUG)

###############################################################################
## GLOBAL BUILD OPTIONS
if (CMAKE_CXX_COMPILER_ID MATCHES "(Clang|GNU)")
    # The last -Werror=sign-compare is needed to enable the sign-compare
    # warning for clang
    add_definitions (-Wall -Werror -Werror=sign-compare)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "(MSVC)")
    add_definitions (/W2)
endif ()


###############################################################################
## TEST RELATED

find_program( MEMORYCHECK_COMMAND valgrind )
set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full --track-origins=yes" )
include( CTest )

include (ExternalProject)
ExternalProject_Add (gtest
  URL https://github.com/google/googletest/archive/release-1.8.0.zip
  URL_HASH SHA256=f3ed3b58511efd272eb074a3a6d6fb79d7c2e6a0e374323d1e6bcbcc1ef141bf
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(gtest SOURCE_DIR BINARY_DIR)
set (GTEST_INCLUDE_DIR ${SOURCE_DIR}/googletest/include)
set (GTEST_BINARY_DIR ${BINARY_DIR}/googlemock/gtest)


###############################################################################
## TUI LIBRARIES

# We want to link to ncurses statically on Mac and Windows
if (WIN32)
   set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
elseif (APPLE)
   set (CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif (WIN32)

find_package (Curses REQUIRED)
ExternalProject_Add (cdk
  URL http://invisible-mirror.net/archives/cdk/cdk-5.0-20150928.tgz
  URL_HASH SHA256=028da75d5f777a1c4184f88e34918bd273bd83bbe3c959bc11710c4f0ea2e448
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/cdk
  SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/cdk/src/cdk
  CONFIGURE_COMMAND "${CMAKE_SOURCE_DIR}/cmake/configure_fpic.sh" "${CMAKE_CURRENT_BINARY_DIR}/cdk/src/cdk/configure"
  BUILD_COMMAND "make"
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(cdk SOURCE_DIR BINARY_DIR)
# BINARY_DIR/include has CDK's cdk_config.h
set (CDK_INCLUDE_DIR "${SOURCE_DIR}/include" "${BINARY_DIR}/include")
set (CDK_STATIC_LIBRARIES "${BINARY_DIR}/libcdk.a")
set (CDK_LIBRARIES ${CURSES_LIBRARIES})


###############################################################################
## BOOST LIBRARIES

# default is off (using dynamic libraries)
if (BOOST_USE_STATIC)
    set (Boost_USE_STATIC_LIBS        ON)
    set (Boost_USE_STATIC_RUNTIME     ON)
endif ()

set (Boost_USE_MULTITHREADED      ON)
set (BOOST_COMPONENTS_NEEDED filesystem log program_options system timer chrono)
if (WITH_PYTHON)
  if (NOT BOOST_PYTHON_MODULE)
    set (BOOST_PYTHON_MODULE python)
  endif (NOT BOOST_PYTHON_MODULE)
  set (BOOST_COMPONENTS_NEEDED ${BOOST_COMPONENTS_NEEDED} ${BOOST_PYTHON_MODULE})
endif (WITH_PYTHON)

find_package (Boost COMPONENTS ${BOOST_COMPONENTS_NEEDED} thread REQUIRED)
if (Boost_FOUND)
    add_definitions ("-DHAS_BOOST")
    if (NOT Boost_USE_STATIC_LIBS)
		add_definitions ("-DBOOST_ALL_DYN_LINK")
    endif ()
    # Make Boost headers SYSTEM so compiler ignores warnings in them
    include_directories (SYSTEM ${Boost_INCLUDE_DIRS})
    link_directories (${Boost_LIBRARY_DIRS})
endif ()


# libpilot
add_subdirectory (lib)
# pilot command line tool
add_subdirectory (cli)
# documentation
add_subdirectory (doc)


# This CTestCustom file is used to exclude tests, such as unit_test_python_statistics,
# from memchecker. We have to do this because ctest_read_custom_files() doesn't work.
configure_file (${PROJECT_SOURCE_DIR}/CTestCustom.cmake ${PROJECT_BINARY_DIR})


install (FILES examples/README.md examples/benchmark_hash.cc
         DESTINATION share/pilot/examples)
install (FILES examples/benchmark_dd/run_dd.sh
         DESTINATION share/pilot/examples/benchmark_dd)
install (FILES examples/benchmark_dd_no_wps/run_dd_extract_tp.sh
         DESTINATION share/pilot/examples/benchmark_dd_no_wps)
install (PROGRAMS examples/make.sh
         DESTINATION share/pilot/examples)

set (CPACK_PACKAGE_NAME pilot-bench)
set (CPACK_PACKAGE_VERSION ${PILOT_VERSION_MAJOR}.${PILOT_VERSION_MINOR})
set (CPACK_PACKAGE_VENDOR "Project ASCAR")
set (CPACK_PACKAGE_CONTACT "yanli@tuneup.ai")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Pilot is a tool and a library (libpilot) that help you run benchmarks to get scientifically accurate and meaningful results fast.")

set (CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/post-install.sh")
set (CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/post-install.sh")
set (CPACK_RPM_PACKAGE_SUMMARY "Pilot Benchmark Framework")
set (CPACK_RPM_PACKAGE_LICENSE GPL)
string(TIMESTAMP RPM_RELEASE "%Y%m%dT%H%M%S")
set (CPACK_RPM_PACKAGE_RELEASE ${RPM_RELEASE})
set (CPACK_RPM_PACKAGE_GROUP "Development")
set (CPACK_RPM_PACKAGE_VENDOR "Project ASCAR")
set (CPACK_RPM_PACKAGE_URL "https://ascar.io/pilot")
set (CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})
set (CPACK_RPM_CHANGELOG_FILE "${CMAKE_SOURCE_DIR}/packaging/rpm_changelog.txt")

set (CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)")

include (CPack)
