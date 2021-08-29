#include "utils.hpp"

#include <render/vk/errors_handling.hpp>

#include <utils/scope_helpers.hpp>


using namespace sandbox::hal::render;


std::pair<avk::vma_image, avk::image_view> sandbox::hal::render::avk::create_attachment(
    uint32_t width, uint32_t height, vk::Format format, uint32_t queue_family, vk::ImageUsageFlags usage)
{
    // clang-format off
    bool is_depth =
        format == vk::Format::eD24UnormS8Uint ||
        format == vk::Format::eD16Unorm ||
        format == vk::Format::eD16UnormS8Uint ||
        format == vk::Format::eD32Sfloat ||
        format == vk::Format::eD32SfloatS8Uint;
    // clang-format on

    if (is_depth) {
        usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    } else {
        usage |= vk::ImageUsageFlagBits::eColorAttachment;
    }

    auto image = avk::create_vma_image(
        vk::ImageCreateInfo{
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family,
            .initialLayout = vk::ImageLayout::eUndefined,
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        });

    auto image_view = avk::create_image_view(
        avk::context::device()->createImageView(
            vk::ImageViewCreateInfo{
                .flags = {},
                .image = image.as<vk::Image>(),
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .components = {
                    .r = vk::ComponentSwizzle::eR,
                    .g = vk::ComponentSwizzle::eG,
                    .b = vk::ComponentSwizzle::eB,
                    .a = vk::ComponentSwizzle::eA,
                },
                .subresourceRange = {
                    .aspectMask = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            }));

    return {std::move(image), std::move(image_view)};
}


avk::vma_buffer avk::create_staging_buffer(
    uint32_t queue_family, size_t data_size, const std::function<void(uint8_t*)>& on_buffer_mapped)
{
    auto buffer = avk::create_vma_buffer(
        vk::BufferCreateInfo{
            .size = data_size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family,
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    if (!on_buffer_mapped) {
        return buffer;
    }

    {
        utils::on_scope_exit exit([&buffer]() {
            auto allocation = buffer.as<VmaAllocation>();
            VkDeviceSize offset = 0;
            VkDeviceSize size = VK_WHOLE_SIZE;

            vmaUnmapMemory(avk::context::allocator(), buffer.as<VmaAllocation>());
            vmaFlushAllocations(avk::context::allocator(), 1, &allocation, &offset, &size);
        });

        void* mapped_data{nullptr};
        VK_C_CALL(vmaMapMemory(avk::context::allocator(), buffer.as<VmaAllocation>(), &mapped_data));
        assert(mapped_data != nullptr);
        on_buffer_mapped(reinterpret_cast<uint8_t*>(mapped_data));
    }

    return buffer;
}


avk::framebuffer avk::create_framebuffer_from_attachments(
    uint32_t width, uint32_t height, const vk::RenderPass& pass, vk::ImageView* attachments, uint32_t attachments_count)
{
    return avk::create_framebuffer(
        avk::context::device()->createFramebuffer(
            vk::FramebufferCreateInfo{
                .flags = {},
                .renderPass = pass,

                .attachmentCount = attachments_count,
                .pAttachments = attachments,

                .width = width,
                .height = height,
                .layers = 1,
            }));
}
