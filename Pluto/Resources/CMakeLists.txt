message("Adding subdirectory: Resources")

set(
    Resources_HDR
    PARENT_SCOPE
)

set (
    Resources_SRC
    PARENT_SCOPE
)

set(SHADERS_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/PrimaryVisibility/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/PrimaryVisibility/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/Skybox/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/Skybox/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/ShadowMap/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/ShadowMap/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/FragmentDepth/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/VRMask/shader.vert

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracePrograms/PrimaryVisibility/shader.rgen
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracePrograms/PrimaryVisibility/shader.rchit
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracePrograms/PrimaryVisibility/shader.rmiss
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracePrograms/PathTracer/shader.rgen
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracePrograms/PathTracer/shader.rchit
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracePrograms/PathTracer/shader.rmiss

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/DeferredFinal/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/EdgeDetect/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ProgressiveRefinement/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ToneMapping/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/TAA/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/VSM/GaussianBlurX/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/VSM/GaussianBlurY/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/SVGF_ATrous/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/SVGF_TAA/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/SVGF_Remodulate/shader.comp
    PARENT_SCOPE
)