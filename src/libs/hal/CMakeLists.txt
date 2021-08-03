
find_package(Vulkan REQUIRED)

make_bin(
    NAME
        hal
    LIB
    LIB_TYPE
    STATIC
    DEPENDS
        glfw
        spdlog
        Vulkan::Vulkan
)
target_compile_definitions(hal PRIVATE VK_USE_PLATFORM_XCB_KHR)

