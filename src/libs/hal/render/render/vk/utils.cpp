#include "utils.hpp"

#include <render/vk/errors_handling.hpp>

#include <utils/scope_helpers.hpp>


using namespace sandbox::hal::render;


std::pair<avk::vma_image, avk::image_view> sandbox::hal::render::avk::create_attachment(
    uint32_t width,
    uint32_t height,
    vk::Format format,
    uint32_t queue_family,
    vk::ImageUsageFlags usage,
    vk::SampleCountFlagBits samples)
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
            .samples = samples,
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
    uint32_t width, uint32_t height, const vk::RenderPass& pass, const vk::ImageView* attachments, uint32_t attachments_count)
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


std::tuple<avk::framebuffer, std::vector<avk::vma_image>, std::vector<avk::image_view>>
avk::create_framebuffer_from_pass(
    uint32_t width,
    uint32_t height,
    uint32_t queue_family,
    const vk::RenderPassCreateInfo& pass_info,
    const vk::RenderPass& pass,
    const std::vector<float>& attachments_scales,
    const std::vector<vk::ImageUsageFlagBits>& attachments_usages)
{
    std::vector<avk::vma_image> images{};
    std::vector<avk::image_view> images_views{};
    std::vector<vk::ImageView> image_views_native{};

    images.reserve(pass_info.attachmentCount);
    images_views.reserve(pass_info.attachmentCount);
    image_views_native.reserve(pass_info.attachmentCount);

    for (int i = 0; i < pass_info.attachmentCount; ++i) {
        const auto& attachment = pass_info.pAttachments[i];

        uint32_t attachment_width = width;
        uint32_t attachment_height = height;
        vk::ImageUsageFlagBits attachment_usage = {};

        if (i < attachments_scales.size() && attachments_scales[i] > 0) {
            attachment_width *= attachments_scales[i];
            attachment_height *= attachments_scales[i];
        }

        if (i < attachments_usages.size()) {
            attachment_usage = attachments_usages[i];
        }

        auto [image, view] = create_attachment(
            attachment_width,
            attachment_height,
            attachment.format,
            queue_family,
            attachment_usage,
            attachment.samples);

        vk::ImageView native_view = view;

        images.emplace_back(std::move(image));
        images_views.emplace_back(std::move(view));
        image_views_native.emplace_back(native_view);
    }

    auto framebuffer = create_framebuffer_from_attachments(width, height, pass, image_views_native.data(), image_views_native.size());

    return std::make_tuple(std::move(framebuffer), std::move(images), std::move(images_views));
}


avk::render_pass_wrapper::render_pass_wrapper(avk::pass_create_info info)
    : m_info(std::move(info))
    , m_pass_info{
          .flags = {},
          .attachmentCount = static_cast<uint32_t>(m_info.attachments.size()),
          .pAttachments = m_info.attachments.data(),
          .subpassCount = static_cast<uint32_t>(m_info.subpasses.size()),
          .pSubpasses = m_info.subpasses.data(),
          .dependencyCount = static_cast<uint32_t>(m_info.subpass_dependencies.size()),
          .pDependencies = m_info.subpass_dependencies.data()}
    , m_pass_handler{avk::create_render_pass(avk::context::device()->createRenderPass(m_pass_info))}
{
}


const vk::RenderPassCreateInfo& avk::render_pass_wrapper::get_info() const
{
    return m_pass_info;
}


vk::RenderPass avk::render_pass_wrapper::get_native_pass() const
{
    return m_pass_handler.as<vk::RenderPass>();
}
