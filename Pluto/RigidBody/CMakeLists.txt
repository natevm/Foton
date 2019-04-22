message("Adding subdirectory: RigidBody")

set(
    RigidBody_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/RigidBody.hxx
    PARENT_SCOPE
)

set (
    RigidBody_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/RigidBody.cxx
    PARENT_SCOPE
)
