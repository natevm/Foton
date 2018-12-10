# - Try to find ZMQ
# Once done this will define
# ZMQ_FOUND - System has ZMQ
# ZMQ_INCLUDE_DIRS - The ZMQ include directories
# ZMQ_LIBRARIES - The libraries needed to use ZMQ
# ZMQ_DEFINITIONS - Compiler switches required for using ZMQ

set ( ZMQ_LIBRARIES ${PROJECT_SOURCE_DIR}/external/libzmq/lib/libzmq-v141-mt-4_3_1.lib)
set ( ZMQ_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/external/libzmq/include)
