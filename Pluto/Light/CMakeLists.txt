message("Adding subdirectory: Light")

set(
    Light_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Light.hxx
    ${ComponentManager_HDR}
    PARENT_SCOPE
)

set (
    Light_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Light.cxx
    PARENT_SCOPE
)
