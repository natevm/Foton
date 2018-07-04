add_subdirectory(GLFW)
add_subdirectory(Bullet)
add_subdirectory(Vulkan)

set (
    Systems_SRC
    ${Bullet_SRC}
    ${GLFW_SRC}
    ${Vulkan_SRC}
)

set (
    Systems_LIBS
    ${PYTHON_LIBRARIES} 
    ${Vulkan_LIBRARY}
    BulletDynamics 
    BulletCollision 
    LinearMath
    glfw
)

add_library(SystemsLib SHARED ${Systems_SRC})
set_target_properties(SystemsLib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(SystemsLib PUBLIC ${Systems_LIBS})

# message(WARN TESTING)
# message(WARN ${BulletDynamics})
# message(WARN ${Vulkan_LIBRARY})

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)

message("Creating Package: Systems")


set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems.i PROPERTY COMPILE_OPTIONS "-Werror")


swig_add_library(Systems TYPE SHARED LANGUAGE python SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Systems.i ${Systems_SRC})
set_property(TARGET Systems PROPERTY SWIG_GENERATED_COMPILE_OPTIONS -Wl,--no-allow-shlib-undefined)


target_link_libraries(Systems PUBLIC SystemsLib)

install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Systems/Systems.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Systems/_Systems.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)