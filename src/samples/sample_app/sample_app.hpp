#pragma once

#include <window/window.hpp>
#include <render/vk/vulkan_dependencies.hpp>

#include <optional>

namespace sandbox
{
    class sample_app
    {
    public:
        explicit sample_app(
            uint32_t init_window_width = 800,
            uint32_t init_window_height = 600,
            const std::string& name = "sample_app");

        void main_loop();

    protected:
        virtual void update(uint64_t);

        std::pair<uint32_t, uint32_t> get_window_framebuffer_size() const;

        virtual void create_render_passes() = 0;
        virtual void create_framebuffers(uint32_t width, uint32_t height) = 0;
        virtual void create_sync_primitives() = 0;
        virtual void init_assets() = 0;

        virtual vk::Image get_final_image() = 0;
        virtual const std::vector<vk::Semaphore>& get_wait_semaphores() = 0;
        virtual const std::vector<vk::Fence>& get_wait_fences() = 0;

    private:
        class sample_app_window_update_listener;
        friend class sample_app_window_update_listener;

        std::optional<hal::window> m_window{};

        std::string m_name{};

        uint32_t m_framebuffer_width{800};
        uint32_t m_framebuffer_height{600};
    };
} // namespace sandbox
