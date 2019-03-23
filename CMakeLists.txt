# ┌──────────────────────────────────────────────────────────────────┐
# │  Projects Settings                                               │
# └──────────────────────────────────────────────────────────────────┘
cmake_minimum_required (VERSION 3.13)
project(Pluto)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_MACOSX_RPATH 1)

# Use c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
if(MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif(MSVC)

# Set the install prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install/" CACHE PATH "..." FORCE)
endif()

# Default to release build
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the build configuration" FORCE)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Definitions                                                     │
# └──────────────────────────────────────────────────────────────────┘

# Force binaries to go to build folder, not subfolders
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )


# ┌──────────────────────────────────────────────────────────────────┐
# │  External Dependencies                                           │
# └──────────────────────────────────────────────────────────────────┘

# glfw
#    note: on linux, xorg-dev might be required
set(GLFW_BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE STRING "" FORCE)
set(GLFW_INSTALL        OFF CACHE STRING "" FORCE)
add_subdirectory(external/glfw)
include_directories(SYSTEM external/glfw/include)
set_property(TARGET glfw PROPERTY FOLDER "external/GLFW3" )

# use GLI
find_package(GLI REQUIRED)
include_directories(SYSTEM ${GLI_INCLUDE_DIR})

# use GLM
find_package(GLM 0.9.7 REQUIRED)
include_directories(SYSTEM ${GLM_INCLUDE_DIR})

# geometry generator
add_subdirectory(external/generator)
include_directories(SYSTEM external/generator/include)
set_property(TARGET generator PROPERTY FOLDER "external/generator")

# swig
find_package(SWIG 3.0.8 REQUIRED)
include(${SWIG_USE_FILE})

# vulkan
find_package(Vulkan 1.1.97 REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})

# zeromq
find_package(ZMQ QUIET)
if(${ZMQ_FOUND})
    include_directories(${ZMQ_INCLUDE_DIRS})
else()
    set(ZMQ_LIBRARIES)
endif(${ZMQ_FOUND})

# python
find_package(Python3 3.6 COMPONENTS Interpreter Development REQUIRED)
include_directories(SYSTEM ${Python3_INCLUDE_DIRS})

# tiny
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/external/tiny)

# openvr
option(BUILD_OPENVR "Build with OpenVR support" OFF)
if(BUILD_OPENVR)
    set(SpaceMouse_Libs "${CMAKE_CURRENT_SOURCE_DIR}/external/openvr")
    find_package(OpenVRSDK REQUIRED)
    include_directories(SYSTEM ${OPENVR_SDK_INCLUDE_DIR})
endif(BUILD_OPENVR)

# spacemouse
# Todo: make a cmake file for this
option(BUILD_SPACEMOUSE "Build with SpaceMouse support" OFF)
if(BUILD_SPACEMOUSE)
    set(SPACEMOUSE_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/external/spacemouse/Lib/x64/siapp.lib")
    set(SPACEMOUSE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/spacemouse/Inc")
    include_directories(SYSTEM ${SPACEMOUSE_INCLUDE_DIR})
endif(BUILD_SPACEMOUSE)

# google test
option(BUILD_TESTS "Include the regression and unit tests target" ON)
if(BUILD_TESTS)
    add_subdirectory(external/googletest)
    enable_testing()
    include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif(BUILD_TESTS)


# add libraries to a list for linking
set (
    LIBRARIES
    ${Python3_LIBRARIES} 
    ${Vulkan_LIBRARY}
    ${GLFW_LIBRARIES}
    ${ZMQ_LIBRARIES}
    ${OPENVR_SDK_LIBRARY}
    ${SPACEMOUSE_LIBRARIES}
    generator
)

message(WARN ${Vulkan_LIBRARY})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Options                                                         │
# └──────────────────────────────────────────────────────────────────┘

set(platformSupportsMultiview OFF)
if(APPLE)
    set(platformSupportsMultiview ON)
endif(APPLE)

option(DISABLE_MULTIVIEW "Force multiview support off (not currently supported on macOS)" ${platformSupportsMultiview})

# ┌──────────────────────────────────────────────────────────────────┐
# │  Add source files                                                │
# └──────────────────────────────────────────────────────────────────┘
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(Pluto)

