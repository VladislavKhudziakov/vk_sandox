make_bin(
    NAME
        test_gltf
    DEPENDS
        hal_window
        hal_filesystem
        samples_utils
        gltf
        hal_renderdoc
    RESOURCES_EXTENSIONS
        .glb
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})