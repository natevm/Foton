message("Adding subdirectory: PlutoEngine")

# Weird swig thing...
cmake_policy(SET CMP0078 NEW)

if(APPLE)
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
endif(APPLE)

# Add all subdirectories
add_subdirectory(Camera)
add_subdirectory(Entity)
add_subdirectory(Libraries)
add_subdirectory(Light)
add_subdirectory(Material)
add_subdirectory(Mesh)
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
    ${Material_SRC}
    ${Mesh_SRC}
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
    ${Material_HDR}
    ${Mesh_HDR}
    ${Systems_HDR}
    ${Texture_HDR}
    ${Tools_HDR}
    ${Transform_HDR}
)


# Global defines
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if (BUILD_OPENVR)
add_definitions(-DBUILD_OPENVR)
endif(BUILD_OPENVR)

if (BUILD_SPACEMOUSE)
add_definitions(-DBUILD_SPACEMOUSE)
endif(BUILD_SPACEMOUSE)

# RPATH for *NIX distros

set(RPATHS "${CMAKE_INSTALL_PREFIX};${CMAKE_INSTALL_PREFIX}/Pluto;${CMAKE_INSTALL_PREFIX}/Pluto/Systems;${CMAKE_INSTALL_PREFIX}/Pluto/Libraries;")
set(CMAKE_INSTALL_RPATH ${INSTALL_RPATH})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Library                                                   │
# └──────────────────────────────────────────────────────────────────┘
add_library(PlutoLib SHARED ${SRC} ${HDR})
target_link_libraries(PlutoLib PUBLIC glfw ${LIBRARIES})
set_target_properties(PlutoLib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
#target_compile_definitions(PlutoLib PRIVATE ZMQ_BUILD_DRAFT_API=1)
set_target_properties(PlutoLib PROPERTIES INSTALL_RPATH "${RPATHS}")
set_target_properties(PlutoLib PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
set_target_properties(PlutoLib PROPERTIES MACOSX_RPATH TRUE)
endif(APPLE)
install(TARGETS PlutoLib DESTINATION ${CMAKE_INSTALL_PREFIX})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC} ${HDR})


# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Engine Executable                                         │
# └──────────────────────────────────────────────────────────────────┘

add_executable(PlutoEngine ${CMAKE_CURRENT_SOURCE_DIR}/main.cxx)
target_link_libraries(PlutoEngine PUBLIC PlutoLib ${LIBRARIES} )
target_include_directories(PlutoEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#target_compile_definitions(PlutoEngine PRIVATE ZMQ_BUILD_DRAFT_API=1)
set_target_properties(PlutoEngine PROPERTIES INSTALL_RPATH "${RPATHS}")
set_target_properties(PlutoEngine PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
set_target_properties(PlutoEngine PROPERTIES MACOSX_RPATH TRUE)
endif(APPLE)
install(TARGETS PlutoEngine DESTINATION ${CMAKE_INSTALL_PREFIX})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Module                                                    │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Pluto TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR} SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i)
set_target_properties(Pluto PROPERTIES INSTALL_RPATH "${RPATHS}")
set_target_properties(Pluto PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
set_target_properties(Pluto PROPERTIES MACOSX_RPATH TRUE)
endif(APPLE)
target_link_libraries(Pluto PUBLIC ${LIBRARIES} PlutoLib)

set_property(TARGET
    Pluto
    PROPERTY FOLDER "swig_libraries/Pluto"
)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Pluto.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Pluto.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${CMAKE_BINARY_DIR}/_Pluto.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/_Pluto.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
else()
install(FILES ${CMAKE_BINARY_DIR}/_Pluto.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Libraries Module                                                │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Libraries TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Libraries SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i)
set_target_properties(Libraries PROPERTIES INSTALL_RPATH "${RPATHS}")
set_target_properties(Libraries PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
set_target_properties(Libraries PROPERTIES MACOSX_RPATH TRUE)
endif(APPLE)
target_link_libraries(Libraries PUBLIC ${LIBRARIES} PlutoLib)

set_property(TARGET
    Libraries
    PROPERTY FOLDER "swig_libraries/Libraries"
)


# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Libraries.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
else()
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
endif()

# Build GLM SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(GLM TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i)
set_target_properties(GLM PROPERTIES INSTALL_RPATH "${RPATHS}")
set_target_properties(GLM PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
target_link_libraries(GLM PUBLIC ${LIBRARIES} PlutoLib)

set_property(TARGET
    GLM
    PROPERTY FOLDER "swig_libraries/GLM"
)

# Install
install(FILES ${CMAKE_BINARY_DIR}/Pluto/GLM.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_GLM.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/_GLM.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/_GLM.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
else()
install(FILES ${CMAKE_BINARY_DIR}/_GLM.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Systems module                                                  │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Systems TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Systems SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i)
set_target_properties(Systems PROPERTIES INSTALL_RPATH "${RPATHS}")
set_target_properties(Systems PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
set_target_properties(Systems PROPERTIES MACOSX_RPATH TRUE)
endif(APPLE)
target_link_libraries(Systems PUBLIC ${LIBRARIES} PlutoLib)

set_property(TARGET
    Systems
    PROPERTY FOLDER "swig_libraries/Systems"
)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Systems/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Systems.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Systems.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)
install(FILES ${CMAKE_BINARY_DIR}/_Systems.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems OPTIONAL)
elseif(APPLE)
install(FILES ${CMAKE_BINARY_DIR}/_Systems.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)
else()
install(FILES ${CMAKE_BINARY_DIR}/_Systems.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Shaders                                                         │
# └──────────────────────────────────────────────────────────────────┘

if (WIN32)
set(GLSLC ${PROJECT_SOURCE_DIR}/external/vulkan/glslc.exe)
else()
find_program(GLSLC glslc)
endif()
set(GLSLC_CONFIG ${PROJECT_SOURCE_DIR}/external/vulkan/glsl.config)
set(GLSLC_OPTIMIZATION -O)

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

    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/RaytracedMaterials/TutorialShaders/shader.rgen
)

set(spvfiles "")
foreach(shader_file ${SHADERS_SRC})
    set(spv_file ${shader_file})
    string(REPLACE "shader.vert" "vert.spv" spv_file ${spv_file})
    string(REPLACE "shader.frag" "frag.spv" spv_file ${spv_file})
    set(extra_flags "")
    if(${DISABLE_MUlTIVIEW})
        set(extra_flags ${extra_flags} -DDISABLE_MUlTIVIEW)
    endif(${DISABLE_MUlTIVIEW})
    message("Extra flags: " ${extra_flags})
    add_custom_command(
        OUTPUT ${spv_file}
        COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} ${extra_flags} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${shader_file} -o ${spv_file} -flimit-file ${GLSLC_CONFIG}
        DEPENDS ${shader_file}
    )
    list(APPEND spvfiles ${spv_file})
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
install(FILES ${PROJECT_SOURCE_DIR}/install.py DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/MANIFEST.in DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_PREFIX})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/Pluto/Resources DESTINATION ${CMAKE_INSTALL_PREFIX})
