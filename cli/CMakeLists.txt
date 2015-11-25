find_package (Curses REQUIRED)

include_directories (${CURSES_INCLUDE_DIR})

add_executable(pilot pilot.cc)
target_link_libraries (pilot ${CURSES_LIBRARIES})

install (TARGETS pilot DESTINATION bin)
