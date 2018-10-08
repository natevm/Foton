message("Adding subdirectory: EventSystem")

set (EventSystem_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/EventSystem.hxx
    PARENT_SCOPE
)

set(
    EventSystem_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/EventSystem.cxx
    PARENT_SCOPE
)