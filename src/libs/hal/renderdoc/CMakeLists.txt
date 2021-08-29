
make_bin(
    NAME
        hal_renderdoc
    LIB
    LIB_TYPE
        STATIC
    DEPENDS
        renderdoc
        hal_dll
)

target_include_directories(hal_renderdoc PUBLIC ${CMAKE_CURRENT_LIST_DIR})