# ┌──────────────────────────────────────────────────────────────────┐
# │  Projects Settings                                               │
# └──────────────────────────────────────────────────────────────────┘
cmake_minimum_required (VERSION 3.13)
cmake_policy( VERSION 3.13...3.14 )
project(Foton CXX C)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Use c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Force 64 bit builds
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
MESSAGE(FATAL_ERROR "4 byte void pointer detected. Please switch to a 64 bit build")
endif()


if(MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)
endif(MSVC)

# Set the install prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install/" CACHE PATH "..." FORCE)
endif()

# clear postfixes
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
set(CMAKE_RELWITHDEBINFO_POSTFIX "" CACHE STRING "" FORCE)
set(CMAKE_MINSIZEREL_POSTFIX "" CACHE STRING "" FORCE)

# Default to release build
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the build configuration" FORCE)
endif()

# ┌──────────────────────────────────────────────────────────────────┐
# │  Options                                                         │
# └──────────────────────────────────────────────────────────────────┘
# option(BUILD_OPENVR "Build with OpenVR support" ON)s
option(BUILD_SPACEMOUSE "Build with SpaceMouse support" OFF)
option(BUILD_TESTS "Include the regression and unit tests target" OFF)
option(LARGE_SCENE_SUPPORT "Increases component limits. Generally only supported by discrete cards" OFF)

set(platformDisablesMultiview OFF)
set(platformDisablesReverseZProjection OFF)
if(APPLE)
    set(platformDisablesMultiview ON)
    set(platformDisablesReverseZProjection ON)
endif(APPLE)

option(DISABLE_MULTIVIEW "Force multiview support off (not currently supported on macOS)" ${platformDisablesMultiview})
option(DISABLE_REVERSE_Z "Force disabling reverse z projection (causes artifacts on macOS)" ${platformDisablesReverseZProjection})



# ┌──────────────────────────────────────────────────────────────────┐
# │  Definitions                                                     │
# └──────────────────────────────────────────────────────────────────┘

if (DISABLE_MULTIVIEW)
    add_definitions(-DDISABLE_MULTIVIEW)
endif(DISABLE_MULTIVIEW)

if (DISABLE_REVERSE_Z)
    add_definitions(-DDISABLE_REVERSE_Z)
endif(DISABLE_REVERSE_Z)

add_definitions(-DBUILD_OPENVR) 
add_definitions(-DOPENVR_BUILD_STATIC)

if (BUILD_SPACEMOUSE)
    add_definitions(-DBUILD_SPACEMOUSE)
endif(BUILD_SPACEMOUSE)

if (LARGE_SCENE_SUPPORT)
  add_definitions(-DLARGE_SCENE_SUPPORT)
endif(LARGE_SCENE_SUPPORT)
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

# For MSVC
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if(APPLE)
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
endif(APPLE)

# Default RPATH settings for *NIX distros
set(RPATHS "${CMAKE_INSTALL_PREFIX};${CMAKE_INSTALL_PREFIX}/Foton;${CMAKE_INSTALL_PREFIX}/Foton/lib;")
set(CMAKE_INSTALL_RPATH "${RPATHS}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(CMAKE_INSTALL_NAME_DIR "${RPATHS}")
# set(CMAKE_SKIP_BUILD_RPATH FALSE)
SET(CMAKE_INSTALL_NAME_DIR @executable_path)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# ┌──────────────────────────────────────────────────────────────────┐
# │  External Dependencies                                           │
# └──────────────────────────────────────────────────────────────────┘

# Git (for auto submodule cloning)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  # Update submodules as needed
  option(GIT_SUBMODULE "Check submodules during build" ON)
  if(GIT_SUBMODULE)
      message(STATUS "Submodule update")
      execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      RESULT_VARIABLE GIT_SUBMOD_RESULT)
      if(NOT GIT_SUBMOD_RESULT EQUAL "0")
          message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
      endif()
  endif()
endif()

# glfw
#    note: on linux, xorg-dev might be required
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/CMakeLists.txt")
  message(FATAL_ERROR "The GLFW submodule directory is empty! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by running \"git submodule update --init --recursive\"")
endif()
if (UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  if (NOT X11_Xrandr_FOUND OR NOT X11_Xinerama_FOUND OR NOT X11_Xkb_FOUND OR NOT X11_Xcursor_FOUND)
    message(FATAL_ERROR "Some X11 development libraries are missing! "
      "You probably dont have xorg-dev installed on your system. It is possible to recover "
      "by running \"sudo apt install xorg-dev\" (or equivalent on your unix distro)")
  endif()  
endif()
set(GLFW_BUILD_DOCS OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE STRING "" FORCE)
set(GLFW_INSTALL        OFF CACHE STRING "" FORCE)
# set(GLFW_VULKAN_STATIC  ON CACHE STRING "" FORCE)
add_subdirectory(external/glfw)
include_directories(SYSTEM external/glfw/include)
set_property(TARGET glfw PROPERTY FOLDER "external/GLFW3" )

# use GLI
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/gli/CMakeLists.txt")
  message(FATAL_ERROR "The GLI submodule directory is empty! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by running \"git submodule update --init --recursive\"")
endif()
find_package(GLI REQUIRED)
include_directories(SYSTEM ${GLI_INCLUDE_DIR})

# use GLM
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glm/CMakeLists.txt")
  message(FATAL_ERROR "The GLM submodule directory is empty! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by running \"git submodule update --init --recursive\"")
endif()
find_package(GLM 0.9.7 REQUIRED)
include_directories(SYSTEM ${GLM_INCLUDE_DIR})

# geometry generator
add_subdirectory(external/generator)
include_directories(SYSTEM external/generator/include)
set_property(TARGET generator PROPERTY FOLDER "external/generator")

# TetGen 1.4.1 (MIT version)
set(TETGEN_SRC 
  ${CMAKE_CURRENT_SOURCE_DIR}/external/tetgen/tetgen.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/external/tetgen/predicates.cxx
)
set(TETGEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/tetgen)
include_directories(SYSTEM ${TETGEN_INCLUDE_DIR})
add_definitions(-DTETLIBRARY)
add_library(TetGen STATIC ${TETGEN_SRC})
set_property(TARGET TetGen PROPERTY POSITION_INDEPENDENT_CODE ON)

# PBRT Parser 
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/external/pbrt-parser/)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/external/pbrt-parser/pbrtParser/include)
add_subdirectory(external/pbrt-parser EXCLUDE_FROM_ALL) 
set_property(TARGET pbrtParser_static PROPERTY POSITION_INDEPENDENT_CODE ON)

# swig
find_package(SWIG 4.0.1)
if (UNIX AND NOT APPLE)
  if (NOT SWIG_FOUND)
    message(FATAL_ERROR "SWIG 4 is missing! "
      "You probably dont have swig 4 installed on your system. It is possible to recover "
      "by running \"sudo apt install swig\" (or equivalent on your unix distro)")
  endif()  
endif()
include(${SWIG_USE_FILE})

# vulkan
find_package(Vulkan 1.1.97 REQUIRED)

if (NOT DEFINED VULKAN_SDK)
set (VULKAN_SDK $ENV{VULKAN_SDK})
endif()

if(APPLE)
if(NOT DEFINED ENV{VULKAN_SDK})
    message(ERROR "Error: VULKAN_SDK environment variable must be set!")
endif()
endif(APPLE)
include_directories(${Vulkan_INCLUDE_DIRS})

# glslc compiler
find_program(GLSLC glslc PATHS ${VULKAN_SDK}/bin )
find_program(SPIRV_OPT spirv-opt PATHS ${VULKAN_SDK}/bin )
if (${GLSLC} STREQUAL GLSLC-NOTFOUND)
message(ERROR "Error, glslc not found!")
endif()
if (${SPIRV_OPT} STREQUAL SPIRV_OPT-NOTFOUND)
message(ERROR "Error, spirv-opt not found!")
endif()

# if (UNIX AND NOT APPLE)
#   if (NOT DEFINED GLSLC) 
#     message(FATAL_ERROR "the glslc compiler is missing! "
#         "You probably dont have glslc on your system path. It is possible to recover "
#         "by running \n \"wget -qO - http://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -\""
#         "\n \"sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.1.97-bionic.list http://packages.lunarg.com/vulkan/1.1.97/lunarg-vulkan-1.1.97-bionic.list\""
#         "\n \"sudo apt update\""
#         "\n \"sudo apt install lunarg-vulkan-sdk shaderc\""
#         "\n\n OR alternatively download the vulkan sdk tarball from https://vulkan.lunarg.com/sdk/home#linux and add glslc to your system path."
#         )
#   endif()
# endif()


# zeromq
find_package(ZMQ QUIET)
if(${ZMQ_FOUND})
    include_directories(${ZMQ_INCLUDE_DIRS})
else()
    set(ZMQ_LIBRARIES)
endif(${ZMQ_FOUND})

# python
find_package(Python3 3.7 COMPONENTS Development REQUIRED)
if (UNIX AND NOT APPLE)
  if (NOT Python3_FOUND)
    message(FATAL_ERROR "Python 3 development libraries missing!"
      "You probably dont have python 3.8 or later development libraries installed on your system. It is possible to recover "
      "by running \"sudo apt install python3-dev\" (or equivalent on your unix distro)")
  endif()  
endif()
include_directories(SYSTEM ${Python3_INCLUDE_DIRS})

# tiny
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/external/tiny)

# openvr
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/openvr/CMakeLists.txt")
  message(FATAL_ERROR "The OpenVR submodule directory is empty! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by running \"git submodule update --init --recursive\"")
endif()
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/openvr)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/external/openvr/headers)

# bullet 
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/bullet3)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/external/bullet3/src)
set_property(TARGET LinearMath PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET BulletDynamics PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET BulletCollision PROPERTY POSITION_INDEPENDENT_CODE ON)

# spacemouse
# Todo: make a cmake file for this
if(BUILD_SPACEMOUSE)
    set(SPACEMOUSE_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/external/spacemouse/Lib/x64/siapp.lib")
    set(SPACEMOUSE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/spacemouse/Inc")
    include_directories(SYSTEM ${SPACEMOUSE_INCLUDE_DIR})
endif(BUILD_SPACEMOUSE)

# google test
if(BUILD_TESTS)
    # Download and unpack googletest at configure time
    configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
    if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
    if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif()

    # Prevent overriding the parent project's compiler/linker
    # settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                    ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                    EXCLUDE_FROM_ALL)

    # The gtest/gtest_main targets carry header search path
    # dependencies automatically when using CMake 2.8.11 or
    # later. Otherwise we have to add them here ourselves.
    if (CMAKE_VERSION VERSION_LESS 2.8.11)
    include_directories("${gtest_SOURCE_DIR}/include")
    endif()
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
    TetGen
    BulletSoftBody
    BulletDynamics
    BulletCollision
    LinearMath
    pbrtParser_static
)

# Name of OpenVR target seems to change...
if (MSVC)
set(LIBRARIES ${LIBRARIES} openvr_api64)
else(MSVC)
set(LIBRARIES ${LIBRARIES} openvr_api)
endif(MSVC)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Add source files                                                │
# └──────────────────────────────────────────────────────────────────┘
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(Foton)

