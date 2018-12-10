message("Adding subdirectory: Material")

set(
    Material_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Material.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/MaterialStruct.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/PipelineParameters.hxx
    PARENT_SCOPE
)

set (
    Material_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Material.cxx
    PARENT_SCOPE
)
