add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glm)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glfw)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/spdlog)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/VulkanMemoryAllocator)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/stb)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/json)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/renderdoc)

target_compile_definitions(spdlog PUBLIC "-DNOMINMAX")
