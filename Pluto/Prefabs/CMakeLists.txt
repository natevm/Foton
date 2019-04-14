message("Adding subdirectory: Prefabs")

set(
    Prefabs_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefabs.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/CameraPrefab.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/VRRig.hxx
    PARENT_SCOPE
)

set (
    Prefabs_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefabs.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/CameraPrefab.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/VRRig.cxx
    PARENT_SCOPE
)
