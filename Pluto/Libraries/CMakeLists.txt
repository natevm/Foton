message("Adding subdirectory: Libraries")

add_subdirectory(GLFW)
add_subdirectory(Vulkan)

set(
    Libraries_HDR
    ${GLFW_HDR}
    ${Vulkan_HDR}
    PARENT_SCOPE
)

set (
    Libraries_SRC
    ${GLFW_SRC}
    ${Vulkan_SRC}
    PARENT_SCOPE
)
