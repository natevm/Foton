message("Adding subdirectory: Libraries")

add_subdirectory(GLFW)
add_subdirectory(Vulkan)
if(BUILD_OPENVR)
    add_subdirectory(OpenVR)
endif(BUILD_OPENVR)
if(BUILD_SPACEMOUSE)
add_subdirectory(SpaceMouse)
endif(BUILD_SPACEMOUSE)

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
