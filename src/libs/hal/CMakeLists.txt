
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

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_compile_definitions(hal PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_compile_definitions(hal PRIVATE VK_USE_PLATFORM_XCB_KHR)
endif()