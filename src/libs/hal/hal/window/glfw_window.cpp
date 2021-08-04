#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <hal/window/window.hpp>

#include <hal/render/vk/context.hpp>


using namespace sandbox::hal;
using namespace sandbox::hal::render;

namespace
{
    class window_context_init_raii
    {
    public:
        window_context_init_raii()
        {
            glfwInit();
            if (!glfwVulkanSupported()) {
                throw std::runtime_error("Vulkan unsupported.");
            }

            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        ~window_context_init_raii()
        {
            avk::context::deinit();
        }
    };

    class glw_window : public sandbox::hal::window::impl
    {
    public:
        glw_window(size_t width, size_t height, const std::string& name)
        {
            static std::weak_ptr<window_context_init_raii> window_context{};
            if (auto curr_window_context = window_context.lock(); curr_window_context == nullptr) {
                m_window_ctx = std::make_shared<window_context_init_raii>();
                window_context = m_window_ctx;
            } else {
                m_window_ctx = curr_window_context;
            }

            m_window_handler.reset(glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr));

            uint32_t ext_count{0};
            auto ext_names = glfwGetRequiredInstanceExtensions(&ext_count);

            avk::context::init_instance({
                .extensions = {
                    .names = ext_names,
                    .count = 2
                }
            });

            m_surface = avk::create_surface(*avk::context::instance(), [this]() -> vk::SurfaceKHR {
                VkSurfaceKHR surface;
                auto res = glfwCreateWindowSurface(static_cast<VkInstance>(*avk::context::instance()), m_window_handler.get(), nullptr, &surface);
                assert(res == VK_SUCCESS);
                assert(surface != VK_NULL_HANDLE);
                return vk::SurfaceKHR{surface};
            });

            avk::context::init_device({
                .required_supported_surfaces = m_surface,
                .required_supported_surfaces_count = 1
            });

            std::vector<vk::SurfaceFormatKHR> formats = avk::context::gpu()->getSurfaceFormatsKHR(m_surface);
            vk::SurfaceCapabilitiesKHR capabilities = avk::context::gpu()->getSurfaceCapabilitiesKHR(m_surface);

            vk::SwapchainCreateInfoKHR swapchain_info{
                .flags = {},
            };
        }

        bool closed() override
        {
            return glfwWindowShouldClose(m_window_handler.get());
        }

        void main_loop() override
        {
            glfwPollEvents();
        }

    private:
        std::unique_ptr<GLFWwindow, void(*)(GLFWwindow* window)> m_window_handler{nullptr, glfwDestroyWindow};
        std::shared_ptr<window_context_init_raii> m_window_ctx{};
        avk::surface m_surface{};
    };
}


std::unique_ptr<window::impl> sandbox::hal::window::impl::create(size_t width, size_t height, const std::string& name)
{
    return std::make_unique<glw_window>(width, height, name);
}