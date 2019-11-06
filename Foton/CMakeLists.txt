message("Adding subdirectory: FotonEngine")

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
add_subdirectory(Collider)
add_subdirectory(RigidBody)
add_subdirectory(Constraint)
add_subdirectory(Importers)
add_subdirectory(Resources)

set (SRC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Foton.cxx
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
    ${Collider_SRC}
    ${RigidBody_SRC}
    ${Constraint_SRC}
    ${Importers_SRC}
)

set (HDR 
    ${CMAKE_CURRENT_SOURCE_DIR}/Foton.cxx
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
    ${Collider_HDR}
    ${RigidBody_HDR}
    ${Constraint_HDR}
    ${Importers_HDR}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC} ${HDR})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Foton Library                                                   │
# └──────────────────────────────────────────────────────────────────┘
add_library(FotonLib SHARED ${SRC} ${HDR} )
target_link_libraries(FotonLib PUBLIC glfw ${LIBRARIES})
set_target_properties(FotonLib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
install(TARGETS FotonLib 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton
    RENAME "FotonLib"
)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Foton Engine Executable                                         │
# └──────────────────────────────────────────────────────────────────┘
add_executable(FotonEngine ${CMAKE_CURRENT_SOURCE_DIR}/main.cxx)
set_target_properties(FotonEngine PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Foton")
set_target_properties(FotonEngine PROPERTIES OUTPUT_NAME "Foton")
target_link_libraries(FotonEngine PUBLIC FotonLib ${LIBRARIES} )
target_include_directories(FotonEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS FotonEngine
    DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton
)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Foton Module                                                    │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Foton.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Foton.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Foton TYPE SHARED LANGUAGE python OUTPUT_DIR ${CMAKE_BINARY_DIR}/SWIG/Foton SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Foton.i)
set_property(TARGET Foton PROPERTY SWIG_DEPENDS ${HDR})
if (WIN32) 
set_property(TARGET Foton PROPERTY SWIG_GENERATED_COMPILE_OPTIONS -bigobj)
endif()
if (BUILD_SPACEMOUSE)
set (SWIG_DEFINITIONS ${SWIG_DEFINITIONS} BUILD_SPACEMOUSE)
endif(BUILD_SPACEMOUSE)
set_property(TARGET Foton PROPERTY SWIG_COMPILE_DEFINITIONS ${SWIG_DEFINITIONS})
target_link_libraries(Foton PUBLIC ${LIBRARIES} FotonLib)

set_property(TARGET
    Foton
    PROPERTY FOLDER "swig_libraries/Foton"
)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
install(FILES ${CMAKE_BINARY_DIR}/SWIG/Foton/Foton.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/bin/_Foton.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
install(FILES ${CMAKE_BINARY_DIR}/bin/_Foton.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/lib/_Foton.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
else()
install(FILES ${CMAKE_BINARY_DIR}/lib/_Foton.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  GLM Module                                                      │
# └──────────────────────────────────────────────────────────────────┘
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(GLM TYPE SHARED LANGUAGE python OUTPUT_DIR ${CMAKE_BINARY_DIR}/SWIG/GLM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i)
target_link_libraries(GLM PUBLIC ${LIBRARIES} FotonLib)

set_property(TARGET
    GLM
    PROPERTY FOLDER "swig_libraries/GLM"
)

# Install
install(FILES ${CMAKE_BINARY_DIR}/SWIG/GLM/GLM.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/bin/_GLM.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
install(FILES ${CMAKE_BINARY_DIR}/bin/_GLM.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/lib/_GLM.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
else()
install(FILES ${CMAKE_BINARY_DIR}/lib/_GLM.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Shaders                                                         │
# └──────────────────────────────────────────────────────────────────┘

find_program(GLSLC glslc)
find_program(SPIRV_OPT spirv-opt)
# set(GLSLC_CONFIG ${PROJECT_SOURCE_DIR}/external/vulkan/glsl.config)
set(GLSLC_OPTIMIZATION -O)
# set(SPIRV_OPTIMIZATION ${SPIRV_OPTIMIZATION} --loop-fission)
# set(SPIRV_OPTIMIZATION ${SPIRV_OPTIMIZATION} --loop-fusion)
set(SPIRV_OPTIMIZATION ${SPIRV_OPTIMIZATION} -O)
set(SPIRV_OPTIMIZATION ${SPIRV_OPTIMIZATION} --ccp)
set(SPIRV_OPTIMIZATION ${SPIRV_OPTIMIZATION} --loop-unroll)
# set(GLSLC_OPTIMIZATION -g -o0)

set(spvfiles "")
foreach(shader_file ${SHADERS_SRC})
    set(spv_file ${shader_file})
    string(REPLACE "shader.vert" "vert.spv" spv_file ${spv_file})
    string(REPLACE "shader.frag" "frag.spv" spv_file ${spv_file})
    string(REPLACE "shader.comp" "comp.spv" spv_file ${spv_file})
    string(REPLACE "shader.rgen" "rgen.spv" spv_file ${spv_file})
    string(REPLACE "shader.rchit" "rchit.spv" spv_file ${spv_file})
    string(REPLACE "shader.rmiss" "rmiss.spv" spv_file ${spv_file})
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
    if (LARGE_SCENE_SUPPORT)
        set(extra_flags ${extra_flags} -DLARGE_SCENE_SUPPORT)
    endif(LARGE_SCENE_SUPPORT)
    add_custom_command(
        OUTPUT ${spv_file}_ # force file to regen every time
        COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} ${extra_flags} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${shader_file} -o ${spv_file}
        COMMAND ${SPIRV_OPT} ${SPIRV_OPTIMIZATION} ${spv_file} -o ${spv_file}
        DEPENDS ${shader_file}
    )
    list(APPEND spvfiles ${spv_file}_)
endforeach(shader_file)

add_custom_target(Shaders ALL
    DEPENDS ${spvfiles}
    SOURCES ${SHADERS_SRC}
)

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${SHADERS_SRC}
)

install(FILES ${PROJECT_SOURCE_DIR}/setup.py DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/install.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
install(FILES ${PROJECT_SOURCE_DIR}/MANIFEST.in DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_PREFIX})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/Foton/Resources DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Install Other Dynamic Libraries and miscelaneous items          │
# └──────────────────────────────────────────────────────────────────┘
if(APPLE)
    # Install MoltenVK libraries
    install(DIRECTORY $ENV{VULKAN_SDK}/lib DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)

    # Install vulkan icd and validation layers
    install(DIRECTORY $ENV{VULKAN_SDK}/etc DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
    install(DIRECTORY $ENV{VULKAN_SDK}/etc DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
endif(APPLE)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Tests                                                           │
# └──────────────────────────────────────────────────────────────────┘
if(BUILD_TESTS)
    file(GLOB TEST_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cxx)
    add_executable(runUnitTests ${TEST_SRC_FILES})
    target_link_libraries(runUnitTests ${LIBRARIES} FotonLib gtest_main)
    add_test(NAME runUnitTests COMMAND example)

    if (WIN32) 
    install(FILES ${CMAKE_BINARY_DIR}/bin/runUnitTests.exe DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
    install(FILES ${CMAKE_BINARY_DIR}/bin/runUnitTests.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton OPTIONAL)
    elseif(APPLE)
    install(FILES ${CMAKE_BINARY_DIR}/bin/runUnitTests DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
    else()
    install(FILES ${CMAKE_BINARY_DIR}/lib/runUnitTests.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Foton)
    endif()

endif(BUILD_TESTS)
