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
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SurfaceMaterials/PBRSurface/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SurfaceMaterials/PBRSurface/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SurfaceMaterials/Skybox/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SurfaceMaterials/Skybox/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/VolumeMaterials/Volume/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/VolumeMaterials/Volume/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/GBuffers/ShadowMap/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/GBuffers/ShadowMap/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/GBuffers/FragmentDepth/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/GBuffers/VRMask/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/GBuffers/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/GBuffers/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Compute/EdgeDetect/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Compute/TAA/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Compute/ProgressiveRefinement/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Compute/ToneMapping/shader.comp

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Compute/VSM/GaussianBlurX/shader.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Compute/VSM/GaussianBlurY/shader.comp

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Compute/ATrous/shader.comp

    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracedMaterials/TutorialShaders/shader.rgen
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracedMaterials/TutorialShaders/shader.rchit
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracedMaterials/TutorialShaders/shader.rmiss
    PARENT_SCOPE
)