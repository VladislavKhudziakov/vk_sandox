#pragma once

#include <render/vk/raii.hpp>

#include <variant>

namespace sandbox::hal::render::avk
{
    class render_pass_instance;

    class framebuffer_instance
    {
        friend class render_pass_instance;

    public:
        vk::Image get_image(uint32_t image) const;
        vk::ImageView get_image_view(uint32_t view) const;

        uint32_t get_width() const;
        uint32_t get_height() const;

        operator vk::Framebuffer() const;

    private:
        void resize(
            vk::RenderPass pass,
            uint32_t queue_family,
            uint32_t width,
            uint32_t height,
            const std::vector<vk::AttachmentDescription>& attachments,
            const std::vector<vk::Extent2D>& sizes,
            const std::vector<std::pair<float, float>>& scales);

        avk::framebuffer m_framebuffer{};
        std::vector<avk::vma_image> m_images{};
        std::vector<avk::image_view> m_images_views{};

        uint32_t m_framebuffer_width{};
        uint32_t m_framebuffer_height{};
    };


    class attachment_instance
    {
        friend class subpass_instance;

    public:
        vk::Image get_draw_image() const;
        vk::ImageView get_draw_image_view() const;

        vk::Image get_resolve_image() const;
        vk::ImageView get_resolve_image_view() const;

        operator vk::Image() const;
        operator vk::ImageView() const;

    private:
        vk::Image m_draw_image{};
        vk::ImageView m_draw_image_view{};

        vk::Image m_resolve_image{};
        vk::ImageView m_resolve_image_view{};
    };


    class subpass_instance
    {
        friend class render_pass_builder;

    public:
        attachment_instance get_color_attachment(uint32_t, const render_pass_instance& pass) const;
        attachment_instance get_depth_attachment(const render_pass_instance& pass) const;

        vk::SampleCountFlagBits get_max_samples_count() const;

    private:
        std::unordered_map<uint32_t, uint32_t> m_msaa_resolve_attachment_map{};

        std::vector<vk::AttachmentReference> m_color_attachments{};
        std::vector<vk::AttachmentReference> m_resolve_attachments{};
        std::optional<vk::AttachmentReference> m_depth_stencil_attachment_ref{};

        vk::SampleCountFlagBits m_max_samples_count{vk::SampleCountFlagBits::e1};
    };


    class render_pass_instance
    {
        friend class render_pass_builder;

    public:
        using scale_fator = std::pair<float, float>;

        void begin(vk::CommandBuffer&, vk::SubpassContents content = vk::SubpassContents::eInline);
        void next_sub_pass(vk::CommandBuffer&, vk::SubpassContents content = vk::SubpassContents::eInline);
        void finish(vk::CommandBuffer&);

        void resize(uint32_t width, uint32_t height);

        const subpass_instance& get_subpass(uint32_t index = 0) const;
        const framebuffer_instance& get_framebuffer() const;

        operator vk::RenderPass() const;

    private:
        uint32_t m_queue_family{};

        std::vector<subpass_instance> m_subpasses{};
        std::vector<framebuffer_instance> m_framebuffers{};

        std::vector<vk::AttachmentDescription> m_attachments_descriptions{};
        std::vector<vk::Extent2D> m_attachments_sizes{};
        std::vector<scale_fator> m_attachments_scales{};
        std::vector<vk::ClearValue> m_clear_values{};

        avk::render_pass m_pass{};
        uint32_t m_current_framebuffer{0};
    };


    class render_pass_builder
    {
    public:
        using scale_fator = std::pair<float, float>;
        using color_clear = std::array<float, 4>;
        using depth_clear = std::pair<float, uint32_t>;

        render_pass_builder& begin_sub_pass(vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
        render_pass_builder& begin_attachment(vk::Format format, vk::ImageLayout init, vk::ImageLayout subpass, vk::ImageLayout final);
        render_pass_builder& set_attachment_size(vk::Extent2D, scale_fator scale = {1, 1});
        render_pass_builder& set_attachmen_clear(color_clear);
        render_pass_builder& set_attachmen_clear(depth_clear);
        render_pass_builder& finish_attachment();

        render_pass_builder& finish_sub_pass();
        render_pass_builder& add_sub_pass_dependency(const vk::SubpassDependency&);
        render_pass_instance create_pass(uint32_t queue_family, uint32_t framebuffers_count = 1);

    private:
        struct attachment_description
        {
            vk::Format format{};
            vk::ImageLayout init_layout{};
            vk::ImageLayout subpass_layout{};
            vk::ImageLayout finish_layout{};

            vk::Extent2D size{0, 0};
            float scale_x{1};
            float scale_y{1};
            vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};

            std::variant<color_clear, depth_clear> clear_value{};
        };

        attachment_description& current_attachment();
        subpass_instance& current_subpass();

        std::optional<attachment_description> m_current_attachment{};

        std::vector<vk::Extent2D> m_attachments_sizes{};
        std::vector<scale_fator> m_attachments_scales{};
        std::vector<vk::ClearValue> m_clear_values{};

        std::vector<vk::AttachmentDescription> m_attachments{};
        std::vector<subpass_instance> m_sub_pass_instances{};
        std::vector<vk::SubpassDescription> m_sub_passes{};
        std::vector<vk::SubpassDependency> m_sub_pass_dependencies{};
    };
} // namespace sandbox::hal::render::avk