message("Adding subdirectory: Components")

add_subdirectory(Component)
add_subdirectory(Transform)
add_subdirectory(ComponentManager)

set(
    Components_HDR
    ${Component_HDR}
    ${Transform_HDR}
    ${ComponentManager_HDR}
    PARENT_SCOPE
)

set (
    Components_SRC
    ${Component_SRC}
    ${Transform_SRC}
    ${ComponentManager_SRC}
    PARENT_SCOPE
)
