message("Adding subdirectory: Importers")

set(
    Importers_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Importers.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/PBRT.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/OBJ.hxx
    PARENT_SCOPE
)

set (
    Importers_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/PBRT.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/OBJ.cxx
    PARENT_SCOPE
)
