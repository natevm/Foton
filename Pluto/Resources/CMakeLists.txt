message("Adding subdirectory: Resources")

set(
    Resources_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/FragmentCommon.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/FragmentVaryings.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/Descriptors.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/VertexCommon.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/VertexVaryings.hxx
    PARENT_SCOPE
)

set (
    Resources_SRC
    PARENT_SCOPE
)
