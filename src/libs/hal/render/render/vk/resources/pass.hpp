#pragma once

#include <render/vk/raii.hpp>

namespace sandbox::hal::render::avk
{
    class render_pass_instance
    {
        friend class render_pass_builder;

    public:
        using scale_fator = std::pair<float, float>;

        void begin(vk::CommandBuffer&, vk::SubpassContents content = vk::SubpassContents::eInline);
        void next_sub_pass(vk::CommandBuffer&, vk::SubpassContents content = vk::SubpassContents::eInline);
        void finish(vk::CommandBuffer&);

        void resize(uint32_t queue_family, uint32_t width, uint32_t height);

        vk::RenderPass get_native_pass() const;

        vk::Image get_framebuffer_image(uint32_t image, uint32_t framebuffer_index = 0);

    private:
        std::vector<avk::framebuffer> m_framebuffers{};
        std::vector<avk::vma_image> m_images{};
        std::vector<avk::image_view> m_images_views{};

        std::vector<vk::AttachmentDescription> m_attachments_descriptions{};
        std::vector<vk::Extent2D> m_attachments_sizes{};
        std::vector<scale_fator> m_attachments_scales{};
        std::vector<vk::ImageUsageFlags> m_attachments_usages{};
        std::vector<vk::ClearValue> m_clear_values{};

        avk::render_pass m_pass{};
        size_t m_framebuffers_count{1};
        uint32_t m_current_framebuffer_index{0};
        uint32_t m_framebuffer_width{};
        uint32_t m_framebuffer_height{};
    };


    class render_pass_builder
    {
    public:
        using scale_fator = std::pair<float, float>;

        render_pass_builder& begin_sub_pass(vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

        render_pass_builder& add_color_attachment(
            vk::Extent2D size,
            scale_fator scale,
            vk::Format format,
            vk::ImageLayout init_layout,
            vk::ImageLayout subpass_layout,
            vk::ImageLayout final_layout,
            std::optional<std::array<float, 4>> clear_values = std::nullopt);

        render_pass_builder& set_depth_stencil_attachment(
            vk::Extent2D size,
            scale_fator scale,
            vk::Format format,
            vk::ImageLayout init_layout,
            vk::ImageLayout subpass_layout,
            vk::ImageLayout final_layout,
            std::optional<float> depth_clear_values = std::nullopt,
            std::optional<uint32_t> stencil_clear_values = std::nullopt);

        render_pass_builder& finish_sub_pass();
        render_pass_builder& add_sub_pass_dependency(const vk::SubpassDependency&);
        render_pass_instance create_pass(uint32_t framebuffers_count = 1);

    private:
        struct sub_pass_data
        {
            vk::SampleCountFlagBits samples{};
            std::vector<vk::AttachmentReference> color_attachments{};
            std::vector<vk::AttachmentReference> resolve_attachments{};
            std::optional<vk::AttachmentReference> depth_stencil_attachment_ref{};
        };

        std::vector<vk::Extent2D> m_attachments_sizes{};
        std::vector<scale_fator> m_attachments_scales{};
        std::vector<vk::ClearValue> m_clear_values{};

        std::vector<vk::AttachmentDescription> m_attachments{};
        std::vector<sub_pass_data> m_sub_passes_data{};
        std::vector<vk::SubpassDescription> m_sub_passes{};
        std::vector<vk::SubpassDependency> m_sub_pass_dependencies{};
    };
} // namespace sandbox::hal::render::avk