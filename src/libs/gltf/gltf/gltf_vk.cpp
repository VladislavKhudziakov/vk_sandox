#include "gltf_vk.hpp"

#include <gltf/vk_utils.hpp>

#include <render/vk/utils.hpp>

#include <stb/stb_image.h>

#include <filesystem>

using namespace sandbox;
using namespace sandbox::hal::render;


gltf::vk_geometry gltf::vk_geometry::from_gltf_model(
    const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family)
{
    gltf::vk_geometry new_geom{};

    create_meshes_data(
        mdl,
        new_geom.m_primitives,
        new_geom.m_vertex_staging_buffer,
        new_geom.m_vertex_buffer,
        new_geom.m_index_staging_buffer,
        new_geom.m_index_buffer,
        command_buffer,
        queue_family);

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


void gltf::vk_geometry::create_meshes_data(
    const gltf::model& mdl,
    std::vector<std::vector<vk_primitive>>& primitives,
    avk::vma_buffer& result_v_staging_buffer,
    avk::vma_buffer& result_v_dst_buffer,
    avk::vma_buffer& result_i_staging_buffer,
    avk::vma_buffer& result_i_dst_buffer,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    primitives.clear();
    primitives.reserve(mdl.get_meshes().size());

    const auto& accessors = mdl.get_accessors();
    const auto& buffer_views = mdl.get_buffer_views();
    const auto& buffers = mdl.get_buffers();

    uint32_t vertex_buffer_size = 0;
    uint32_t index_buffer_size = 0;

    for (const auto& gltf_mesh : mdl.get_meshes()) {
        auto& new_mesh = primitives.emplace_back();
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

    auto [vertex_staging_buffer, vertex_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        vertex_buffer_size,
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::AccessFlagBits::eVertexAttributeRead,
        [&](const uint8_t* dst) {
            auto vk_mesh = primitives.begin();
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

    auto [index_staging_buffer, index_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        index_buffer_size,
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::AccessFlagBits::eIndexRead,
        [&](const uint8_t* dst) {
            auto vk_mesh = primitives.begin();
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

    result_v_dst_buffer = std::move(vertex_buffer);
    result_v_staging_buffer = std::move(vertex_staging_buffer);
    result_i_dst_buffer = std::move(index_buffer);
    result_i_staging_buffer = std::move(index_staging_buffer);
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
            const auto abs_path = (std::filesystem::path(mdl.get_cwd()) / image.get_uri()).string();
            pix_data.handler.reset(stbi_load(abs_path.c_str(), &pix_data.w, &pix_data.h, &pix_data.c, 0));
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
        }

        auto staging_buffer = avk::gen_staging_buffer(
            queue_family,
            staging_buffer_size,
            [&pix_data, &rearranged_pix_data, staging_buffer_size](uint8_t* data) {
                std::memcpy(data, rearranged_pix_data.empty() ? pix_data.handler.get() : rearranged_pix_data.data(), staging_buffer_size);
            });

        auto [vk_image, vk_image_view] = avk::gen_texture(
            queue_family, vk::ImageViewType::e2D, format, pix_data.w, pix_data.h);

        avk::copy_buffer_to_image(
            command_buffer,
            staging_buffer.as<vk::Buffer>(),
            vk_image.as<vk::Image>(),
            pix_data.w,
            pix_data.h,
            1,
            1);

        new_atlas.m_images.emplace_back(std::move(vk_image));
        new_atlas.m_image_views.emplace_back(std::move(vk_image_view));
        new_atlas.m_staging_resources.emplace_back(std::move(staging_buffer));
        new_atlas.m_image_sizes.emplace_back(pix_data.w, pix_data.h);
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


gltf::vk_geometry_skins gltf::vk_geometry_skins::from_gltf_model(
    const gltf::model& mdl,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    auto new_skins = vk_geometry_skins{};

    create_skins_data(
        mdl,
        new_skins.m_skins,
        new_skins.m_skin_staging_buffer,
        new_skins.m_skin_buffer,
        command_buffer, queue_family);

    create_hierarchy_data(
        mdl,
        new_skins.m_default_hierarchy_transforms,
        new_skins.m_hierarchy_staging_buffer,
        new_skins.m_hierarchy_buffer,
        command_buffer,
        queue_family);

    return new_skins;
}


void gltf::vk_geometry_skins::create_skins_data(
    const gltf::model& mdl,
    std::vector<vk_skin>& skins,
    hal::render::avk::vma_buffer& result_staging_buffer,
    hal::render::avk::vma_buffer& result_dst_buffer,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    const auto& accessors = mdl.get_accessors();
    const auto& buffer_views = mdl.get_buffer_views();
    const auto& buffers = mdl.get_buffers();

    std::vector<std::vector<bone_data>> bones_data{};
    bones_data.reserve(mdl.get_skins().size() + 1);

    skins.clear();
    skins.reserve(mdl.get_skins().size() + 1);

    size_t skins_buffer_size = 0;

    for (const auto& skin : mdl.get_skins()) {
        const auto& joints = skin.get_joints();

        auto& new_bones_data = bones_data.emplace_back();
        const auto inv_bind_poses_accessor = accessors[skin.get_inv_bind_matrices()];

        const auto* inv_bind_poses = reinterpret_cast<const glm::mat4*>(
            inv_bind_poses_accessor.get_data(buffers.data(), buffers.size(), buffer_views.data(), buffer_views.size()));

        new_bones_data.reserve(joints.size());

        for (const auto joint : joints) {
            new_bones_data.emplace_back(bone_data{
                .inv_bind_pose = *inv_bind_poses++,
                .joint = static_cast<uint32_t>(joint)
            });
        }

        const auto skin_data_size = new_bones_data.size() * sizeof(new_bones_data.front());

        skins.emplace_back(vk_skin{
            .offset = skins_buffer_size,
            .size = skin_data_size
        });

        skins_buffer_size += skin_data_size;
    }

    bones_data.emplace_back();
    skins.emplace_back(vk_skin{
        .offset = skins_buffer_size,
        .size = sizeof(bone_data)
    });

    auto [staging_buffer, dst_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        bones_data.size() * sizeof(bone_data),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::PipelineStageFlagBits::eVertexShader,
        vk::AccessFlagBits::eUniformRead,
        [&](const uint8_t* dst){
            for (const auto& skin : skins) {
                std::memcpy((void*)(dst + skin.offset), bones_data.data(), skin.size);
            }
        });

    result_staging_buffer = std::move(staging_buffer);
    result_dst_buffer = std::move(dst_buffer);
}


void gltf::vk_geometry_skins::create_hierarchy_data(
    const gltf::model& model,
    std::vector<glm::mat4>& default_data,
    avk::vma_buffer& result_staging_buffer,
    avk::vma_buffer& result_dst_buffer,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    default_data.clear();
    default_data.resize(model.get_nodes().size());

    glm::mat4 parent_transform{1};

    for_each_scene_node(
        model,
        [&default_data](const gltf::node& node, int32_t node_index, const glm::mat4& parent_transform) {
            default_data[node_index] = parent_transform * node.get_matrix();
            return default_data[node_index];
            },
            parent_transform);

    auto [staging_buffer, dst_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        default_data.size() * sizeof(default_data.front()),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::PipelineStageFlagBits::eVertexShader,
        vk::AccessFlagBits::eUniformRead,
        [&](const uint8_t* dst){
            std::memcpy((void*)dst, default_data.data(), default_data.size() * sizeof(default_data.front()));
        });

    result_staging_buffer = std::move(staging_buffer);
    result_dst_buffer = std::move(dst_buffer);
}


const std::vector<gltf::vk_skin>& gltf::vk_geometry_skins::get_skins() const
{
    return m_skins;
}


const std::vector<glm::mat4>& gltf::vk_geometry_skins::get_hierarchy_transforms() const
{
    return m_default_hierarchy_transforms;
}


const hal::render::avk::vma_buffer& gltf::vk_geometry_skins::get_hierarchy_buffer() const
{
    return m_hierarchy_buffer;
}


const hal::render::avk::vma_buffer& gltf::vk_geometry_skins::get_skin_buffer() const
{
    return m_skin_buffer;
}


const hal::render::avk::vma_buffer& gltf::vk_geometry_skins::get_hierarchy_staging_buffer() const
{
    return m_hierarchy_staging_buffer;
}
