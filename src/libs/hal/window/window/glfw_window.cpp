#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <window/window.hpp>
#include <window/vk_main_loop_update_listener.hpp>

#include <render/vk/raii.hpp>
#include <render/vk/errors_handling.hpp>

using namespace sandbox::hal;
using namespace sandbox::hal::render;

namespace
{
    class swapchain_handler
    {
    public:
        vk::SwapchainKHR swapchain()
        {
            return m_swapchain;
        }

        uint32_t images_count() const
        {
            return m_images_count;
        }

        uint32_t current_image() const
        {
            return m_curr_image;
        }

        void set_swapcain_reset_listener(const std::function<void(size_t, size_t)>& listener)
        {
            m_swapchain_reset_listener = listener;
        }

        void reset(GLFWwindow* window, const vk::SurfaceKHR& surface)
        {
            avk::context::queue(vk::QueueFlagBits::eGraphics, 0).waitIdle();

            vk::SurfaceCapabilitiesKHR capabilities = avk::context::gpu()->getSurfaceCapabilitiesKHR(surface);

            vk::SurfaceFormatKHR surface_format = get_surface_format(surface);
            vk::PresentModeKHR present_mode = get_surface_present_mode(surface);
            m_size = get_surface_extent(window, capabilities);

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

            if (m_swapchain_reset_listener) {
                m_swapchain_reset_listener(m_size.width, m_size.height);
            }
        }

        const std::vector<vk::Image>& swapchain_images() const
        {
            return m_swapchain_images;
        }

        std::tuple<vk::Semaphore, vk::Fence, uint32_t> request_next_image(GLFWwindow* window, const vk::SurfaceKHR& surface)
        {
            vk::Semaphore curr_wait_semaphore{};
            if (!m_wait_semaphores[m_curr_image]) {
                m_wait_semaphores[m_curr_image] = avk::create_semaphore(avk::context::device()->createSemaphore({}));
            }

            curr_wait_semaphore = m_wait_semaphores[m_curr_image];

            vk::ResultValue<uint32_t> acquire_result{vk::Result::eSuccess, static_cast<uint32_t>(-1)};

            do {
                if (acquire_result.result == vk::Result::eSuboptimalKHR || acquire_result.result == vk::Result::eErrorOutOfDateKHR) {
                    reset(window, surface);
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
            }

            return {curr_wait_semaphore, m_wait_fences[m_curr_image], acquire_result.value};
        }

        void present(
            GLFWwindow* window,
            const vk::SurfaceKHR& surface,
            vk::Semaphore* wait_semaphores,
            uint32_t wait_semaphores_count,
            uint32_t present_queue_index)
        {
            vk::Result present_result = vk::Result::eSuccess;

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
                reset(window, surface);
            } catch (...) {
                throw;
            }

            if (present_result == vk::Result::eSuboptimalKHR || present_result == vk::Result::eErrorOutOfDateKHR) {
                reset(window, surface);
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

        vk::Extent2D get_surface_extent(GLFWwindow* window, const vk::SurfaceCapabilitiesKHR& capabilities)
        {
            if (capabilities.currentExtent.width != 0xFFFFFFFF && capabilities.currentExtent.height != 0xFFFFFFFF) {
                return capabilities.currentExtent;
            }

            int width, height;
            glfwGetWindowSize(window, &width, &height);

            width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return {
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height)};
        }


        uint32_t get_surface_images_count(const vk::SurfaceCapabilitiesKHR& capabilities)
        {
            return std::clamp(capabilities.minImageCount + 1, capabilities.minImageCount, capabilities.minImageCount);
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
        uint32_t m_images_count{0};
        uint32_t m_curr_image{0};
        std::vector<avk::fence> m_wait_fences{};
        std::vector<avk::semaphore> m_wait_semaphores{};

        std::function<void(size_t, size_t)> m_swapchain_reset_listener{};
    };

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
        glw_window(size_t width, size_t height, const std::string& name, std::unique_ptr<window::main_loop_update_listener> update_listener)
            : m_update_listener(std::move(update_listener))
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

            avk::context::init_instance({.extensions = {.names = ext_names, .count = ext_count}});

            m_surface = avk::create_surface(*avk::context::instance(), [this]() -> vk::SurfaceKHR {
                VkSurfaceKHR surface;
                auto res = glfwCreateWindowSurface(static_cast<VkInstance>(*avk::context::instance()), m_window_handler.get(), nullptr, &surface);
                assert(res == VK_SUCCESS);
                assert(surface != VK_NULL_HANDLE);
                return vk::SurfaceKHR{surface};
            });

            avk::context::init_device({.required_supported_surfaces = m_surface.handler_ptr(), .required_supported_surfaces_count = 1});

            static_cast<vk_main_loop_update_listener*>(m_update_listener.get())->on_window_initialized();

            m_swapchain.set_swapcain_reset_listener([this](size_t width, size_t height) {
                static_cast<vk_main_loop_update_listener*>(m_update_listener.get())->on_swapchain_reset(width, height);
            });

            m_swapchain.reset(m_window_handler.get(), m_surface);
        }

        ~glw_window() override
        {
            avk::context::device()->waitIdle();
        }

        bool closed() override
        {
            return glfwWindowShouldClose(m_window_handler.get());
        }

        void main_loop() override
        {
            if (!m_command_pool) {
                m_command_pool = avk::create_command_pool(avk::context::device()->createCommandPool(vk::CommandPoolCreateInfo{
                    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                    .queueFamilyIndex = avk::context::queue_family(vk::QueueFlagBits::eGraphics),
                }));
            }

            glfwPollEvents();

            auto* update_listener_impl = static_cast<vk_main_loop_update_listener*>(m_update_listener.get());
            // TODO: add TS calc
            update_listener_impl->update(0);

            const vk::Image src_image = update_listener_impl->get_present_image();

            if (!src_image) {
                return;
            }

            const vk::Extent2D src_image_size = update_listener_impl->get_present_image_size();
            const vk::ImageLayout src_image_layout = update_listener_impl->get_present_image_layout();

            std::vector<vk::Semaphore> wait_semaphores = update_listener_impl->get_wait_semaphores();

            auto [image_request_wait_semaphore, signal_fence, image_index] = m_swapchain.request_next_image(m_window_handler.get(), m_surface);

            wait_semaphores.push_back(image_request_wait_semaphore);

            const vk::Image dst_image = m_swapchain.swapchain_images()[image_index];
            const vk::Extent2D dst_image_size = m_swapchain.get_size();

            const vk::CommandBuffer current_command_buffer = get_current_command_buffer();

            assert(src_image_size >= dst_image_size);

            current_command_buffer.begin(vk::CommandBufferBeginInfo{
                .flags = {},
                .pInheritanceInfo = nullptr,
            });

            current_command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer,
                {},
                {},
                {},
                {vk::ImageMemoryBarrier{
                     .srcAccessMask = {},
                     .dstAccessMask = {},

                     .oldLayout = src_image_layout,
                     .newLayout = vk::ImageLayout::eTransferSrcOptimal,

                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                     .image = src_image,

                     .subresourceRange = {
                         .aspectMask = vk::ImageAspectFlagBits::eColor,
                         .baseMipLevel = 0,
                         .levelCount = 1,
                         .baseArrayLayer = 0,
                         .layerCount = 1,
                     },
                 },

                 vk::ImageMemoryBarrier{
                     .srcAccessMask = {},
                     .dstAccessMask = {},

                     .oldLayout = vk::ImageLayout::eUndefined,
                     .newLayout = vk::ImageLayout::eTransferDstOptimal,

                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                     .image = dst_image,

                     .subresourceRange = {
                         .aspectMask = vk::ImageAspectFlagBits::eColor,
                         .baseMipLevel = 0,
                         .levelCount = 1,
                         .baseArrayLayer = 0,
                         .layerCount = 1,
                     },
                 }});

            current_command_buffer.blitImage(
                src_image,
                vk::ImageLayout::eTransferSrcOptimal,
                dst_image,
                vk::ImageLayout::eTransferDstOptimal,
                {vk::ImageBlit{
                    .srcSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                    .srcOffsets = {{vk::Offset3D{
                                        .x = 0,
                                        .y = 0,
                                        .z = 0,
                                    },
                                    vk::Offset3D{
                                        .x = static_cast<int32_t>(src_image_size.width),
                                        .y = static_cast<int32_t>(src_image_size.height),
                                        .z = 1,
                                    }}},
                    .dstSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                    .dstOffsets = {{vk::Offset3D{
                                        .x = 0,
                                        .y = 0,
                                        .z = 0,
                                    },
                                    vk::Offset3D{
                                        .x = static_cast<int32_t>(dst_image_size.width),
                                        .y = static_cast<int32_t>(dst_image_size.height),
                                        .z = 1,
                                    }}},
                }},
                vk::Filter::eLinear);

            current_command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                {},
                {},
                {},
                {vk::ImageMemoryBarrier{
                     .srcAccessMask = {},
                     .dstAccessMask = {},

                     .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                     .newLayout = src_image_layout,

                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                     .image = src_image,

                     .subresourceRange = {
                         .aspectMask = vk::ImageAspectFlagBits::eColor,
                         .baseMipLevel = 0,
                         .levelCount = 1,
                         .baseArrayLayer = 0,
                         .layerCount = 1,
                     },
                 },

                 vk::ImageMemoryBarrier{
                     .srcAccessMask = {},
                     .dstAccessMask = {},

                     .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                     .newLayout = vk::ImageLayout::ePresentSrcKHR,

                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                     .image = dst_image,

                     .subresourceRange = {
                         .aspectMask = vk::ImageAspectFlagBits::eColor,
                         .baseMipLevel = 0,
                         .levelCount = 1,
                         .baseArrayLayer = 0,
                         .layerCount = 1,
                     },
                 }});

            current_command_buffer.end();

            if (m_wait_stages.size() != wait_semaphores.size()) {
                m_wait_stages.resize(wait_semaphores.size(), vk::PipelineStageFlagBits::eTransfer);
            }

            auto curr_semaphore = get_current_semaphore();

            avk::context::queue(vk::QueueFlagBits::eGraphics, 0)
                .submit(
                    vk::SubmitInfo{
                        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
                        .pWaitSemaphores = wait_semaphores.data(),
                        .pWaitDstStageMask = m_wait_stages.data(),
                        .commandBufferCount = 1,
                        .pCommandBuffers = &current_command_buffer,
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores = &curr_semaphore,
                    },
                    signal_fence);

            m_swapchain.present(m_window_handler.get(), m_surface, &curr_semaphore, 1, 0);
        }

    private:
        vk::CommandBuffer get_current_command_buffer()
        {
            if (m_command_buffers->empty()) {
                m_command_buffers = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
                    .commandPool = m_command_pool,
                    .commandBufferCount = m_swapchain.images_count(),
                });
            }

            m_command_buffers->at(m_swapchain.current_image()).reset();

            return m_command_buffers->at(m_swapchain.current_image());
        }

        vk::Semaphore get_current_semaphore()
        {
            if (m_submit_semaphores.empty()) {
                m_submit_semaphores.resize(m_swapchain.images_count());
            }

            if (!m_submit_semaphores[m_swapchain.current_image()]) {
                m_submit_semaphores[m_swapchain.current_image()] = avk::create_semaphore(avk::context::device()->createSemaphore({}));
            }

            return m_submit_semaphores[m_swapchain.current_image()];
        }

        std::unique_ptr<GLFWwindow, void (*)(GLFWwindow* window)> m_window_handler{nullptr, glfwDestroyWindow};
        std::shared_ptr<window_context_init_raii> m_window_ctx{};
        std::unique_ptr<window::main_loop_update_listener> m_update_listener{};
        avk::surface m_surface{};
        avk::command_pool m_command_pool{};
        avk::command_buffer_list m_command_buffers{};
        swapchain_handler m_swapchain{};

        std::vector<vk::PipelineStageFlags> m_wait_stages{};
        std::vector<avk::semaphore> m_submit_semaphores{};
    };
} // namespace


std::unique_ptr<window::impl> sandbox::hal::window::impl::create(
    size_t width,
    size_t height,
    const std::string& name,
    std::unique_ptr<main_loop_update_listener> main_loop_listener)
{
    return std::make_unique<glw_window>(width, height, name, std::move(main_loop_listener));
}