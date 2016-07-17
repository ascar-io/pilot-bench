cmake_minimum_required (VERSION 2.6)
project (pilot)

###############################################################################
## VERSION

set (PILOT_VERSION_MAJOR 0)
set (PILOT_VERSION_MINOR 1)

###############################################################################
## OPTIONS
option (WITH_PYTHON "Build Python binding" OFF)

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
  URL https://googletest.googlecode.com/files/gtest-1.7.0.zip
  # Comment above line, and uncomment line below to use subversion.
  # SVN_REPOSITORY http://googletest.googlecode.com/svn/trunk/
  # Uncomment line below to freeze a revision (here the one for 1.7.0)
  # SVN_REVISION -r700

  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(gtest SOURCE_DIR BINARY_DIR)
set (GTEST_INCLUDE_DIR ${SOURCE_DIR})
set (GTEST_BINARY_DIR ${BINARY_DIR})


###############################################################################
## TUI LIBRARIES

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
install (PROGRAMS examples/make.sh
         DESTINATION share/pilot/examples)

set (CPACK_PACKAGE_NAME pilot-bench)
set (CPACK_PACKAGE_VENDOR "Project ASCAR")
set (CPACK_PACKAGE_CONTACT "yanli@ascar.io")
set (CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/post-install.sh")
set (CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/post-install.sh")
set (CPACK_RPM_PACKAGE_SUMMARY "Pilot Benchmark Framework")
set (CPACK_RPM_PACKAGE_LICENSE GPL)
set (CPACK_RPM_PACKAGE_RELEASE 1.el7)
set (CPACK_RPM_PACKAGE_GROUP "Development")
set (CPACK_RPM_PACKAGE_VENDOR "Project ASCAR")
set (CPACK_RPM_PACKAGE_URL "https://ascar.io/pilot")
set (CPACK_RPM_PACKAGE_DESCRIPTION "Pilot is a tool and a library (libpilot) that help you run benchmarks to get scientifically accurate and meaningful results fast.")
set (CPACK_RPM_CHANGELOG_FILE "${CMAKE_SOURCE_DIR}/packaging/rpm_changelog.txt")
include (CPack)
