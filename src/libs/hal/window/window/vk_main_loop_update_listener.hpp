
#pragma once

#include <window/window.hpp>

#include <render/vk/vulkan_dependencies.hpp>

namespace sandbox::hal
{
    class vk_main_loop_update_listener : public window::main_loop_update_listener
    {
    public:
        ~vk_main_loop_update_listener() override = default;

        virtual void on_swapchain_reset(size_t width, size_t height) = 0;

        virtual vk::Image get_present_image() const = 0;
        virtual vk::ImageLayout get_present_image_layout() const = 0;
        virtual vk::Extent2D get_present_image_size() const = 0;
        virtual std::vector<vk::Semaphore> get_wait_semaphores() const = 0;
    };
} // namespace sandbox::hal
