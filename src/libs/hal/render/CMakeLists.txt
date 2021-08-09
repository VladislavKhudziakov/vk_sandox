find_package(Vulkan REQUIRED)

make_bin(
    NAME
        hal_render
    LIB
    LIB_TYPE
        STATIC
    DEPENDS
        VulkanMemoryAllocator
        Vulkan::Vulkan
        spdlog
)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_compile_definitions(hal_render PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_compile_definitions(hal_render PRIVATE VK_USE_PLATFORM_XCB_KHR)
endif()

target_include_directories(hal_render PUBLIC .)