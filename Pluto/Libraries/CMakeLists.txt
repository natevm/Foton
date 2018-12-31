message("Adding subdirectory: Libraries")

add_subdirectory(GLFW)
add_subdirectory(Vulkan)
add_subdirectory(OpenVR)

set(
    Libraries_HDR
    ${GLFW_HDR}
    ${Vulkan_HDR}
    ${OpenVR_HDR}
    PARENT_SCOPE
)

set (
    Libraries_SRC
    ${GLFW_SRC}
    ${Vulkan_SRC}
    ${OpenVR_SRC}
    PARENT_SCOPE
)
