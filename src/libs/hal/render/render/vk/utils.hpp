#pragma once

#include <render/vk/raii.hpp>

#include <functional>

namespace sandbox::hal::render::avk
{
    std::pair<avk::vma_image, avk::image_view> create_attachment(
        uint32_t width,
        uint32_t height,
        vk::Format format,
        uint32_t queue_family,
        vk::ImageUsageFlags usage = {},
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);


    avk::vma_buffer create_staging_buffer(
        uint32_t queue_family, size_t data_size, const std::function<void(uint8_t*)>& on_buffer_mapped = {});


    avk::framebuffer create_framebuffer_from_attachments(
        uint32_t width, uint32_t height, const vk::RenderPass& pass, const vk::ImageView* attachments, uint32_t attachments_count);


    std::tuple<avk::framebuffer, std::vector<avk::vma_image>, std::vector<avk::image_view>>
    create_framebuffer_from_pass(
        uint32_t width,
        uint32_t height,
        uint32_t queue_family,
        const vk::RenderPassCreateInfo& pass_info,
        const vk::RenderPass& pass,
        const std::vector<float>& attachments_scales = {},
        const std::vector<vk::ImageUsageFlagBits>& attachments_usages = {});


    struct pass_create_info
    {
        std::vector<vk::AttachmentDescription> attachments{};
        std::vector<vk::AttachmentReference> attachments_refs{};
        std::vector<vk::SubpassDescription> subpasses{};
        std::vector<vk::SubpassDependency> subpass_dependencies{};
    };

    class render_pass_wrapper
    {
    public:
        render_pass_wrapper() = default;

        render_pass_wrapper(pass_create_info info);
        const vk::RenderPassCreateInfo& get_info() const;
        vk::RenderPass get_native_pass() const;

    private:
        pass_create_info m_info{};
        std::vector<vk::AttachmentReference> m_attachments_refs{};
        vk::RenderPassCreateInfo m_pass_info{};
        avk::render_pass m_pass_handler{};
    };
} // namespace sandbox::hal::render::avk
