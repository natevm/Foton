message("Adding subdirectory: Systems")

add_subdirectory(RenderSystem)
add_subdirectory(EventSystem)
add_subdirectory(PythonSystem)
add_subdirectory(PhysicsSystem)

set(
    Systems_HDR
    ${EventSystem_HDR}
    ${PythonSystem_HDR}
    ${RenderSystem_HDR}
    ${PhysicsSystem_HDR}
    PARENT_SCOPE
)

set (
    Systems_SRC
    ${EventSystem_SRC}
    ${PythonSystem_SRC}
    ${RenderSystem_SRC}
    ${PhysicsSystem_SRC}
    PARENT_SCOPE
)
