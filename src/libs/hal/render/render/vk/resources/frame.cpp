#include "frame.hpp"

#include <render/vk/context.hpp>
#include <render/vk/errors_handling.hpp>
#include <render/vk/resources/pass.hpp>

#include <utils/conditions_helpers.hpp>

using namespace sandbox::hal::render;

class avk::frames_pool::swapchain_handler
{
public:
    swapchain_handler(uint32_t desired_swapchain_images, vk::Extent2D init_extent, const vk::SurfaceKHR& surface)
        : m_desired_images_count(desired_swapchain_images)
    {
        reset(init_extent, surface);
    }

    vk::SwapchainKHR swapchain()
    {
        return m_swapchain;
    }
    
    uint32_t get_images_count() const
    {
        return m_images_count;
    }

    uint32_t get_current_image() const
    {
        return m_curr_image;
    }

    void set_swapcain_reset_listener(const std::function<void(size_t, size_t)>& listener)
    {
        m_swapchain_reset_listener = listener;
    }

    void reset(vk::Extent2D desired_extent, const vk::SurfaceKHR& surface)
    {
        avk::context::queue(vk::QueueFlagBits::eGraphics, 0).waitIdle();

        vk::SurfaceCapabilitiesKHR capabilities = avk::context::gpu()->getSurfaceCapabilitiesKHR(surface);

        vk::SurfaceFormatKHR surface_format = get_surface_format(surface);
        vk::PresentModeKHR present_mode = get_surface_present_mode(surface);
            
        m_size = get_surface_extent(desired_extent, capabilities);

        m_images_count = get_surface_images_count(capabilities);

        uint32_t present_queue_family = avk::context::queue_family(vk::QueueFlagBits::eGraphics);

        vk::SwapchainCreateInfoKHR swapchain_info{
            .flags = {},
            .surface = surface,
            .minImageCount = m_images_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = m_size,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &present_queue_family,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = get_surface_composite_alpha(capabilities),
            .presentMode = present_mode,
            .clipped = VK_FALSE,
            .oldSwapchain = m_swapchain,
        };

        m_swapchain = avk::create_swapchain(avk::context::device()->createSwapchainKHR(swapchain_info));

        m_swapchain_images = avk::context::device()->getSwapchainImagesKHR(m_swapchain);
        assert(m_images_count == m_swapchain_images.size());

        if (m_wait_fences.size() != m_images_count) {
            m_wait_fences.resize(m_images_count);
        }

        if (m_wait_semaphores.size() != m_images_count) {
            m_wait_semaphores.resize(m_images_count);
        }

        if (m_destroy_resources_callbacks.size() != m_images_count) {
            m_destroy_resources_callbacks.resize(m_images_count);
        }

        if (m_swapchain_reset_listener) {
            m_swapchain_reset_listener(m_size.width, m_size.height);
        }
    }

    const std::vector<vk::Image>& swapchain_images() const
    {
        return m_swapchain_images;
    }

    std::tuple<vk::Semaphore, vk::Fence, uint32_t> acuire_next_image(vk::Extent2D desired_extent, const vk::SurfaceKHR& surface)
    {
        vk::Semaphore curr_wait_semaphore{};
        if (!m_wait_semaphores[m_curr_image]) {
            m_wait_semaphores[m_curr_image] = avk::create_semaphore(avk::context::device()->createSemaphore({}));
        }

        curr_wait_semaphore = m_wait_semaphores[m_curr_image];

        vk::ResultValue<uint32_t> acquire_result{vk::Result::eSuccess, static_cast<uint32_t>(-1)};

        do {
            if (acquire_result.result == vk::Result::eSuboptimalKHR || acquire_result.result == vk::Result::eErrorOutOfDateKHR) {
                reset(desired_extent, surface);
            } else if (acquire_result.result != vk::Result::eSuccess) {
                throw avk::vulkan_result_error(acquire_result.result);
            }
            acquire_result = avk::context::device()->acquireNextImageKHR(m_swapchain, UINT64_MAX, m_wait_semaphores[m_curr_image]);
        } while (acquire_result.result != vk::Result::eSuccess);

        m_curr_image = acquire_result.value;

        if (!static_cast<bool>(m_wait_fences[m_curr_image])) {
            m_wait_fences[m_curr_image] = avk::create_fence(avk::context::device()->createFence({}));
        } else {
            VK_CALL(avk::context::device()->waitForFences({m_wait_fences[m_curr_image]}, VK_TRUE, UINT64_MAX));
            avk::context::device()->resetFences({m_wait_fences[m_curr_image]});
            if (auto& dtor = m_destroy_resources_callbacks[m_curr_image]) {
                dtor();
            }
        }

        return {curr_wait_semaphore, m_wait_fences[m_curr_image], acquire_result.value};
    }

    void present(
        vk::Extent2D desired_extent,
        const vk::SurfaceKHR& surface,
        vk::Semaphore* wait_semaphores,
        uint32_t wait_semaphores_count,
        uint32_t present_queue_index,
        const std::function<void()>& destroy_resources_callback)
    {
        vk::Result present_result = vk::Result::eSuccess;

        m_destroy_resources_callbacks[m_curr_image] = destroy_resources_callback;

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = wait_semaphores_count,
            .pWaitSemaphores = wait_semaphores,
            .swapchainCount = 1,
            .pSwapchains = m_swapchain.handler_ptr(),
            .pImageIndices = &m_curr_image,
            .pResults = &present_result,
        };

        try {
            VK_CALL(avk::context::queue(vk::QueueFlagBits::eGraphics, present_queue_index).presentKHR(presentInfo));
        } catch (const vk::OutOfDateKHRError& error) {
            present_result = vk::Result::eSuccess;
            reset(desired_extent, surface);
        } catch (...) {
            throw;
        }

        if (present_result == vk::Result::eSuboptimalKHR || present_result == vk::Result::eErrorOutOfDateKHR) {
            reset(desired_extent, surface);
        }
    }

    vk::Extent2D get_size() const
    {
        return m_size;
    }

private:
    vk::SurfaceFormatKHR get_surface_format(const vk::SurfaceKHR& surface)
    {
        std::vector<vk::SurfaceFormatKHR> sf_formats = avk::context::gpu()->getSurfaceFormatsKHR(surface);

        for (const auto& sf_format : sf_formats) {
            auto is_srgb_fmt =
                sf_format.format == vk::Format::eR8G8B8A8Srgb || sf_format.format == vk::Format::eB8G8R8A8Srgb;

            if (is_srgb_fmt) {
                return sf_format;
            }
        }

        return sf_formats.front();
    }


    vk::PresentModeKHR get_surface_present_mode(const vk::SurfaceKHR& surface)
    {
        std::vector<vk::PresentModeKHR> modes = avk::context::gpu()->getSurfacePresentModesKHR(surface);

        if (std::find(modes.begin(), modes.end(), vk::PresentModeKHR::eMailbox) != modes.end()) {
            return vk::PresentModeKHR::eMailbox;
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D get_surface_extent(vk::Extent2D desired_extent, const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != 0xFFFFFFFF && capabilities.currentExtent.height != 0xFFFFFFFF) {
            return capabilities.currentExtent;
        }

        desired_extent.width = std::clamp(static_cast<uint32_t>(desired_extent.width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        desired_extent.height = std::clamp(static_cast<uint32_t>(desired_extent.height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return {
            .width = static_cast<uint32_t>(desired_extent.width),
            .height = static_cast<uint32_t>(desired_extent.height)};
    }

    uint32_t get_surface_images_count(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        const auto curr_images_count = std::clamp(m_desired_images_count, capabilities.minImageCount + 1, capabilities.maxImageCount);
        return std::clamp(curr_images_count, capabilities.minImageCount, capabilities.maxImageCount);
    }

    vk::CompositeAlphaFlagBitsKHR get_surface_composite_alpha(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque) {
            return vk::CompositeAlphaFlagBitsKHR::eOpaque;
        }

        if (capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) {
            return vk::CompositeAlphaFlagBitsKHR::eInherit;
        }

        if (capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) {
            return vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
        }

        if (capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied) {
            return vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
        }

        throw std::runtime_error("Cannot find surface compatible composite alpha.");
    }

    std::vector<vk::Image> m_swapchain_images{};
    vk::Extent2D m_size{};
    avk::swapchain m_swapchain{};
    uint32_t m_desired_images_count{0};
    uint32_t m_images_count{0};
    uint32_t m_curr_image{0};
    std::vector<avk::fence> m_wait_fences{};
    std::vector<avk::semaphore> m_wait_semaphores{};
    std::vector<std::function<void()>> m_destroy_resources_callbacks{};

    std::function<void(size_t, size_t)> m_swapchain_reset_listener{};
};


void avk::frame::write_commands(const callback_type& callback)
{
    switch (m_command_buffer_state) {
        case command_buffer_state::init:
            [[fallthrough]];
        case command_buffer_state::write_primary:
            callback(m_main_command_buffer->front());
            break;
        case command_buffer_state::write_secondary: 
        {
            CHECK(!m_passes_infos.empty());
            auto& curr_pass_info = m_passes_infos.back();

            curr_pass_info.command_buffers.emplace_back() = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
                .commandPool = m_command_pool,
                .level = vk::CommandBufferLevel::eSecondary,
                .commandBufferCount = 1});

            curr_pass_info.command_buffers_native.emplace_back() = curr_pass_info.command_buffers.back()->back();

            curr_pass_info.command_buffers_native.back().begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eRenderPassContinue,
                .pInheritanceInfo = &curr_pass_info.inheritance_info});

            callback(curr_pass_info.command_buffers_native.back());

            curr_pass_info.command_buffers_native.back().end();
        }
            break;
        default:
            CHECK(false);
            break;
    }
}


void avk::frame::begin()
{
    CHECK(m_command_buffer_state == command_buffer_state::submitted);
    CHECK(!m_main_command_buffer);
    CHECK(m_passes_infos.empty());
    m_command_buffer_state = command_buffer_state::init;

    m_main_command_buffer = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
        .commandPool = m_command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1});
}


void avk::frame::start_pass(avk::render_pass_instance& pass)
{    
    m_passes_infos.emplace_back(pass_info{
        .inheritance_info = vk::CommandBufferInheritanceInfo{
            .renderPass = pass,
            .subpass = 0,
            .framebuffer = pass.get_framebuffer(),
            .occlusionQueryEnable = VK_FALSE,
            .queryFlags = {}
        }
    });

    pass.begin(m_main_command_buffer->front(), vk::SubpassContents::eSecondaryCommandBuffers); 

    CHECK(m_command_buffer_state != command_buffer_state::write_secondary);
    m_command_buffer_state = command_buffer_state::write_secondary;
}


void avk::frame::finish_pass()
{
    CHECK(!m_passes_infos.empty());
    CHECK(m_command_buffer_state == command_buffer_state::write_secondary);

    if (!m_passes_infos.back().command_buffers_native.empty()) {
        m_main_command_buffer->front().executeCommands(m_passes_infos.back().command_buffers_native);
    }

    m_main_command_buffer->front().endRenderPass();

    m_command_buffer_state = command_buffer_state::write_primary;
}


void avk::frame::submit()
{
    CHECK(m_command_buffer_state == command_buffer_state::write_primary);

    m_main_command_buffer->front().end();

    auto queue = avk::context::queue(vk::QueueFlagBits::eGraphics);
    
    if (!m_wait_semaphore) {
        m_wait_semaphore = avk::create_semaphore(avk::context::device()->createSemaphore(vk::SemaphoreCreateInfo{}));
    }

    auto wait_semaphore = m_wait_semaphore.as<vk::Semaphore>();

    avk::context::queue(vk::QueueFlagBits::eGraphics, 0)
    .submit(
        vk::SubmitInfo{
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = m_main_command_buffer->data(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &wait_semaphore,
        },
        m_wait_fence);

    if (m_submit_callback) {
        auto wait_semaphore = m_wait_semaphore.as<vk::Semaphore>();

        m_submit_callback(&wait_semaphore, 1, [this]() {
            m_passes_infos.clear();
            m_main_command_buffer = {};
        });
    }

    m_passes_infos.clear();

    m_command_buffer_state = command_buffer_state::submitted;
}


void avk::frame::acquire(vk::Fence wait_fence, submit_callback_type submit_callback)
{
    m_submit_callback = std::move(submit_callback);
    m_wait_fence = wait_fence;
}


avk::frames_pool::frames_pool(uint32_t desired_frames_in_flight_count, uint32_t present_queue, vk::Extent2D init_extent, const vk::SurfaceKHR& surface)
    : m_swapchain(std::make_unique<swapchain_handler>(desired_frames_in_flight_count, init_extent, m_surface))
    , m_current_extent(init_extent)
    , m_surface(surface)
    , m_present_queue(present_queue)
{
    m_frames.reserve(m_swapchain->get_images_count());

    for (int32_t i = 0; i < m_swapchain->get_images_count(); ++i) {
        auto& new_frame = m_frames.emplace_back();
        new_frame.m_command_pool = m_command_pool.as<vk::CommandPool>();
    }
}


avk::frames_pool::~frames_pool() = default;


avk::frame* avk::frames_pool::new_frame()
{
    auto [curr_semaphore, curr_fence, error_code] = m_swapchain->acuire_next_image(m_current_extent, m_surface);
    CHECK(error_code == VK_SUCCESS);
    
    auto& curr_frame = m_frames[m_swapchain->get_current_image()];

    curr_frame.acquire(curr_fence, [curr_semaphore, this](vk::Semaphore* wait_semaphores, uint32_t wait_semaphores_count, const std::function<void()>& destroy_callback) {
        m_wait_semaphores.clear();
        m_wait_semaphores.push_back(curr_semaphore);
        std::copy(wait_semaphores, wait_semaphores + wait_semaphores_count, std::back_inserter(m_wait_semaphores));
        m_swapchain->present(m_current_extent, m_surface, m_wait_semaphores.data(), m_wait_semaphores.size(), m_present_queue, destroy_callback);    
    });

    return &curr_frame;
}