cmake_minimum_required (VERSION 2.6)
project (pilot)
set (PILOT_VERSION_MAJOR 0)
set (PILOT_VERSION_MINOR 1)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif (NOT CMAKE_BUILD_TYPE)

find_program( MEMORYCHECK_COMMAND valgrind )
set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" )
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
ExternalProject_Get_Property(gtest source_dir binary_dir)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/include/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories("${PROJECT_BINARY_DIR}")

# Enable Boost
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

# libpilot
add_subdirectory (lib)
# pilot command line tool
add_subdirectory (cli)
# documentation
add_subdirectory (doc)
