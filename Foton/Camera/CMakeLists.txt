message("Adding subdirectory: Camera")

set(
    Camera_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.hxx
    PARENT_SCOPE
)

set (
    Camera_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cxx
    PARENT_SCOPE
)
