message("Adding subdirectory: Vulkan")

set(
    Vulkan_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan.hxx
    PARENT_SCOPE
)

set(
    Vulkan_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan.cxx
    PARENT_SCOPE
)