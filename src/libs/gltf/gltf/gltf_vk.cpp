

#include "gltf_vk.hpp"

#include <gltf/vk_utils.hpp>

#include <render/vk/errors_handling.hpp>
#include <render/vk/utils.hpp>
#include <utils/scope_helpers.hpp>

#include <filesystem/common_file.hpp>

#include <stb/stb_image.h>

#include <filesystem>

using namespace sandbox;
using namespace sandbox::hal::render;

namespace
{
    std::pair<avk::vma_buffer, avk::vma_buffer> create_buffer(
        vk::CommandBuffer& command_buffer,
        uint32_t queue_family,
        uint32_t buffer_size,
        vk::BufferUsageFlagBits buffer_usage,
        vk::AccessFlagBits access_flags,
        const std::function<void(const uint8_t* dst)>& on_mapped_callback)
    {
        auto staging_buffer = avk::gen_staging_buffer(
            queue_family,
            buffer_size,
            on_mapped_callback);

        auto result_buffer = avk::create_vma_buffer(
            vk::BufferCreateInfo{
                .flags = {},
                .size = buffer_size,
                .usage = vk::BufferUsageFlagBits::eTransferDst | buffer_usage,
                .sharingMode = vk::SharingMode::eExclusive,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &queue_family},
            VmaAllocationCreateInfo{
                .usage = VMA_MEMORY_USAGE_GPU_ONLY});

        command_buffer.copyBuffer(
            staging_buffer.as<vk::Buffer>(),
            result_buffer.as<vk::Buffer>(),
            {vk::BufferCopy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = buffer_size,
            }});

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexInput,
            {},
            {vk::MemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = access_flags,
            }},
            {},
            {});

        return std::make_pair(std::move(staging_buffer), std::move(result_buffer));
    }
} // namespace


gltf::vk_geometry gltf::vk_geometry::from_gltf_model(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family)
{
    gltf::vk_geometry new_geom{};

    new_geom.m_primitives.reserve(mdl.get_meshes().size());

    const auto& accessors = mdl.get_accessors();
    const auto& buffer_views = mdl.get_buffer_views();
    const auto& buffers = mdl.get_buffers();

    uint32_t vertex_buffer_size = 0;
    uint32_t index_buffer_size = 0;

    for (const auto& gltf_mesh : mdl.get_meshes()) {
        auto& new_mesh = new_geom.m_primitives.emplace_back();
        new_mesh.reserve(gltf_mesh.get_primitives().size());

        for (const auto& gltf_primitive : gltf_mesh.get_primitives()) {
            auto attrs_bindings_size = gltf_primitive.get_attributes().size();

            auto& new_primitive = new_mesh.emplace_back();
            new_primitive.attributes.reserve(attrs_bindings_size);
            new_primitive.bindings.reserve(attrs_bindings_size);
            new_primitive.vertex_buffer_offset = vertex_buffer_size;

            uint32_t curr_attr_binding_index = 0;

            for (const auto& attr : gltf_primitive.get_attributes()) {
                const auto& attr_accessor = accessors[attr];
                if (new_primitive.vertices_count == 0) {
                    new_primitive.vertices_count = attr_accessor.get_count();
                } else {
                    if (new_primitive.vertices_count != attr_accessor.get_count()) {
                        throw std::runtime_error("Bad primitive");
                    }
                }

                const auto vert_attr_fmt = to_vk_format(attr_accessor.get_type(), attr_accessor.get_component_type());

                new_primitive.attributes.emplace_back(vk::VertexInputAttributeDescription{
                    .location = curr_attr_binding_index,
                    .binding = curr_attr_binding_index,
                    .format = vert_attr_fmt,
                    .offset = vertex_buffer_size,
                });

                new_primitive.bindings.emplace_back(vk::VertexInputBindingDescription{
                    .binding = curr_attr_binding_index,
                    .stride = avk::get_format_info(static_cast<VkFormat>(vert_attr_fmt)).size,
                    .inputRate = vk::VertexInputRate::eVertex,
                });

                curr_attr_binding_index++;

                vertex_buffer_size += attr_accessor.get_data_size();
            }

            if (gltf_primitive.get_indices() >= 0) {
                const auto& indices_accessor = accessors[gltf_primitive.get_indices()];
                new_primitive.index_type = to_vk_index_type(indices_accessor.get_type(), indices_accessor.get_component_type());
                new_primitive.index_buffer_offset = index_buffer_size;
                new_primitive.indices_count = indices_accessor.get_count();

                index_buffer_size += indices_accessor.get_data_size();
            }
        }
    }

    auto [vertex_staging_buffer, vertex_buffer] = create_buffer(
        command_buffer,
        queue_family,
        vertex_buffer_size,
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::AccessFlagBits::eVertexAttributeRead,
        [&](const uint8_t* dst) {
            auto vk_mesh = new_geom.m_primitives.begin();
            for (const auto& gltf_mesh : mdl.get_meshes()) {
                auto vk_primitive = vk_mesh->begin();
                for (const auto& gltf_primitive : gltf_mesh.get_primitives()) {
                    auto vk_attr = vk_primitive->attributes.begin();
                    for (const auto& attr : gltf_primitive.get_attributes()) {
                        const auto& accessor = accessors[attr];
                        std::memcpy(
                            (void*) (dst + vk_primitive->vertex_buffer_offset + vk_attr->offset),
                            accessor.get_data(
                                buffers.data(),
                                buffers.size(),
                                buffer_views.data(),
                                buffer_views.size()),
                            accessor.get_data_size());
                        vk_attr++;
                    }
                    vk_primitive++;
                }
                vk_mesh++;
            }
        });

    auto [index_staging_buffer, index_buffer] = create_buffer(
        command_buffer,
        queue_family,
        index_buffer_size,
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::AccessFlagBits::eIndexRead,
        [&](const uint8_t* dst) {
            auto vk_mesh = new_geom.m_primitives.begin();
            for (const auto& gltf_mesh : mdl.get_meshes()) {
                auto vk_primitive = vk_mesh->begin();
                for (const auto& gltf_primitive : gltf_mesh.get_primitives()) {
                    for (const auto& attr : gltf_primitive.get_attributes()) {
                        if (gltf_primitive.get_indices() >= 0) {
                            const auto& accessor = accessors[gltf_primitive.get_indices()];
                            std::memcpy(
                                (void*) (dst + vk_primitive->index_buffer_offset),
                                accessor.get_data(
                                    buffers.data(),
                                    buffers.size(),
                                    buffer_views.data(),
                                    buffer_views.size()),
                                accessor.get_data_size());
                        }
                    }
                    vk_primitive++;
                }
            }
        });

    new_geom.m_vertex_buffer = std::move(vertex_buffer);
    new_geom.m_vertex_staging_buffer = std::move(vertex_staging_buffer);
    new_geom.m_index_buffer = std::move(index_buffer);
    new_geom.m_index_staging_buffer = std::move(index_staging_buffer);

    return new_geom;
}


vk::Buffer gltf::vk_geometry::get_vertex_buffer() const
{
    return m_vertex_buffer.as<vk::Buffer>();
}


vk::Buffer gltf::vk_geometry::get_index_buffer() const
{
    return m_index_buffer.as<vk::Buffer>();
}


void gltf::vk_geometry::clear_staging_resources()
{
    m_vertex_staging_buffer = {};
    m_index_staging_buffer = {};
}


const std::vector<gltf::vk_primitive>& gltf::vk_geometry::get_primitives(uint32_t mesh) const
{
    return m_primitives[mesh];
}


gltf::vk_texture_atlas gltf::vk_texture_atlas::from_gltf_model(
    const gltf::model& mdl,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    vk_texture_atlas new_atlas{};
    new_atlas.m_images.reserve(mdl.get_images().size());
    new_atlas.m_image_views.reserve(mdl.get_images().size());
    new_atlas.m_staging_resources.reserve(mdl.get_images().size());
    new_atlas.m_image_sizes.reserve(mdl.get_images().size());


    new_atlas.m_samplers.reserve(mdl.get_textures().size());
    new_atlas.m_vk_textures.reserve(mdl.get_textures().size());

    const auto& buffers = mdl.get_buffers();
    const auto& buffer_views = mdl.get_buffer_views();

    struct tex_pixel_data
    {
        int w, h, c;
        std::unique_ptr<void, std::function<void(void*)>> handler{nullptr, [](void* ptr) {if (ptr) stbi_image_free(ptr); }};
    };

    for (const auto& image : mdl.get_images()) {
        tex_pixel_data pix_data{};

        if (image.get_buffer_view() >= 0) {
            const auto& buffer_view = buffer_views[image.get_buffer_view()];
            const auto* data_ptr = buffer_view.get_data(buffers.data(), buffers.size());

            pix_data.handler.reset(stbi_load_from_memory(
                data_ptr, buffer_view.get_byte_length(), &pix_data.w, &pix_data.h, &pix_data.c, 0));
        } else if (!image.get_uri().empty()) {
            pix_data.handler.reset(stbi_load(image.get_uri().c_str(), &pix_data.w, &pix_data.h, &pix_data.c, 0));
        } else {
            throw std::runtime_error("Bad image.");
        }

        if (pix_data.handler == nullptr) {
            throw std::runtime_error("Cannot load image.");
        }

        vk::Format format = gltf::stb_channels_count_to_vk_format(pix_data.c);
        const auto tex_fmt_info = avk::get_format_info(static_cast<VkFormat>(format));
        size_t staging_buffer_size = pix_data.w * pix_data.h * tex_fmt_info.size;

        std::vector<uint8_t> rearranged_pix_data{};

        if (pix_data.c == 3) {
            rearranged_pix_data.reserve(pix_data.w * pix_data.h * 4);

            auto pixels = reinterpret_cast<uint8_t*>(pix_data.handler.get());

            for (uint64_t y = 0; y < pix_data.h; ++y) {
                for (uint64_t x = 0; x < pix_data.w; ++x) {
                    rearranged_pix_data.push_back(pixels[(y * pix_data.w + x) * 3 + 0]);
                    rearranged_pix_data.push_back(pixels[(y * pix_data.w + x) * 3 + 1]);
                    rearranged_pix_data.push_back(pixels[(y * pix_data.w + x) * 3 + 2]);
                    rearranged_pix_data.push_back(255);
                }
            }

            staging_buffer_size = rearranged_pix_data.size();
            format = vk::Format::eR8G8B8A8Srgb;

            auto staging_buffer = avk::gen_staging_buffer(
                queue_family,
                staging_buffer_size,
                [&pix_data, &rearranged_pix_data, staging_buffer_size](uint8_t* data) {
                    std::memcpy(data, rearranged_pix_data.empty() ? pix_data.handler.get() : rearranged_pix_data.data(), staging_buffer_size);
                });

            auto [image, image_view] = avk::gen_texture(
                queue_family, vk::ImageViewType::e2D, format, pix_data.w, pix_data.h);

            avk::copy_buffer_to_image(
                command_buffer,
                staging_buffer.as<vk::Buffer>(),
                image.as<vk::Image>(),
                pix_data.w,
                pix_data.h,
                1,
                1);

            new_atlas.m_images.emplace_back(std::move(image));
            new_atlas.m_image_views.emplace_back(std::move(image_view));
            new_atlas.m_staging_resources.emplace_back(std::move(staging_buffer));
            new_atlas.m_image_sizes.emplace_back(pix_data.w, pix_data.h);
        }
    }

    const auto& samplers = mdl.get_samplers();

    for (const auto& gltf_texture : mdl.get_textures()) {
        const auto& gltf_sampler = samplers[gltf_texture.get_sampler()];
        const auto [width, height] = new_atlas.m_image_sizes[gltf_texture.get_image()];

        auto max_lod = std::max(std::log2f(width), std::log2f(height));

        auto sampler = avk::gen_sampler(
            gltf::to_vk_sampler_filter(gltf_sampler.get_mag_filter()).first,
            gltf::to_vk_sampler_filter(gltf_sampler.get_min_filter()).first,
            gltf::to_vk_sampler_filter(gltf_sampler.get_min_filter()).second,
            gltf::to_vk_sampler_wrap(gltf_sampler.get_wrap_s()),
            gltf::to_vk_sampler_wrap(gltf_sampler.get_wrap_t()),
            vk::SamplerAddressMode::eRepeat,
            max_lod);

        new_atlas.m_vk_textures.emplace_back(
            vk_texture{
                .image = new_atlas.m_images[gltf_texture.get_image()].as<vk::Image>(),
                .image_view = new_atlas.m_image_views[gltf_texture.get_image()].as<vk::ImageView>(),
                .sampler = sampler.as<vk::Sampler>()});

        new_atlas.m_samplers.emplace_back(std::move(sampler));
    }

    return new_atlas;
}


const gltf::vk_texture& gltf::vk_texture_atlas::get_texture(uint32_t index) const
{
    return m_vk_textures[index];
}
