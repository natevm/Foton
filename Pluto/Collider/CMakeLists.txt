message("Adding subdirectory: Collider")

set(
    Collider_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Collider.hxx
    PARENT_SCOPE
)

set (
    Collider_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Collider.cxx
    PARENT_SCOPE
)
