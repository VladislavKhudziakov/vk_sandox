#include "pass.hpp"

#include <render/vk/utils.hpp>

#include <utils/conditions_helpers.hpp>

using namespace sandbox::hal::render;

void avk::render_pass_instance::resize(uint32_t width, uint32_t height)
{
    for (auto& fb : m_framebuffers) {
        fb.resize(*this, m_queue_family, width, height, m_attachments_descriptions, m_attachments_sizes, m_attachments_scales);
    }
}


const avk::framebuffer_instance& sandbox::hal::render::avk::render_pass_instance::get_framebuffer() const
{
    return m_framebuffers[m_current_framebuffer];
}


const avk::subpass_instance& sandbox::hal::render::avk::render_pass_instance::get_subpass(uint32_t index) const
{
    return m_subpasses[index];
}


sandbox::hal::render::avk::render_pass_instance::operator vk::RenderPass() const
{
    return m_pass;
}


void avk::render_pass_instance::begin(vk::CommandBuffer& command_buffer, vk::SubpassContents content)
{
    auto& fb = m_framebuffers[m_current_framebuffer];

    command_buffer.setViewport(
        0,
        {vk::Viewport{
            .x = 0,
            .y = 0,
            .width = static_cast<float>(fb.get_width()),
            .height = static_cast<float>(fb.get_height()),
            .minDepth = 0,
            .maxDepth = 1}});

    command_buffer.setScissor(
        0, {vk::Rect2D{
               .offset = {.x = 0, .y = 0},
               .extent = {
                   .width = fb.get_width(),
                   .height = fb.get_height(),
               },
           }});

    command_buffer.beginRenderPass(
        vk::RenderPassBeginInfo{
            .renderPass = m_pass,
            .framebuffer = fb,
            .renderArea = {
                .offset = {
                    .x = 0,
                    .y = 0,
                },
                .extent = {
                    .width = fb.get_width(),
                    .height = fb.get_height(),
                },
            },

            .clearValueCount = static_cast<uint32_t>(m_clear_values.size()),
            .pClearValues = m_clear_values.data(),
        },
        content);
}


void avk::render_pass_instance::next_sub_pass(vk::CommandBuffer& command_buffer, vk::SubpassContents content)
{
    command_buffer.nextSubpass(content);
}


void avk::render_pass_instance::finish(vk::CommandBuffer& command_buffer)
{
    command_buffer.endRenderPass();
    m_current_framebuffer = (m_current_framebuffer + 1) % m_framebuffers.size();
}


avk::render_pass_builder& avk::render_pass_builder::begin_sub_pass(vk::SampleCountFlagBits samples)
{
    m_sub_pass_instances.emplace_back();

    return *this;
}


avk::render_pass_builder& sandbox::hal::render::avk::render_pass_builder::begin_attachment(
    vk::Format format, vk::ImageLayout init, vk::ImageLayout subpass, vk::ImageLayout finish)
{
    m_current_attachment = attachment_description{
        .format = format,
        .init_layout = init,
        .subpass_layout = subpass,
        .finish_layout = finish};

    return *this;
}


avk::render_pass_builder& sandbox::hal::render::avk::render_pass_builder::set_attachment_size(vk::Extent2D extent, scale_fator scale)
{
    current_attachment().size = extent;
    current_attachment().scale_x = scale.first;
    current_attachment().scale_y = scale.second;
    return *this;
}


avk::render_pass_builder& sandbox::hal::render::avk::render_pass_builder::set_attachmen_clear(color_clear clear_value)
{
    current_attachment().clear_value = clear_value;
    return *this;
}


avk::render_pass_builder& sandbox::hal::render::avk::render_pass_builder::set_attachmen_clear(depth_clear clear_value)
{
    current_attachment().clear_value = clear_value;
    return *this;
}


avk::render_pass_builder& sandbox::hal::render::avk::render_pass_builder::finish_attachment()
{
    const auto& curr_attachment = current_attachment();
    auto& curr_subpass = current_subpass();

    bool is_color_attachment = !is_depth_format(curr_attachment.format);
    bool need_clear = false;
    color_clear clear_color_value{};
    depth_clear clear_depth_value{};

    if (is_color_attachment) {
        if (auto* clr = std::get_if<color_clear>(&curr_attachment.clear_value); clr != nullptr) {
            need_clear = true;
            clear_color_value = *clr;
        }
    } else {
        if (auto* clr = std::get_if<depth_clear>(&curr_attachment.clear_value); clr != nullptr) {
            need_clear = true;
            clear_depth_value = *clr;
        }
    }

    m_attachments_sizes.emplace_back(curr_attachment.size);
    m_attachments_scales.emplace_back(curr_attachment.scale_x, curr_attachment.scale_y);

    m_attachments.emplace_back(vk::AttachmentDescription{
        .flags = {},
        .format = curr_attachment.format,
        .samples = is_color_attachment ? curr_attachment.samples : vk::SampleCountFlagBits::e1,
        .loadOp = need_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .initialLayout = curr_attachment.init_layout,
        .finalLayout = curr_attachment.finish_layout});

    if (is_color_attachment) {
        curr_subpass.m_color_attachments.emplace_back(vk::AttachmentReference{
            .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
            .layout = curr_attachment.subpass_layout,
        });

        if (need_clear) {
            m_clear_values.emplace_back(vk::ClearColorValue{clear_color_value});
        }

        if (curr_attachment.samples != vk::SampleCountFlagBits::e1) {
            curr_subpass.m_msaa_resolve_attachment_map[m_attachments.size() - 1] = m_attachments.size();

            m_attachments_sizes.emplace_back(curr_attachment.size);
            m_attachments_scales.emplace_back(curr_attachment.scale_x, curr_attachment.scale_y);

            m_attachments.emplace_back(vk::AttachmentDescription{
                .flags = {},
                .format = curr_attachment.format,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eDontCare,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .initialLayout = curr_attachment.init_layout,
                .finalLayout = curr_attachment.finish_layout});

            curr_subpass.m_resolve_attachments.emplace_back(vk::AttachmentReference{
                .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
                .layout = curr_attachment.subpass_layout,
            });
        }

        curr_subpass.m_max_samples_count = std::max(curr_subpass.m_max_samples_count, curr_attachment.samples);
    } else {
        curr_subpass.m_depth_stencil_attachment_ref.emplace(vk::AttachmentReference{
            .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
            .layout = curr_attachment.subpass_layout,
        });

        if (need_clear) {
            m_clear_values.emplace_back(vk::ClearDepthStencilValue{clear_depth_value.first, clear_depth_value.second});
        }
    }

    return *this;
}


avk::render_pass_builder& avk::render_pass_builder::finish_sub_pass()
{
    return *this;
}


avk::render_pass_instance avk::render_pass_builder::create_pass(uint32_t queue_family, uint32_t framebuffers_count)
{
    avk::render_pass_instance new_pass{};
    new_pass.m_queue_family = queue_family;

    for (const auto& subpass : m_sub_pass_instances) {
        m_sub_passes.emplace_back(vk::SubpassDescription{
            .flags = {},
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = static_cast<uint32_t>(subpass.m_color_attachments.size()),
            .pColorAttachments = subpass.m_color_attachments.data(),
            .pResolveAttachments = subpass.m_resolve_attachments.data(),
            .pDepthStencilAttachment = subpass.m_depth_stencil_attachment_ref ? &*subpass.m_depth_stencil_attachment_ref : nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr,
        });
    }

    new_pass.m_pass = avk::create_render_pass(avk::context::device()->createRenderPass(vk::RenderPassCreateInfo{
        .flags = {},
        .attachmentCount = static_cast<uint32_t>(m_attachments.size()),
        .pAttachments = m_attachments.data(),
        .subpassCount = static_cast<uint32_t>(m_sub_passes.size()),
        .pSubpasses = m_sub_passes.data(),
        .dependencyCount = static_cast<uint32_t>(m_sub_pass_dependencies.size()),
        .pDependencies = m_sub_pass_dependencies.data(),
    }));

    new_pass.m_attachments_scales = m_attachments_scales;
    new_pass.m_attachments_sizes = m_attachments_sizes;
    new_pass.m_clear_values = m_clear_values;
    new_pass.m_attachments_descriptions = m_attachments;
    new_pass.m_framebuffers.resize(framebuffers_count);

    new_pass.m_subpasses = std::move(m_sub_pass_instances);

    return new_pass;
}


avk::render_pass_builder::attachment_description& sandbox::hal::render::avk::render_pass_builder::current_attachment()
{
    CHECK(m_current_attachment);
    return *m_current_attachment;
}


avk::subpass_instance& sandbox::hal::render::avk::render_pass_builder::current_subpass()
{
    CHECK(!m_sub_pass_instances.empty());
    return m_sub_pass_instances.back();
}


avk::render_pass_builder& avk::render_pass_builder::add_sub_pass_dependency(const vk::SubpassDependency& dep)
{
    m_sub_pass_dependencies.emplace_back(dep);
    return *this;
}


vk::Image sandbox::hal::render::avk::framebuffer_instance::get_image(uint32_t image) const
{
    return m_images[image].as<vk::Image>();
}


vk::ImageView sandbox::hal::render::avk::framebuffer_instance::get_image_view(uint32_t view) const
{
    return m_images_views[view];
}

uint32_t sandbox::hal::render::avk::framebuffer_instance::get_width() const
{
    return m_framebuffer_width;
}


uint32_t sandbox::hal::render::avk::framebuffer_instance::get_height() const
{
    return m_framebuffer_height;
}


sandbox::hal::render::avk::framebuffer_instance::operator vk::Framebuffer() const
{
    return m_framebuffer;
}


void sandbox::hal::render::avk::framebuffer_instance::resize(
    vk::RenderPass pass,
    uint32_t queue_family,
    uint32_t width,
    uint32_t height,
    const std::vector<vk::AttachmentDescription>& attachments,
    const std::vector<vk::Extent2D>& sizes,
    const std::vector<std::pair<float, float>>& scales)
{
    if (m_framebuffer_width == width && m_framebuffer_height == height) {
        return;
    }

    m_framebuffer_width = width;
    m_framebuffer_height = height;

    m_images.clear();
    m_images.reserve(attachments.size());
    m_images_views.clear();
    m_images_views.reserve(attachments.size());


    auto [framebuffer, images, images_views] = avk::gen_framebuffer(
        width,
        height,
        queue_family,
        pass,
        attachments,
        sizes,
        scales);

    m_framebuffer = std::move(framebuffer);

    for (auto& img : images) {
        m_images.emplace_back(std::move(img));
    }

    for (auto& img_view : images_views) {
        m_images_views.emplace_back(std::move(img_view));
    }
}


vk::Image sandbox::hal::render::avk::attachment_instance::get_draw_image() const
{
    return m_draw_image;
}


vk::ImageView sandbox::hal::render::avk::attachment_instance::get_draw_image_view() const
{
    return m_draw_image_view;
}


vk::Image sandbox::hal::render::avk::attachment_instance::get_resolve_image() const
{
    return m_resolve_image;
}


vk::ImageView sandbox::hal::render::avk::attachment_instance::get_resolve_image_view() const
{
    return m_resolve_image_view;
}


sandbox::hal::render::avk::attachment_instance::operator vk::Image() const
{
    if (bool(m_resolve_image)) {
        return m_resolve_image;
    } else {
        return m_draw_image;
    }
}


sandbox::hal::render::avk::attachment_instance::operator vk::ImageView() const
{
    if (bool(m_resolve_image_view)) {
        return m_resolve_image_view;
    } else {
        return m_draw_image_view;
    }
}


avk::attachment_instance sandbox::hal::render::avk::subpass_instance::get_depth_attachment(const render_pass_instance& rpass) const
{
    CHECK(m_depth_stencil_attachment_ref);
    const auto& fb = rpass.get_framebuffer();
    avk::attachment_instance result{};

    result.m_draw_image = fb.get_image(m_depth_stencil_attachment_ref->attachment);
    result.m_draw_image_view = fb.get_image_view(m_depth_stencil_attachment_ref->attachment);

    return result;
}


vk::SampleCountFlagBits sandbox::hal::render::avk::subpass_instance::get_max_samples_count() const
{
    return m_max_samples_count;
}


avk::attachment_instance sandbox::hal::render::avk::subpass_instance::get_color_attachment(uint32_t index, const render_pass_instance& rpass) const
{
    auto& attachment = m_color_attachments[index];

    const auto& fb = rpass.get_framebuffer();
    avk::attachment_instance result{};

    result.m_draw_image = fb.get_image(attachment.attachment);
    result.m_draw_image_view = fb.get_image_view(attachment.attachment);

    if (m_msaa_resolve_attachment_map.find(attachment.attachment) != m_msaa_resolve_attachment_map.end()) {
        uint32_t i = attachment.attachment;

        result.m_resolve_image = fb.get_image(m_msaa_resolve_attachment_map.at(i));
        result.m_draw_image_view = fb.get_image_view(m_msaa_resolve_attachment_map.at(i));
    }

    return result;
}
