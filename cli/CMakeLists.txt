set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
find_package (Curses REQUIRED)

ExternalProject_Add (cdk
  URL ftp://invisible-island.net/cdk/cdk.tar.gz
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/cdk
  SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/cdk/src/cdk
  CONFIGURE_COMMAND "${CMAKE_CURRENT_BINARY_DIR}/cdk/src/cdk/configure"
  BUILD_COMMAND "make"
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(cdk SOURCE_DIR BINARY_DIR)

# BINARY_DIR/include has CDK's cdk_config.h
set (CDK_INCLUDE_DIR "${SOURCE_DIR}/include" "${BINARY_DIR}/include")
set (CDK_LIBRARIES "${BINARY_DIR}/libcdk.a" ${CURSES_LIBRARIES})

set (LIBPILOT_LIBRARIES libpilot)

include_directories (${CDK_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/lib/interface_include)

add_executable (pilot pilot.cc benchmark_worker.cc)
add_dependencies (pilot cdk)
target_compile_features (pilot PRIVATE cxx_auto_type)
target_link_libraries (pilot ${CDK_LIBRARIES} ${LIBPILOT_LIBRARIES} ${Boost_LIBRARIES})

install (TARGETS pilot DESTINATION bin)
