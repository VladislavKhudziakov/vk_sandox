#pragma once

#include <render/vk/raii.hpp>

#include <functional>

namespace sandbox::hal::render::avk
{
    std::pair<avk::vma_image, avk::image_view> create_attachment(
        uint32_t width, uint32_t height, vk::Format format, uint32_t queue_family, vk::ImageUsageFlags usage = {});

    avk::vma_buffer create_staging_buffer(
        uint32_t queue_family, size_t data_size, const std::function<void(uint8_t*)>& on_buffer_mapped = {});

    avk::framebuffer create_framebuffer_from_attachments(
        uint32_t width, uint32_t height, const vk::RenderPass& pass, vk::ImageView* attachments, uint32_t attachments_count);
} // namespace sandbox::hal::render::avk
