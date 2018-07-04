# ┌──────────────────────────────────────────────────────────────────┐
# │  Projects Settings                                               │
# └──────────────────────────────────────────────────────────────────┘
cmake_minimum_required (VERSION 3.12)
project(Vinyl)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Use c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
if(MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 -Wl,--no-as-needed")
endif(MSVC)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Definitions                                                     │
# └──────────────────────────────────────────────────────────────────┘
add_definitions(-DResourcePath="${CMAKE_CURRENT_SOURCE_DIR}/Resources/")

# Force binaries to go to build folder, not subfolders
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} )
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR} )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR} )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )


# ┌──────────────────────────────────────────────────────────────────┐
# │  External Dependencies                                           │
# └──────────────────────────────────────────────────────────────────┘

# glfw
#    note: on linux, xorg-dev might be required
set(GLFW_BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE STRING "" FORCE)
set(GLFW_INSTALL        OFF CACHE STRING "" FORCE)
find_package(GLFW REQUIRED)
include_directories(SYSTEM ${GLFW_INCLUDE_DIR})

# bullet
find_package(Bullet REQUIRED)
include_directories(SYSTEM ${BULLET_INCLUDE_DIRS})

# use GLI
find_package(GLI REQUIRED)
include_directories(SYSTEM ${GLI_INCLUDE_DIR})

# swig
find_package(SWIG 3 REQUIRED)
include(${SWIG_USE_FILE})

# vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
message(${Vulkan_INCLUDE_DIRS})

# python
find_package(PythonLibs 3 REQUIRED)
include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})
include_directories(SYSTEM ${PYTHON_INCLUDE_PATH})

# add libraries to a list for linking
set(LIBRARIES
    ${PYTHON_LIBRARIES}
    ${Vulkan_LIBRARY}
    ${GLFW_LIBRARIES}
    ${OPENVR_LIBRARY})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Add source files                                                │
# └──────────────────────────────────────────────────────────────────┘
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(VinylEngine)
