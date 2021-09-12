#include "utils.hpp"

#include <render/vk/errors_handling.hpp>

#include <utils/scope_helpers.hpp>
#include <utils/conditions_helpers.hpp>


using namespace sandbox::hal::render;


std::pair<avk::vma_image, avk::image_view> sandbox::hal::render::avk::gen_attachment(
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


avk::vma_buffer avk::gen_staging_buffer(
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


avk::framebuffer avk::gen_framebuffer(
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
avk::gen_framebuffer(
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

        auto [image, view] = gen_attachment(
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

    auto framebuffer = gen_framebuffer(width, height, pass, image_views_native.data(), image_views_native.size());

    return std::make_tuple(std::move(framebuffer), std::move(images), std::move(images_views));
}


void avk::copy_buffer_to_image(
    vk::CommandBuffer command_buffer,
    const vk::Buffer& staging_buffer,
    const vk::Image& dst_image,
    uint32_t image_width,
    uint32_t image_height,
    uint32_t image_level_count,
    uint32_t image_layers_count,
    size_t buffer_offset)
{
    // clang-format off
    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        {},
        {vk::ImageMemoryBarrier{
            .srcAccessMask = {},
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = image_level_count,
                .baseArrayLayer = 0,
                .layerCount = image_layers_count
            },
        }});

    command_buffer.copyBufferToImage(
        staging_buffer,
        dst_image,
        vk::ImageLayout::eTransferDstOptimal,
        {vk::BufferImageCopy{
            .bufferOffset = static_cast<VkDeviceSize>(buffer_offset),
            .bufferRowLength = image_width,
            .bufferImageHeight = image_height,

            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },

            .imageOffset = {
                0, 0, 0
            },

            .imageExtent = {
                image_width, image_height, 1
            },
        }});

    vk::PipelineStageFlags all_shaders_flags =
        vk::PipelineStageFlagBits::eVertexShader |
        vk::PipelineStageFlagBits::eGeometryShader |
        vk::PipelineStageFlagBits::eTessellationControlShader |
        vk::PipelineStageFlagBits::eTessellationEvaluationShader |
        vk::PipelineStageFlagBits::eComputeShader |
        vk::PipelineStageFlagBits::eFragmentShader;


    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        all_shaders_flags,
        {},
        {},
        {},
        {vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = image_level_count,
                .baseArrayLayer = 0,
                .layerCount = image_layers_count
                },
            }});

    // clang-format on
}


avk::descriptor_set_layout avk::gen_descriptor_set_layout(uint32_t count, vk::DescriptorType type)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings{};
    bindings.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        bindings.emplace_back(vk::DescriptorSetLayoutBinding{
            .binding = i,
            .descriptorType = type,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll,
            .pImmutableSamplers = nullptr
        });
    }

    return avk::create_descriptor_set_layout(avk::context::device()->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    }));
}


std::pair<avk::descriptor_pool, avk::descriptor_set_list> avk::gen_descriptor_sets(
    const std::vector<vk::DescriptorSetLayout>& layouts,
    const std::vector<std::pair<uint32_t, vk::DescriptorType>>& layouts_data)
{
    std::vector<vk::DescriptorPoolSize> descriptor_pool_sizes{};
    descriptor_pool_sizes.reserve(layouts_data.size());

    auto descriptor_pool = avk::create_descriptor_pool(avk::context::device()->createDescriptorPool(
        vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = static_cast<uint32_t>(layouts.size()),
            .poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size()),
            .pPoolSizes = descriptor_pool_sizes.data(),
        }
    ));

    auto descriptor_sets = avk::allocate_descriptor_sets(vk::DescriptorSetAllocateInfo {
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()
    });

    return std::make_pair(std::move(descriptor_pool), std::move(descriptor_sets));
}


void avk::write_texture_descriptors(
    vk::DescriptorSet dst_set,
    const std::vector<vk::ImageView>& images_views,
    const std::vector<vk::Sampler>& samplers)
{
    CHECK(images_views.size() == samplers.size());

    std::vector<vk::DescriptorImageInfo> images_infos{};
    images_infos.reserve(images_views.size());

    for (int i = 0; i < images_views.size(); ++i) {
        images_infos.emplace_back(vk::DescriptorImageInfo{
            .sampler = samplers[i],
            .imageView = images_views[i],
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        });
    }

    // clang-format off
    avk::context::device()->updateDescriptorSets({
        vk::WriteDescriptorSet{
            .dstSet = dst_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(images_infos.size()),
            .pImageInfo = images_infos.data(),
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }}, {});
    // clang-format on
}


void avk::write_buffer_descriptors(
    vk::DescriptorSet dst_set,
    const std::vector<vk::Buffer>& buffers,
    const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>& sizes)
{
    CHECK(buffers.size() == sizes.size());

    std::vector<vk::DescriptorBufferInfo> buffers_infos{};

    for (int i = 0; i < buffers.size(); ++i) {
        buffers_infos.emplace_back(vk::DescriptorBufferInfo{
            .buffer = buffers[i],
            .offset = sizes[i].first,
            .range = sizes[i].second
        });
    }

    // clang-format off
    avk::context::device()->updateDescriptorSets({
        vk::WriteDescriptorSet{
            .dstSet = dst_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(buffers_infos.size()),
            .pImageInfo = nullptr,
            .pBufferInfo = buffers_infos.data(),
            .pTexelBufferView = nullptr
        }}, {});
    // clang-format on
}


avk::pipeline_layout avk::gen_pipeline_layout(
    const std::vector<vk::PushConstantRange>& push_constants,
    const std::vector<vk::DescriptorSetLayout>& desc_set_layouts)
{
    return avk::create_pipeline_layout(avk::context::device()->createPipelineLayout(vk::PipelineLayoutCreateInfo{
        .setLayoutCount = static_cast<uint32_t>(desc_set_layouts.size()),
        .pSetLayouts = desc_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
        .pPushConstantRanges = push_constants.data()}));

    return sandbox::hal::render::avk::pipeline_layout();
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
