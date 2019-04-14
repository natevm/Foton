message("Adding subdirectory: PlutoEngine")

# Add all subdirectories
add_subdirectory(Camera)
add_subdirectory(Entity)
add_subdirectory(Libraries)
add_subdirectory(Light)
add_subdirectory(Material)
add_subdirectory(Mesh)
add_subdirectory(Prefabs)
add_subdirectory(Systems)
add_subdirectory(Texture)
add_subdirectory(Tools)
add_subdirectory(Transform)

set (SRC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.cxx
    ${Camera_SRC}
    ${Entity_SRC}
    ${Libraries_SRC}
    ${Light_SRC}
    ${Resources_SRC}
    ${Material_SRC}
    ${Mesh_SRC}
    ${Prefabs_SRC}
    ${Systems_SRC}
    ${Texture_SRC}
    ${Tools_SRC}
    ${Transform_SRC}
)

set (HDR 
    ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.cxx
    ${Camera_HDR}
    ${Entity_HDR}
    ${Libraries_HDR}
    ${Light_HDR}
    ${Resources_HDR}
    ${Material_HDR}
    ${Mesh_HDR}
    ${Prefabs_HDR}
    ${Systems_HDR}
    ${Texture_HDR}
    ${Tools_HDR}
    ${Transform_HDR}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC} ${HDR})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Library                                                   │
# └──────────────────────────────────────────────────────────────────┘
add_library(PlutoLib SHARED ${SRC} ${HDR})
target_link_libraries(PlutoLib PUBLIC glfw ${LIBRARIES})
set_target_properties(PlutoLib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
install(TARGETS PlutoLib DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Engine Executable                                         │
# └──────────────────────────────────────────────────────────────────┘
add_executable(PlutoEngine ${CMAKE_CURRENT_SOURCE_DIR}/main.cxx)
set_target_properties(PlutoEngine PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Pluto")
set_target_properties(PlutoEngine PROPERTIES OUTPUT_NAME "pluto")
target_link_libraries(PlutoEngine PUBLIC PlutoLib ${LIBRARIES} )
target_include_directories(PlutoEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS PlutoEngine
    DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto
)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Module                                                    │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Pluto TYPE SHARED LANGUAGE python OUTPUT_DIR ${CMAKE_BINARY_DIR}/SWIG/Pluto SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i)
if (WIN32) 
set_property(TARGET Pluto PROPERTY SWIG_GENERATED_COMPILE_OPTIONS -bigobj)
endif()
if (BUILD_OPENVR)
set (SWIG_DEFINITIONS ${SWIG_DEFINITIONS} BUILD_OPENVR)
endif(BUILD_OPENVR)
if (BUILD_SPACEMOUSE)
set (SWIG_DEFINITIONS ${SWIG_DEFINITIONS} BUILD_SPACEMOUSE)
endif(BUILD_SPACEMOUSE)
set_property(TARGET Pluto PROPERTY SWIG_COMPILE_DEFINITIONS ${SWIG_DEFINITIONS})
target_link_libraries(Pluto PUBLIC ${LIBRARIES} PlutoLib)

set_property(TARGET
    Pluto
    PROPERTY FOLDER "swig_libraries/Pluto"
)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${CMAKE_BINARY_DIR}/SWIG/Pluto/Pluto.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/bin/_Pluto.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${CMAKE_BINARY_DIR}/bin/_Pluto.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/lib/_Pluto.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
else()
install(FILES ${CMAKE_BINARY_DIR}/lib/_Pluto.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  GLM Module                                                      │
# └──────────────────────────────────────────────────────────────────┘
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(GLM TYPE SHARED LANGUAGE python OUTPUT_DIR ${CMAKE_BINARY_DIR}/SWIG/GLM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i)
target_link_libraries(GLM PUBLIC ${LIBRARIES} PlutoLib)

set_property(TARGET
    GLM
    PROPERTY FOLDER "swig_libraries/GLM"
)

# Install
install(FILES ${CMAKE_BINARY_DIR}/SWIG/GLM/GLM.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/bin/_GLM.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${CMAKE_BINARY_DIR}/bin/_GLM.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/lib/_GLM.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
else()
install(FILES ${CMAKE_BINARY_DIR}/lib/_GLM.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Shaders                                                         │
# └──────────────────────────────────────────────────────────────────┘

if (WIN32)
set(GLSLC ${PROJECT_SOURCE_DIR}/external/vulkan/glslc.exe)
else()
find_program(GLSLC glslc)
endif()
# set(GLSLC_CONFIG ${PROJECT_SOURCE_DIR}/external/vulkan/glsl.config)
set(GLSLC_OPTIMIZATION -O)
# set(GLSLC_OPTIMIZATION -g -o0)

set(SHADERS_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/UniformColor/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/UniformColor/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Blinn/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Blinn/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/NormalSurface/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/NormalSurface/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/TexCoordSurface/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/TexCoordSurface/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/PBRSurface/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/PBRSurface/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Skybox/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Skybox/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Depth/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Depth/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/VolumeMaterials/Volume/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/VolumeMaterials/Volume/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/GBuffers/ShadowMap/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/GBuffers/ShadowMap/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/GBuffers/FragmentDepth/shader.vert

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/GBuffers/FragmentPosition/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/GBuffers/FragmentPosition/shader.frag

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/RaytracedMaterials/TutorialShaders/shader.rgen
)

set(spvfiles "")
foreach(shader_file ${SHADERS_SRC})
    set(spv_file ${shader_file})
    string(REPLACE "shader.vert" "vert.spv" spv_file ${spv_file})
    string(REPLACE "shader.frag" "frag.spv" spv_file ${spv_file})
    string(REPLACE "shader.rgen" "rgen.spv" spv_file ${spv_file})
    set(extra_flags "")
    if(APPLE)
        set(extra_flags ${extra_flags} -DAPPLE=1)
    endif(APPLE)
    if(${DISABLE_MULTIVIEW})
        set(extra_flags ${extra_flags} -DDISABLE_MULTIVIEW)
    endif(${DISABLE_MULTIVIEW})
    if(${DISABLE_REVERSE_Z})
        set(extra_flags ${extra_flags} -DDISABLE_REVERSE_Z)
    endif(${DISABLE_REVERSE_Z})
    add_custom_command(
        OUTPUT ${spv_file}_ # force file to regen every time
        COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} ${extra_flags} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${shader_file} -o ${spv_file}
        DEPENDS ${shader_file}
    )
    list(APPEND spvfiles ${spv_file}_)
endforeach(shader_file)

add_custom_target(Shaders ALL
    DEPENDS ${spvfiles}
    SOURCES ${SHADERS_SRC}
)

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/
    FILES ${SHADERS_SRC}
)

install(FILES ${PROJECT_SOURCE_DIR}/setup.py DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/install.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${PROJECT_SOURCE_DIR}/MANIFEST.in DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_PREFIX})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/Pluto/Resources DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Install Other Dynamic Libraries and miscelaneous items          │
# └──────────────────────────────────────────────────────────────────┘
if(APPLE)
    # Install MoltenVK libraries
    install(DIRECTORY $ENV{VULKAN_SDK}/lib DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)

    # Install vulkan icd and validation layers
    install(DIRECTORY $ENV{VULKAN_SDK}/etc DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
    install(DIRECTORY $ENV{VULKAN_SDK}/etc DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
endif(APPLE)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Tests                                                           │
# └──────────────────────────────────────────────────────────────────┘
if(BUILD_TESTS)
    file(GLOB TEST_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cxx)
    add_executable(runUnitTests ${TEST_SRC_FILES})
    target_link_libraries(runUnitTests ${LIBRARIES} PlutoLib gtest_main)
    add_test(NAME runUnitTests COMMAND example)

    if (WIN32) 
    install(FILES ${CMAKE_BINARY_DIR}/bin/runUnitTests.exe DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
    install(FILES ${CMAKE_BINARY_DIR}/bin/runUnitTests.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto OPTIONAL)
    elseif(APPLE)
    install(FILES ${CMAKE_BINARY_DIR}/bin/runUnitTests DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
    else()
    install(FILES ${CMAKE_BINARY_DIR}/lib/runUnitTests.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
    endif()

endif(BUILD_TESTS)
