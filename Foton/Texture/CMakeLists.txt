message("Adding subdirectory: Texture")

set(
    Texture_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/Texture.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/TextureStruct.hxx
    PARENT_SCOPE
)

set (
    Texture_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Texture.cxx
    PARENT_SCOPE
)
