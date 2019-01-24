message("Adding subdirectory: PlutoEngine")

# Weird swig thing...
cmake_policy(SET CMP0078 NEW)

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

# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Library                                                   │
# └──────────────────────────────────────────────────────────────────┘
add_library(PlutoLib SHARED ${SRC})
target_link_libraries(PlutoLib PUBLIC ${LIBRARIES})
set_target_properties(PlutoLib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
target_compile_definitions(PlutoLib PRIVATE ZMQ_BUILD_DRAFT_API=1)
install(FILES ${CMAKE_BINARY_DIR}/PlutoLib.dll DESTINATION ${CMAKE_INSTALL_PREFIX})


# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Engine Executable                                         │
# └──────────────────────────────────────────────────────────────────┘

add_executable(PlutoEngine ${CMAKE_CURRENT_SOURCE_DIR}/main.cxx)
target_link_libraries(PlutoEngine PUBLIC PlutoLib ${LIBRARIES} )
target_include_directories(PlutoEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(PlutoEngine PRIVATE ZMQ_BUILD_DRAFT_API=1)
install(FILES ${CMAKE_BINARY_DIR}/PlutoEngine.exe DESTINATION ${CMAKE_INSTALL_PREFIX})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Pluto Module                                                    │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Pluto TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR} SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Pluto.i)
target_link_libraries(Pluto PUBLIC ${LIBRARIES} PlutoLib)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Pluto.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Pluto.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
install(FILES ${CMAKE_BINARY_DIR}/_Pluto.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto OPTIONAL)
else()
install(FILES ${CMAKE_BINARY_DIR}/Pluto/_Pluto.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Libraries Module                                                │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Libraries TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Libraries SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i)
target_link_libraries(Libraries PUBLIC ${LIBRARIES} PlutoLib)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Libraries.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries OPTIONAL)
else()
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Libraries/_Libraries.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
endif()

# Build GLM SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(GLM TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/GLM/GLM.i)
target_link_libraries(GLM PUBLIC ${LIBRARIES} PlutoLib)

# Install
install(FILES ${CMAKE_BINARY_DIR}/Pluto/GLM.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_GLM.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/_GLM.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries OPTIONAL)
else()
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Libraries/GLM.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Libraries)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Systems module                                                  │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Systems TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Systems SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i)
target_link_libraries(Systems PUBLIC ${LIBRARIES} PlutoLib)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Systems/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Systems.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Systems.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)
install(FILES ${CMAKE_BINARY_DIR}/_Systems.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems OPTIONAL)
else()
install(FILES ${CMAKE_BINARY_DIR}/Pluto/Systems/_Systems.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Pluto/Systems)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Shaders                                                         │
# └──────────────────────────────────────────────────────────────────┘
set(GLSLC ${PROJECT_SOURCE_DIR}/external/vulkan/glslc.exe)
set(GLSLC_CONFIG ${PROJECT_SOURCE_DIR}/external/vulkan/glsl.config)
set(GLSLC_OPTIMIZATION -O)

add_custom_target(Shaders 
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/UniformColor/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/UniformColor/vert.spv -flimit-file ${GLSLC_CONFIG} 
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/UniformColor/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/UniformColor/frag.spv -flimit-file ${GLSLC_CONFIG}
    
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Blinn/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Blinn/vert.spv -flimit-file ${GLSLC_CONFIG}
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Blinn/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Blinn/frag.spv -flimit-file ${GLSLC_CONFIG}

    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/NormalSurface/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/NormalSurface/vert.spv -flimit-file ${GLSLC_CONFIG}
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/NormalSurface/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/NormalSurface/frag.spv -flimit-file ${GLSLC_CONFIG}

    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/TexCoordSurface/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/TexCoordSurface/vert.spv -flimit-file ${GLSLC_CONFIG}
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/TexCoordSurface/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/TexCoordSurface/frag.spv -flimit-file ${GLSLC_CONFIG}

    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/PBRSurface/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/PBRSurface/vert.spv -flimit-file ${GLSLC_CONFIG}
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/PBRSurface/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/PBRSurface/frag.spv -flimit-file ${GLSLC_CONFIG}

    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/SkyBox/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/SkyBox/vert.spv -flimit-file ${GLSLC_CONFIG}
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/SkyBox/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/SkyBox/frag.spv -flimit-file ${GLSLC_CONFIG}

    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Depth/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Depth/vert.spv -flimit-file ${GLSLC_CONFIG}
    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Depth/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/SurfaceMaterials/Depth/frag.spv -flimit-file ${GLSLC_CONFIG}

    COMMAND ${GLSLC} ${GLSLC_OPTIMIZATION} -I ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/RaytracedMaterials/TutorialShaders/shader.rgen -o ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/RaytracedMaterials/TutorialShaders/rgen.spv -flimit-file ${GLSLC_CONFIG}
)

install(FILES ${PROJECT_SOURCE_DIR}/setup.py DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/install.py DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/MANIFEST.in DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_PREFIX})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/Pluto/Resources DESTINATION ${CMAKE_INSTALL_PREFIX})
