make_bin(
    NAME
        test_glfw
    DEPENDS
        hal_window
        hal_filesystem
        stb
)

target_compile_definitions(test_glfw PRIVATE WORK_DIR="${CMAKE_CURRENT_LIST_DIR}")