# - Find JPEGTURBO
# Find the libjpeg-turbo includes and library
# This module defines
#  JPEGTURBO_INCLUDE_DIR, where to find jpeglib.h and turbojpeg.h, etc.
#  JPEGTURBO_LIBRARIES, the libraries needed to use libjpeg-turbo.
#  JPEGTURBO_FOUND, If false, do not try to use libjpeg-turbo.
# also defined, but not for general use are
#  JPEGTURBO_LIBRARY, where to find the libjpeg-turbo library.

#=============================================================================
# Copyright 2001-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

#FIND_PATH(JPEGTURBO_INCLUDE_DIR turbojpeg.h)



FIND_PATH(JPEGTURBO_INCLUDE_DIR "jpeglib.h"
  HINTS ${JPEGTURBO_PREFIX}/include
  PATHS
  $ENV{JPEGTURBO_HOME}/include
  $ENV{EXTERNLIBS}/libjpeg-turbo64/include
  $ENV{EXTERNLIBS}/libjpeg-turbo/include
  ~/Library/Frameworks/include
  /Library/Frameworks/include
  /usr/local/opt/jpeg-turbo/include
  /usr/local/include
  /usr/include
  /sw/include # Fink
  /opt/local/include # DarwinPorts
  /opt/csw/include # Blastwave
  /opt/include
  DOC "JPEGTURBO - Headers"
)

# FIND_PATH(JPEGTURBO_INCLUDE_DIR_INT "jpegint.h"
#    PATHS ${JPEGTURBO_INCLUDE_DIR}
#    DOC "JPEGTURBO - Internal Headers"
# )

#FIND_LIBRARY(TURBOJPEG_LIBRARY NAMES turbojpeg)
#FIND_LIBRARY(JPEGTURBO_LIBRARY NAMES jpeg)

FIND_LIBRARY(JPEGTURBO_LIBRARY NAMES jpeg
  HINTS ${JPEGTURBO_PREFIX}/lib ${JPEGTURBO_PREFIX}/lib64
  PATHS
  $ENV{JPEGTURBO_HOME}
  $ENV{EXTERNLIBS}/libjpeg-turbo64
  $ENV{EXTERNLIBS}/libjpeg-turbo
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr/local/opt/jpeg-turbo
  /usr
  /sw
  /opt/local
  /opt/csw
  /opt
  PATH_SUFFIXES lib lib64
  DOC "JPEGTURBO - Library"
)

