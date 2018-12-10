message("Adding subdirectory: GLFW")

set(GLFW_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/GLFW.hxx
    PARENT_SCOPE
)

set(
    GLFW_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/GLFW.cxx
    PARENT_SCOPE
)