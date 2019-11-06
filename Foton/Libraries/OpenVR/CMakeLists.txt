message("Adding subdirectory: OpenVR")

set(
    OpenVR_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/OpenVR.hxx
    PARENT_SCOPE
)

set(
    OpenVR_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/OpenVR.cxx
    PARENT_SCOPE
)