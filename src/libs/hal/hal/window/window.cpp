
#include "glfw_window.hpp"

#include <hal/render/vk/raii.hpp>
#include <hal/render/vk/context.hpp>

#include <GLFW/glfw3.h>

using namespace sandbox::hal;
using namespace sandbox::hal::render;

namespace
{
    class glfw_context
    {
    public:
        glfw_context()
        {
            glfwInit();
            if (!glfwVulkanSupported()) {
                throw std::runtime_error("Vulkan unsupported.");
            }

            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
            glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        }
    };
}

sandbox::hal::glfw_window::glfw_window(size_t width, size_t height, const std::string& name)
{
    static glfw_context ctx{};
    m_window = winow_hanlder{glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr), glfwDestroyWindow};

    uint32_t ext_count{0};
    auto ext_names = glfwGetRequiredInstanceExtensions(&ext_count);

    avk::context::init_instance({
        .extensions = {
            .names = ext_names,
            .count = ext_count
        }
    });

    avk::create_surface({}, [this]() -> vk::SurfaceKHR {
        VkSurfaceKHR surface;
        glfwCreateWindowSurface(*avk::context::instance(), m_window.get(), nullptr, &surface);
        return {surface};
    });

    avk::context::init_device({

    });
}


void sandbox::hal::glfw_window::main_loop()
{
    glfwPollEvents();
}

bool sandbox::hal::glfw_window::closed() const
{
    return glfwWindowShouldClose(m_window.get());
}

sandbox::hal::glfw_window::~glfw_window() = default;
