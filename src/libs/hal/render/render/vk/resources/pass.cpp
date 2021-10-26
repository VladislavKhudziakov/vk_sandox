#include "pass.hpp"

#include <render/vk/utils.hpp>

#include <utils/conditions_helpers.hpp>

using namespace sandbox::hal::render;

void avk::render_pass_instance::resize(uint32_t queue_family, uint32_t width, uint32_t height)
{
    m_framebuffer_width = width;
    m_framebuffer_height = height;

    if (m_attachments_usages.empty()) {
        m_attachments_usages.reserve(m_attachments_descriptions.size());

        for (const auto& _ : m_attachments_descriptions) {
            m_attachments_usages.emplace_back(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);
        }
    }

    m_framebuffers.clear();
    m_framebuffers.reserve(m_framebuffers_count);
    m_images.clear();
    m_images.reserve(m_attachments_descriptions.size() * m_framebuffers_count);
    m_images_views.clear();
    m_images_views.reserve(m_attachments_descriptions.size() * m_framebuffers_count);

    for (int i = 0; i < m_framebuffers_count; ++i) {
        auto [framebuffer, images, images_views] = avk::gen_framebuffer(
            width,
            height,
            queue_family,
            m_pass,
            m_attachments_descriptions,
            m_attachments_sizes,
            m_attachments_scales,
            m_attachments_usages);

        m_framebuffers.emplace_back(std::move(framebuffer));

        for (auto& img : images) {
            m_images.emplace_back(std::move(img));
        }

        for (auto& img_view : images_views) {
            m_images_views.emplace_back(std::move(img_view));
        }
    }
}


void avk::render_pass_instance::begin(vk::CommandBuffer& command_buffer, vk::SubpassContents content)
{
    command_buffer.setViewport(
        0,
        {vk::Viewport{
            .x = 0,
            .y = 0,
            .width = static_cast<float>(m_framebuffer_width),
            .height = static_cast<float>(m_framebuffer_height),
            .minDepth = 0,
            .maxDepth = 1}});

    command_buffer.setScissor(
        0, {vk::Rect2D{
               .offset = {.x = 0, .y = 0},
               .extent = {
                   .width = m_framebuffer_width,
                   .height = m_framebuffer_height,
               },
           }});

    command_buffer.beginRenderPass(
        vk::RenderPassBeginInfo{
            .renderPass = m_pass,
            .framebuffer = m_framebuffers[m_current_framebuffer_index],
            .renderArea = {
                .offset = {
                    .x = 0,
                    .y = 0,
                },
                .extent = {
                    .width = m_framebuffer_width,
                    .height = m_framebuffer_height,
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
    m_current_framebuffer_index = (m_current_framebuffer_index + 1) % m_framebuffers_count;
}


vk::RenderPass avk::render_pass_instance::get_native_pass() const
{
    return m_pass.as<vk::RenderPass>();
}


vk::Image avk::render_pass_instance::get_framebuffer_image(uint32_t image, uint32_t framebuffer_index)
{
    return m_images[m_attachments_descriptions.size() * framebuffer_index + image].as<vk::Image>();
}


avk::render_pass_builder& avk::render_pass_builder::begin_sub_pass(vk::SampleCountFlagBits samples)
{
    m_sub_passes_data.emplace_back(sub_pass_data{
        .samples = samples});

    return *this;
}


avk::render_pass_builder& avk::render_pass_builder::add_color_attachment(
    vk::Extent2D size,
    scale_fator scale,
    vk::Format format,
    vk::ImageLayout init_layout,
    vk::ImageLayout subpass_layout,
    vk::ImageLayout final_layout,
    std::optional<std::array<float, 4>> clear_values)
{
    m_attachments_sizes.emplace_back(size);
    m_attachments_scales.emplace_back(scale);

    m_attachments.emplace_back(vk::AttachmentDescription{
        .flags = {},
        .format = format,
        .samples = m_sub_passes_data.back().samples,
        .loadOp = clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .initialLayout = init_layout,
        .finalLayout = final_layout});

    m_sub_passes_data.back().color_attachments.emplace_back(vk::AttachmentReference{
        .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
        .layout = subpass_layout,
    });

    if (clear_values) {
        m_clear_values.emplace_back(vk::ClearColorValue{*clear_values});
    }

    if (m_sub_passes_data.back().samples != vk::SampleCountFlagBits::e1) {
        m_attachments_sizes.emplace_back(size);
        m_attachments_scales.emplace_back(scale);

        m_attachments.emplace_back(vk::AttachmentDescription{
            .flags = {},
            .format = format,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .initialLayout = init_layout,
            .finalLayout = final_layout});

        m_sub_passes_data.back().resolve_attachments.emplace_back(vk::AttachmentReference{
            .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
            .layout = subpass_layout,
        });

        if (clear_values) {
            m_clear_values.emplace_back(vk::ClearColorValue{*clear_values});
        }
    }

    return *this;
}


avk::render_pass_builder& avk::render_pass_builder::set_depth_stencil_attachment(
    vk::Extent2D size,
    scale_fator scale,
    vk::Format format,
    vk::ImageLayout init_layout,
    vk::ImageLayout subpass_layout,
    vk::ImageLayout final_layout,
    std::optional<float> clear_values,
    std::optional<uint32_t> stencil_clear_values)
{
    m_attachments_sizes.emplace_back(size);
    m_attachments_scales.emplace_back(scale);

    ASSERT(!m_sub_passes_data.back().depth_stencil_attachment_ref);

    // TODO MSAA
    m_attachments.emplace_back(vk::AttachmentDescription{
        .flags = {},
        .format = format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = stencil_clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
        .stencilStoreOp = vk::AttachmentStoreOp::eStore,
        .initialLayout = init_layout,
        .finalLayout = final_layout});

    m_sub_passes_data.back().depth_stencil_attachment_ref = vk::AttachmentReference{
        .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
        .layout = subpass_layout,
    };

    if (clear_values || stencil_clear_values) {
        m_clear_values.emplace_back(vk::ClearDepthStencilValue{
            clear_values ? *clear_values : 1,
            stencil_clear_values ? *stencil_clear_values : 0});
    }

    return *this;
}


avk::render_pass_builder& avk::render_pass_builder::finish_sub_pass()
{
    return *this;
}


avk::render_pass_instance avk::render_pass_builder::create_pass(uint32_t framebuffers_count)
{
    avk::render_pass_instance new_pass{};

    for (const auto& pass_data : m_sub_passes_data) {
        m_sub_passes.emplace_back(vk::SubpassDescription{
            .flags = {},
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = static_cast<uint32_t>(pass_data.color_attachments.size()),
            .pColorAttachments = pass_data.color_attachments.data(),
            .pResolveAttachments = pass_data.resolve_attachments.data(),
            .pDepthStencilAttachment = pass_data.depth_stencil_attachment_ref ? &*pass_data.depth_stencil_attachment_ref : nullptr,
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
    new_pass.m_framebuffers_count = framebuffers_count;

    return new_pass;
}


avk::render_pass_builder& avk::render_pass_builder::add_sub_pass_dependency(const vk::SubpassDependency& dep)
{
    m_sub_pass_dependencies.emplace_back(dep);
    return *this;
}