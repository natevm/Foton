message("Adding subdirectory: RenderSystem")

set(
    RenderSystem_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderSystem.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/PipelineParameters.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/PushConstants.hxx
    PARENT_SCOPE
)

set(
    RenderSystem_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderSystem.cxx
    PARENT_SCOPE
)