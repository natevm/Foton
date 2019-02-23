message("Adding subdirectory: Resources")

set(
    Resources_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/FragmentCommon.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderCommon.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/VertexCommon.hxx
    PARENT_SCOPE
)

set (
    Resources_SRC
    PARENT_SCOPE
)
