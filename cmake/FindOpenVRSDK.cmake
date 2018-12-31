# Find the OpenVR SDK
# NOTE: there is no default installation path as the code needs to be build
# This module defines:
# OPENVR_SDK_FOUND, if false do not try to link against the Open VR SDK
# OPENVR_SDK_LIBRARY, the name of the Open VR SDK library to link against
# OPENVR_SDK_INCLUDE_DIR, the Open VR SDK include directory
#
# You can also specify the environment variable OPENVR_SDK or define it with
# -DOPENVR_SDK=... to hint at the module where to search for the Open VR SDK if it's
# installed in a non-standard location.

# CMake is too stupid to figure out the path!?
find_path(OPENVR_SDK_INCLUDE_DIR openvr.h
	HINTS
	$ENV{OPENVR_SDK}
	${OPENVR_SDK}
	PATH_SUFFIXES headers/
	# TODO: Unsure on handling of the possible default install locations
	PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local/include/
	/usr/include/
	/sw # Fink
	/opt/local # DarwinPorts
	/opt/csw # Blastwave
	/opt
)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
	if (UNIX OR MINGW)
		set(LIB_PATH_SUFFIX "lib/linux64/")
	elseif (MSVC)
		set(LIB_PATH_SUFFIX "lib/win64/")
	else()
		message(ERROR "Error: Unsupported 64 bit configuration")
	endif()
else()
	if (UNIX OR MINGW)
		set(LIB_PATH_SUFFIX "lib/linux32/")
	elseif (MSVC)
		set(LIB_PATH_SUFFIX "lib/win32/")
	elseif(APPLE)
		set(LIB_PATH_SUFFIX "lib/osx32/")
	else()
		message(ERROR "Error: Unsupported 32 bit configuration")
	endif()
endif()

find_library(OPENVR_SDK_LIBRARY_TMP NAMES openvr_api openr_api.lib
	HINTS
	$ENV{OPENVR_SDK}
	${OPENVR_SDK}
	PATH_SUFFIXES ${LIB_PATH_SUFFIX}
	# TODO: I don't know if these will be correct if people have installed
	# the library on to their system instead of just using the git repo or w/e
	PATHS
	/sw
	/opt/local
	/opt/csw
	/opt
)

set(OPENVR_SDK_FOUND FALSE)
if (OPENVR_SDK_LIBRARY_TMP AND OPENVR_SDK_INCLUDE_DIR)
	set(OPENVR_SDK_LIBRARY ${OPENVR_SDK_LIBRARY_TMP} CACHE STRING "Which OpenVR library to link against")
	set(OPENVR_SDK_LIBRARY_TMP ${OPENVR_SDK_LIBRARY_TMP} CACHE INTERNAL "")
	set(OPENVR_SDK_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenVRSDK REQUIRED_VARS OPENVR_SDK_LIBRARY OPENVR_SDK_INCLUDE_DIR)

