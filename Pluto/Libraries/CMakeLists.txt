message("Adding subdirectory: Libraries")

add_subdirectory(GLFW)
add_subdirectory(Vulkan)
add_subdirectory(OpenVR)
add_subdirectory(SpaceMouse)

set(
    Libraries_HDR
    ${GLFW_HDR}
    ${Vulkan_HDR}
    ${OpenVR_HDR}
    ${SpaceMouse_HDR}
    PARENT_SCOPE
)

set (
    Libraries_SRC
    ${GLFW_SRC}
    ${Vulkan_SRC}
    ${OpenVR_SRC}
    ${SpaceMouse_SRC}
    PARENT_SCOPE
)
