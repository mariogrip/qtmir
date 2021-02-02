configure_file(paths.h.in ${CMAKE_CURRENT_BINARY_DIR}/paths.h @ONLY)

add_subdirectory(qtmir-demo-client)
add_subdirectory(qtmir-demo-shell)
