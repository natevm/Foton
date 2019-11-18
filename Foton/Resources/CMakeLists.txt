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
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RasterPrograms/Skybox/shader.vert
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/Compositing/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/TAA/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/VSM/GaussianBlurX/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/VSM/GaussianBlurY/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_1_ReprojectSeeds/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_2_ComputeGradientAndMoments/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_3_ReconstructGradientAndMoments/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_4_DiffuseTemporalAccumulation/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_4_SpecularTemporalAccumulation/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_5_EstimateVariance/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_6_ReconstructDiffuse/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_6_ReconstructGlossy/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/ASVGF_7_Remodulate/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ComputePrograms/CopyHistory/shader.comp
    PARENT_SCOPE
)