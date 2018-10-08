# ┌──────────────────────────────────────────────────────────────────┐
# │  Build executable                                                │
# └──────────────────────────────────────────────────────────────────┘
message("Adding subdirectory: VinylEngine")

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.json DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl)

# Add all subdirectories
add_subdirectory(BaseClasses)
add_subdirectory(Libraries)
add_subdirectory(Systems)
add_subdirectory(Components)
add_subdirectory(Tools)

# Build Vinyl library
add_library(Vinyl SHARED ${Libraries_SRC} ${Systems_SRC} ${Components_SRC} ${Tools_SRC})
target_link_libraries(Vinyl PUBLIC ${LIBRARIES})
set_target_properties(Vinyl PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
target_compile_definitions(Vinyl PRIVATE "ALIB_FEAT_SINGLETON_MAPPED_ON")
install(FILES ${CMAKE_BINARY_DIR}/Vinyl.dll DESTINATION ${CMAKE_INSTALL_PREFIX})

# Build custom interpreter
add_executable(VinylEngine ${CMAKE_CURRENT_SOURCE_DIR}/main.cxx)
target_link_libraries(VinylEngine PUBLIC Vinyl ${Libraries} )
target_include_directories(VinylEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine.exe DESTINATION ${CMAKE_INSTALL_PREFIX})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Libraries module                                                │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Libraries TYPE SHARED LANGUAGE python SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Libraries.i)
target_link_libraries(Libraries PUBLIC ${LIBRARIES} Vinyl)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Libraries.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Libraries)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Libraries)
install(FILES ${CMAKE_BINARY_DIR}/_Libraries.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Libraries OPTIONAL)
else()
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Libraries/_Libraries.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Libraries)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Systems module                                                  │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Systems TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Systems SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Systems/Systems.i)
target_link_libraries(Systems PUBLIC ${LIBRARIES} Vinyl)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Systems/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Systems.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Systems.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)
install(FILES ${CMAKE_BINARY_DIR}/_Systems.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems OPTIONAL)
else()
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Systems/_Systems.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Systems)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Components module                                               │
# └──────────────────────────────────────────────────────────────────┘

# Build SWIG module 
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Components/Components.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Components/Components.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES TRUE)
swig_add_library(Components TYPE SHARED LANGUAGE python OUTFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Components SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Components/Components.i)
target_link_libraries(Components PUBLIC ${LIBRARIES} Vinyl)

# Install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Components/__init__.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Components)
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Components.py DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Components)

if (WIN32) 
install(FILES ${CMAKE_BINARY_DIR}/_Components.pyd DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Components)
install(FILES ${CMAKE_BINARY_DIR}/_Components.pdb DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Components OPTIONAL)
else()
install(FILES ${CMAKE_BINARY_DIR}/VinylEngine/Components/_Components.so DESTINATION ${CMAKE_INSTALL_PREFIX}/Vinyl/Components)
endif()