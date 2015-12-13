set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

set (LIBPILOT_LIBRARIES libpilot)

include_directories (${CDK_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/lib/interface_include)

add_executable (pilot pilot.cc)
add_dependencies (pilot cdk)
target_compile_features (pilot PRIVATE cxx_auto_type)
target_link_libraries (pilot ${CDK_LIBRARIES} ${LIBPILOT_LIBRARIES} ${Boost_LIBRARIES})

install (TARGETS pilot DESTINATION bin)
