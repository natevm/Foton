# ┌──────────────────────────────────────────────────────────────────┐
# │  Build executable                                                │
# └──────────────────────────────────────────────────────────────────┘
# set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Vinyl.i PROPERTY CPLUSPLUS ON)
# set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Vinyl.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)

# swig_add_library(Vinyl LANGUAGE python SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Vinyl.i ${CMAKE_CURRENT_SOURCE_DIR}/Vinyl.hxx)
# swig_link_libraries(Vinyl ${PYTHON_LIBRARIES})


# install(FILES ${CMAKE_BINARY_DIR}/Vinyl/Vinyl.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.json DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl)

message("Adding subdirectory: Systems")
add_subdirectory(Systems)
