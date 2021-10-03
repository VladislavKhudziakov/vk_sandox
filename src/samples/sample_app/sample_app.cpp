

#include "sample_app.hpp"

#include <render/vk/context.hpp>
#include <render/vk/errors_handling.hpp>
#include <window/vk_main_loop_update_listener.hpp>

using namespace sandbox;
using namespace sandbox::hal::render;

class sample_app::sample_app_window_update_listener : public hal::vk_main_loop_update_listener
{
public:
    explicit sample_app_window_update_listener(sample_app* app)
        : m_app(app)
    {
    }

    ~sample_app_window_update_listener() override = default;

    void on_swapchain_reset(size_t width, size_t height) override
    {
        width = std::max(width, size_t(1));
        height = std::max(height, size_t(1));
        m_app->m_framebuffer_width = width;
        m_app->m_framebuffer_height = height;
        m_app->create_framebuffers(width, height);
    }

    vk::Image get_present_image() const override
    {
        return m_app->get_final_image();
    }


    vk::ImageLayout get_present_image_layout() const override
    {
        return vk::ImageLayout::eTransferSrcOptimal;
    }


    vk::Extent2D get_present_image_size() const override
    {
        return {
            m_app->m_framebuffer_width,
            m_app->m_framebuffer_height};
    }


    std::vector<vk::Semaphore> get_wait_semaphores() const override
    {
        return m_app->get_wait_semaphores();
    }


    void on_window_initialized() override
    {
        m_app->create_render_passes();
        m_app->create_sync_primitives();
        m_app->init_assets();
    }


    void update(uint64_t dt) override
    {
        m_app->update(dt);
    }

private:
    sample_app* m_app;
};


sample_app::sample_app(uint32_t init_window_width, uint32_t init_window_height, const std::string& name)
    : m_name(name)
    , m_framebuffer_width(init_window_width)
    , m_framebuffer_height(init_window_height)
{
}


void sandbox::sample_app::update(uint64_t dt)
{
    const auto& fences = get_wait_fences();
    if (!fences.empty()) {
        VK_CALL(avk::context::device()->waitForFences(fences, VK_TRUE, UINT64_MAX));
        avk::context::device()->resetFences(fences);
    }
}


void sandbox::sample_app::main_loop()
{
    m_window.emplace(m_framebuffer_width, m_framebuffer_height, m_name, std::make_unique<sample_app_window_update_listener>(this));

    while (!m_window->closed()) {
        m_window->main_loop();
    }

    avk::context::device()->waitIdle();
}


std::pair<uint32_t, uint32_t> sandbox::sample_app::get_window_framebuffer_size() const
{
    return std::make_pair(m_framebuffer_width, m_framebuffer_height);
}
