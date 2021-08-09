add_library(
    stb
    STATIC
    ${CMAKE_CURRENT_LIST_DIR}/stb_image.c)

target_include_directories(stb PUBLIC ${CMAKE_CURRENT_LIST_DIR})