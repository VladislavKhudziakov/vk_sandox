#include "image.hpp"

#include <render/vk/utils.hpp>

#include <utils/conditions_helpers.hpp>
#include <utils/scope_helpers.hpp>
#include "pass.hpp"

using namespace sandbox::hal::render;


avk::image_instance::image_instance(image_pool& pool)
    : m_pool(&pool)
{
}


vk::Format avk::image_instance::get_format() const
{
    return m_format;
}


uint32_t avk::image_instance::get_width() const
{
    return m_width;
}


uint32_t avk::image_instance::get_height() const
{
    return m_height;
}


uint32_t avk::image_instance::get_depth() const
{
    return m_depth;
}


uint32_t avk::image_instance::get_layers() const
{
    return m_layers;
}


uint32_t avk::image_instance::get_faces() const
{
    return m_faces;
}


uint32_t avk::image_instance::get_mips_levels() const
{
    return m_mips_levels;
}


avk::image_instance::operator vk::Image() const
{
    return m_pool->get_subresource_image(m_subresource_index);
}


avk::image_instance::operator vk::ImageView() const
{
    return m_pool->get_subresource_image_view(m_subresource_index);
}


void avk::image_instance::upload(std::function<void(uint8_t*)> cb)
{
    m_pool->update_subresource(m_subresource_index, [offset = m_staging_offset, cb = std::move(cb)](uint8_t* dst) {
        cb(dst + offset);
    });
}

avk::image_builder& avk::image_builder::set_format(vk::Format format)
{
    m_format = format;
    return *this;
}


avk::image_builder& avk::image_builder::set_width(size_t width)
{
    m_width = width;
    return *this;
}


avk::image_builder& avk::image_builder::set_height(size_t height)
{
    m_height = height;
    return *this;
}


avk::image_builder& avk::image_builder::set_depth(size_t depth)
{
    m_depth = depth;
    return *this;
}


avk::image_builder& avk::image_builder::set_layers(size_t layers)
{
    m_layers = layers;
    return *this;
}


avk::image_builder& avk::image_builder::set_faces(size_t faces)
{
    m_faces = faces;
    return *this;
}


avk::image_builder& avk::image_builder::set_mips_levels(size_t levels)
{
    m_mips_levels = levels;
    return *this;
}


avk::image_builder& avk::image_builder::gen_mips(bool gen)
{
    m_gen_mips = gen;
    return *this;
}


avk::image_instance avk::image_builder::create(std::function<void(uint8_t*)> cb)
{
    CHECK(m_width > 0);
    CHECK(m_height > 0);
    CHECK(m_depth > 0);
    CHECK(m_layers > 0);
    CHECK(m_mips_levels > 0);
    CHECK(m_faces == 1 || m_faces == 6);
    CHECK(m_format != vk::Format::eUndefined);

    if (m_depth != 1) {
        CHECK(m_faces == 1 && m_layers == 1);
    }

    if (m_gen_mips) {
        CHECK(m_mips_levels == 1);
        CHECK(cb);
    }

    image_instance result(m_pool);

    result.m_width = m_width;
    result.m_height = m_height;
    result.m_depth = m_depth;
    result.m_layers = m_layers;

    if (m_gen_mips) {
        result.m_mips_levels = std::max(std::max(log2f(m_width), log2f(m_height)), log2f(m_depth)) + 1;
    } else {
        result.m_mips_levels = m_mips_levels;
    }

    result.m_faces = m_faces;
    result.m_format = m_format;

    m_pool.add_image_instance(result, m_gen_mips, (bool) cb);
    result.upload(cb);

    return result;
}


sandbox::hal::render::avk::image_builder::image_builder(image_pool& pool)
    : m_pool(pool)
{
}


void avk::image_pool::add_image_instance(image_instance& instance, bool gen_mips, bool reserve_staging_space)
{
    instance.m_subresource_index = m_subresources.size();

    image_subresource subresource{
        .width = instance.m_width,
        .height = instance.m_height,
        .depth = instance.m_depth,
        .layers = instance.m_layers,
        .faces = instance.m_faces,
        .levels = instance.m_mips_levels,
        .format = instance.m_format,
        .reserve_staging_space = reserve_staging_space,
        .gen_mips = gen_mips,
    };

    if (reserve_staging_space) {
        m_staging_buffer_size = get_aligned_size(m_staging_buffer_size, get_format_info(instance.m_format).size);

        subresource.m_staging_offset = m_staging_buffer_size;
        instance.m_staging_offset = m_staging_buffer_size;

        for (size_t i = 0; i < subresource.levels; i++) {
            m_staging_buffer_size += get_subresource_level_size(subresource, i);
        }
    }

    m_subresources.emplace_back(std::move(subresource));
}


vk::Image avk::image_pool::get_subresource_image(uint32_t s) const
{
    return m_subresources[s].m_image.as<vk::Image>();
}


vk::ImageView avk::image_pool::get_subresource_image_view(uint32_t s) const
{
    return m_subresources[s].m_image_view.as<vk::ImageView>();
}


avk::image_builder avk::image_pool::get_builder()
{
    return image_builder(*this);
}


void avk::image_pool::update_subresource(uint32_t subresource, std::function<void(uint8_t* dst)> cb)
{
    m_subresources_to_update.emplace(subresource);
    m_upload_callbacks.emplace_back(std::move(cb));
}


void avk::image_pool::update(vk::CommandBuffer& command_buffer)
{
    if (!m_upload_callbacks.empty()) {
        void* dst_ptr{nullptr};
        auto res = vmaMapMemory(avk::context::allocator(), m_staging_buffer.as<VmaAllocation>(), &dst_ptr);

        utils::on_scope_exit scope_guard{[this, res]() {
            if (res == VK_SUCCESS) {
                vmaUnmapMemory(avk::context::allocator(), m_staging_buffer.as<VmaAllocation>());
                VkDeviceSize offset = 0;
                VkDeviceSize size = m_staging_buffer->get_alloc_info().size;
                VmaAllocation allocation = m_staging_buffer.as<VmaAllocation>();
                vmaFlushAllocations(avk::context::allocator(), 1, &allocation, &offset, &size);
            }
        }};

        ASSERT(res == VK_SUCCESS);

        for (const auto& cb : m_upload_callbacks) {
            cb(reinterpret_cast<uint8_t*>(dst_ptr));
        }

        m_upload_callbacks.clear();
    }


    for (auto& subres : m_subresources_to_update) {
        if (m_subresources[subres].reserve_staging_space) {
            copy_subres_data(command_buffer, m_subresources[subres]);
        }
    }

    m_subresources_to_update.clear();
}


void avk::image_pool::gen_subresource_images(image_subresource& subres, uint32_t queue_family)
{
    vk::ImageViewType type{};

    if (subres.faces != 1) {
        type = subres.layers == 1 ? vk::ImageViewType::eCube : vk::ImageViewType::eCubeArray;
    } else if (subres.depth != 1) {
        type = vk::ImageViewType::e3D;
    } else {
        type = subres.layers == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
    }

    auto [image, view] = avk::gen_texture(
        queue_family,
        type,
        subres.format,
        subres.width,
        subres.height,
        subres.depth,
        subres.levels,
        subres.layers * subres.faces);

    subres.m_image = std::move(image);
    subres.m_image_view = std::move(view);
}


void avk::image_pool::copy_subres_data(
    vk::CommandBuffer& command_buffer,
    const image_subresource& subres)
{
    if (subres.gen_mips) {
        image_pipeline_barrier(
            command_buffer,
            subres,
            0,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            {},
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal);

        uint32_t level_offset{0};
        copy_subres_level(subres, 0, command_buffer, level_offset);
        gen_subres_mips(command_buffer, subres, 1);

    } else {
        uint32_t level_offset{0};

        for (size_t i = 0; i < subres.levels; i++) {
            image_pipeline_barrier(
                command_buffer,
                subres,
                i,
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer,
                {},
                {},
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal);

            copy_subres_level(subres, i, command_buffer, level_offset);

            image_pipeline_barrier(
                command_buffer,
                subres,
                i,
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eTransferWrite,
                vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
}


void avk::image_pool::copy_subres_level(
    const image_subresource& subres,
    uint32_t level,
    vk::CommandBuffer& command_buffer,
    uint32_t& level_offset)
{
    auto level_width = std::max(subres.width >> level, 1u);
    auto level_height = std::max(subres.height >> level, 1u);
    auto level_depth = std::max(subres.depth >> level, 1u);

    auto buffer_offset = subres.m_staging_offset + level_offset;

    vk::BufferImageCopy copy_data{
        .bufferOffset = buffer_offset,
        .bufferRowLength = level_width,
        .bufferImageHeight = level_height,

        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = level,
            .baseArrayLayer = 0,
            .layerCount = subres.layers * subres.faces},

        .imageOffset = {.x = 0, .y = 0, .z = 0},

        .imageExtent = {.width = level_width, .height = level_height, .depth = level_depth},
    };

    command_buffer.copyBufferToImage(
        m_staging_buffer.as<vk::Buffer>(),
        subres.m_image.as<vk::Image>(),
        vk::ImageLayout::eTransferDstOptimal,
        {copy_data});

    level_offset += get_subresource_level_size(subres, level);
}


void avk::image_pool::gen_subres_mips(
    vk::CommandBuffer& command_buffer,
    const image_subresource& subres,
    uint32_t level)
{
    if (level >= subres.levels) {
        image_pipeline_barrier(
            command_buffer,
            subres,
            level - 1,
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        return;
    }

    int32_t prev_level_width = std::max(subres.width >> level - 1, 1u);
    int32_t prev_level_height = std::max(subres.height >> level - 1, 1u);

    int32_t curr_level_width = std::max(subres.width >> level, 1u);
    int32_t curr_level_height = std::max(subres.height >> level, 1u);

    // clang-format off
    vk::ImageBlit blit_region{
        .srcSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = level - 1,
            .baseArrayLayer = 0,
            .layerCount = subres.layers,
        },
        .srcOffsets = {
            {
                vk::Offset3D{.x = 0, .y = 0, .z = 0}, 
                vk::Offset3D{.x = prev_level_width, .y = prev_level_height, .z = 1}
            }
        },

        .dstSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = level,
            .baseArrayLayer = 0,
            .layerCount = subres.layers,
        },
        .dstOffsets = {
            {
                vk::Offset3D{.x = 0, .y = 0, .z = 0}, 
                vk::Offset3D{.x = curr_level_width, .y = curr_level_height, .z = 1}
            }
        }
    };
    // clang-format on

    image_pipeline_barrier(
        command_buffer,
        subres,
        level - 1,
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eTransferRead,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eTransferSrcOptimal);

    image_pipeline_barrier(
        command_buffer,
        subres,
        level,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal);

    command_buffer.blitImage(
        subres.m_image.as<vk::Image>(),
        vk::ImageLayout::eTransferSrcOptimal,
        subres.m_image.as<vk::Image>(),
        vk::ImageLayout::eTransferDstOptimal,
        {blit_region},
        vk::Filter::eLinear);

    image_pipeline_barrier(
        command_buffer,
        subres,
        level - 1,
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal);

    gen_subres_mips(command_buffer, subres, level + 1);
}


VkDeviceSize avk::image_pool::get_subresource_level_size(const image_subresource& subres, uint32_t level)
{
    auto level_width = std::max(subres.width >> level, 1u);
    auto level_height = std::max(subres.height >> level, 1u);

    auto info = get_format_info(subres.format);

    return level_width * level_height * subres.depth * subres.layers * subres.faces * info.size;
}

void avk::image_pool::image_pipeline_barrier(
    vk::CommandBuffer& command_buffer,
    const image_subresource& subres,
    uint32_t subres_level,
    vk::PipelineStageFlags src_stage,
    vk::PipelineStageFlags dst_stage,
    vk::AccessFlags src_access,
    vk::AccessFlags dst_access,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout)
{
    vk::ImageMemoryBarrier barrier{
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,

        .oldLayout = old_layout,
        .newLayout = new_layout,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image = subres.m_image.as<vk::Image>(),

        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = subres_level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = subres.layers * subres.faces,
        },
    };

    command_buffer.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, {barrier});
}


void avk::image_pool::flush(uint32_t queue_family, vk::CommandBuffer& command_buffer)
{
    if (m_staging_buffer_size > 0) {
        m_staging_buffer = avk::gen_staging_buffer(queue_family, m_staging_buffer_size, [this](uint8_t* dst) {
            for (const auto& cb : m_upload_callbacks) {
                cb(dst);
            }
        });
    }


    for (auto& subres : m_subresources) {
        gen_subresource_images(subres, queue_family);
        if (subres.reserve_staging_space) {
            copy_subres_data(command_buffer, subres);
        }
    }
}
