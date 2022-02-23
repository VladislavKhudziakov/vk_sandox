#pragma once

#include <render/vk/raii.hpp>

#include <functional>
#include <vector>
#include <memory>

namespace sandbox::hal::render::avk
{
    class render_pass_instance;
    class pipeline_instance;
    class frames_pool;

    class frame
    {
        friend class avk::frames_pool;
    public:
        using callback_type = std::function<void(vk::CommandBuffer& buffer)>;

        frame() = default;
        frame(const frame&) = default;
        frame(frame&&) noexcept = default;

        frame& operator=(const frame&) = default;
        frame& operator=(frame&&) noexcept = default;

        void begin();
        void start_pass(avk::render_pass_instance& pass);
        void finish_pass();
        void write_commands(const callback_type& callback);
        void submit();

    private:
        using destroy_resources_callback = std::function<void()>;
        using submit_callback_type = std::function<void(vk::Semaphore*, uint32_t, const destroy_resources_callback&)>;

        struct pass_info
        {
            vk::CommandBufferInheritanceInfo inheritance_info{};
            std::vector<avk::command_buffer_list> command_buffers{};
            std::vector<vk::CommandBuffer> command_buffers_native{};
        };

        enum class command_buffer_state
        {
            init,
            write_primary,
            write_secondary,
            submitted
        };

        void acquire(vk::Fence, submit_callback_type submit_calllback);

        std::vector<pass_info> m_passes_infos{};
        avk::command_buffer_list m_main_command_buffer{};
        vk::CommandPool m_command_pool{};
        avk::semaphore m_wait_semaphore{};
        vk::Fence m_wait_fence{};
        submit_callback_type m_submit_callback{};
        command_buffer_state m_command_buffer_state{command_buffer_state::init};
    };


    class frames_pool
    {
    public:
        frames_pool(uint32_t desired_frames_in_flight_count, uint32_t present_queue, vk::Extent2D init_extent, const vk::SurfaceKHR& surface);
        ~frames_pool();

        frame* new_frame();

    private:
        class swapchain_handler;

        std::unique_ptr<swapchain_handler> m_swapchain{};
        std::vector<frame> m_frames{};
        std::vector<vk::Semaphore> m_wait_semaphores{};
        avk::command_pool m_command_pool{};
        vk::Extent2D m_current_extent{};
        vk::SurfaceKHR m_surface{};
        uint32_t m_current_frame{0};
        uint32_t m_present_queue{0};
    };
}
