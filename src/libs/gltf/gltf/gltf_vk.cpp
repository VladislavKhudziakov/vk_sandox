#include "gltf_vk.hpp"

#include <gltf/vk_utils.hpp>
#include <render/vk/utils.hpp>

#include <stb/stb_image.h>

#include <filesystem>

using namespace sandbox;
using namespace sandbox::hal::render;

namespace
{
    template<typename T>
    struct attribute_converter;

    template<size_t ElementCount, typename ElementType, glm::qualifier ElementQualifier>
    struct attribute_converter<glm::vec<ElementCount, ElementType, ElementQualifier>>
    {
        using VecT = glm::vec<ElementCount, ElementType, glm::qualifier::mediump>;

        vk::Format desired_vk_format{};

        VecT operator()(
            const uint8_t* data,
            gltf::accessor_type actual_accessor_type,
            gltf::component_type actual_component_type,
            gltf::accessor_type /* ignored */,
            gltf::component_type /* ignored */) const
        {
            VecT result{};

            const auto actual_el_count = accessor_components_count(actual_accessor_type);
            if (actual_el_count > ElementCount) {
                throw std::runtime_error("Types are not convertible.");
            }

            for (size_t i = 0; i < ElementCount; ++i) {
                if (i < actual_el_count) {
                    result[i] = convert_element(actual_component_type, i, data);
                } else {
                    // just fill with zeros
                    result[i] = ElementType{0};
                }
            }
            return result;
        }

        ElementType convert_element(gltf::component_type actual_component_type, size_t element_index, const uint8_t* data) const
        {
            // TODO: add pack/unpack elements data relative actual vk_format
            if constexpr (std::is_same_v<ElementType, float>) {
                switch (actual_component_type) {
                    case gltf::component_type::float32:
                        return reinterpret_cast<const float*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, uint32_t>) {
                switch (actual_component_type) {
                    case gltf::component_type::unsigned_int:
                        return reinterpret_cast<const uint32_t*>(data)[element_index];
                    case gltf::component_type::unsigned_short:
                        return uint32_t(reinterpret_cast<const uint16_t*>(data)[element_index]);
                    case gltf::component_type::signed_short:
                        return uint32_t(reinterpret_cast<const int16_t*>(data)[element_index]);
                    case gltf::component_type::signed_byte:
                        return uint32_t(reinterpret_cast<const int8_t*>(data)[element_index]);
                    case gltf::component_type::unsigned_byte:
                        return uint32_t(data[element_index]);
                    default:
                        throw std::runtime_error("Unsupported convert type.");
                }
            } else if constexpr (std::is_same_v<ElementType, int16_t>) {
                switch (actual_component_type) {
                    case gltf::component_type::signed_short:
                        return reinterpret_cast<const int16_t*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, uint16_t>) {
                switch (actual_component_type) {
                    case gltf::component_type::unsigned_short:
                        return reinterpret_cast<const uint16_t*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, int8_t>) {
                switch (actual_component_type) {
                    case gltf::component_type::signed_byte:
                        return reinterpret_cast<const int8_t*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, uint8_t>) {
                switch (actual_component_type) {
                    case gltf::component_type::unsigned_byte:
                        return data[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            }
        }
    };


    template<size_t ElementCount, typename ElementT>
    void copy_attribute_data(
        const gltf::primitive::vertex_attribute& attribute,
        vk::Format desired_vk_format,
        gltf::accessor_type desired_type,
        gltf::component_type desired_component_type,
        uint64_t vtx_size,
        uint64_t offset,
        const uint8_t* dst)
    {
        using T = glm::vec<ElementCount, ElementT>;
        attribute.for_each_element<T>(
            [dst, vtx_size, offset](T v, uint32_t index) {
                if constexpr (ElementCount == 1) {
                    std::memcpy((void*) (dst + vtx_size * index + offset), &v.x, sizeof(v));
                } else {
                    std::memcpy((void*) (dst + vtx_size * index + offset), glm::value_ptr(v), sizeof(v));
                }
            },
            desired_type,
            desired_component_type,
            attribute_converter<T>{desired_vk_format});
    }
} // namespace


//gltf::vk_geometry gltf::vk_geometry::from_gltf_model(
//    const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family)
//{
//    gltf::vk_geometry new_geom{};
//
//    create_meshes_data(
//        mdl,
//        new_geom.m_primitives,
//        new_geom.m_vertex_staging_buffer,
//        new_geom.m_vertex_buffer,
//        new_geom.m_index_staging_buffer,
//        new_geom.m_index_buffer,
//        command_buffer,
//        queue_family);
//
//    return new_geom;
//}


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
        command_buffer,
        queue_family);

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

    std::vector<std::vector<joint_data>> joints_data{};
    joints_data.reserve(mdl.get_skins().size() + 1);

    skins.clear();
    skins.reserve(mdl.get_skins().size() + 1);

    size_t skins_buffer_size = 0;

    for (const auto& skin : mdl.get_skins()) {
        const auto& joints = skin.get_joints();

        auto& new_joints_data = joints_data.emplace_back();
        const auto inv_bind_poses_accessor = accessors[skin.get_inv_bind_matrices()];

        const auto* inv_bind_poses = reinterpret_cast<const glm::mat4*>(
            inv_bind_poses_accessor.get_data(buffers.data(), buffers.size(), buffer_views.data(), buffer_views.size()));

        assert(joints.size() == inv_bind_poses_accessor.get_count());

        new_joints_data.reserve(joints.size());

        for (const auto joint : joints) {
            new_joints_data.emplace_back(joint_data{
                .inv_bind_pose = *inv_bind_poses++,
                .joint = static_cast<uint32_t>(joint)});
        }

        const auto skin_data_size = new_joints_data.size() * sizeof(new_joints_data.front());

        skins.emplace_back(vk_skin{
            .offset = skins_buffer_size,
            .size = skin_data_size,
            .count = joints.size()});

        skins_buffer_size += skin_data_size;
    }

    joints_data.emplace_back();
    joints_data.back().emplace_back();
    skins.emplace_back(vk_skin{
        .offset = skins_buffer_size,
        .size = sizeof(joint_data)});
    skins_buffer_size += sizeof(joint_data);

    auto [staging_buffer, dst_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        skins_buffer_size,
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::PipelineStageFlagBits::eVertexShader,
        vk::AccessFlagBits::eUniformRead,
        [&](const uint8_t* dst) {
            auto curr_joints = joints_data.begin();
            for (const auto& skin : skins) {
                std::memcpy((void*) (dst + skin.offset), curr_joints++->data(), skin.size);
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
        [&](const uint8_t* dst) {
            std::memcpy((void*) dst, default_data.data(), default_data.size() * sizeof(default_data.front()));
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


gltf::vk_geometry_builder& gltf::vk_geometry_builder::set_fixed_vertex_format(
    const std::array<vk::Format, 8>& fmt)
{
    m_fixed_format = fmt;
    return *this;
}


gltf::vk_geometry gltf::vk_geometry_builder::create_with_fixed_format(
    const gltf::model& mdl,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    if (!m_fixed_format) {
        throw std::runtime_error("Fixed vertex format wasn't settled.");
    }

    std::vector<vk::VertexInputAttributeDescription> attributes{};
    std::vector<vk::VertexInputBindingDescription> bindings{};

    attributes.reserve(m_fixed_format->size());
    bindings.reserve(1);

    uint32_t vertex_size = 0;
    uint32_t attribute_location = 0;

    for (const vk::Format vk_fmt : *m_fixed_format) {
        attributes.emplace_back(vk::VertexInputAttributeDescription{
            .location = attribute_location++,
            .binding = 0,
            .format = vk_fmt,
            .offset = vertex_size});

        vertex_size += avk::get_format_info(static_cast<VkFormat>(vk_fmt)).size;
    }

    bindings.emplace_back(vk::VertexInputBindingDescription{
        .binding = 0,
        .stride = vertex_size,
        .inputRate = vk::VertexInputRate::eVertex});

    gltf::vk_geometry new_geometry{};

    const auto& accessors = mdl.get_accessors();
    const auto& buffer_views = mdl.get_buffer_views();
    const auto& buffers = mdl.get_buffers();

    uint32_t vertex_buffer_size = 0;
    uint32_t index_buffer_size = 0;

    std::vector<std::vector<vk_primitive>> meshes{};
    meshes.reserve(mdl.get_meshes().size());

    for (const auto& gltf_mesh : mdl.get_meshes()) {
        auto& new_mesh = meshes.emplace_back();
        new_mesh.reserve(gltf_mesh.get_primitives().size());

        for (const auto& gltf_primitive : gltf_mesh.get_primitives()) {
            auto& new_primitive = new_mesh.emplace_back();
            new_primitive.attributes = attributes;
            new_primitive.bindings = bindings;
            const auto& vertices_accessor = accessors[gltf_primitive.get_attributes().front()];
            new_primitive.vertices_count = vertices_accessor.get_count();
            new_primitive.vertex_buffer_offset = vertex_buffer_size;
            vertex_buffer_size += new_primitive.vertices_count * vertex_size;

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
            auto vk_mesh = meshes.begin();
            for (const auto& gltf_mesh : mdl.get_meshes()) {
                auto vk_primitive = vk_mesh->begin();
                for (const auto& gltf_primitive : gltf_mesh.get_primitives()) {
                    auto primitive_dst_ptr = dst + vk_primitive->vertex_buffer_offset;
                    uint32_t attr_offset = 0;

                    for (uint32_t i = 0; i < m_fixed_format->size(); ++i) {
                        auto curr_path = static_cast<attribute_path>(i);
                        const auto attribute = gltf_primitive.attribute_at_path(mdl, static_cast<attribute_path>(i));
                        copy_attribute_data(attribute, m_fixed_format->at(i), vertex_size, attr_offset, primitive_dst_ptr);
                        attr_offset += avk::get_format_info(static_cast<VkFormat>(m_fixed_format->at(i))).size;
                    }
                }
                vk_mesh++;
            }
        });

    auto [index_staging_buffer, index_buffer] = create_index_buffer(mdl, index_buffer_size, meshes, command_buffer, queue_family);

    new_geometry.m_primitives = std::move(meshes);
    new_geometry.m_vertex_staging_buffer = std::move(vertex_staging_buffer);
    new_geometry.m_vertex_buffer = std::move(vertex_buffer);
    new_geometry.m_index_staging_buffer = std::move(index_staging_buffer);
    new_geometry.m_index_buffer = std::move(index_buffer);

    return new_geometry;
}


gltf::vk_geometry gltf::vk_geometry_builder::create(
    const gltf::model& mdl,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    gltf::vk_geometry new_geometry;

    std::vector<std::vector<vk_primitive>> meshes;

    meshes.clear();
    meshes.reserve(mdl.get_meshes().size());

    const auto& accessors = mdl.get_accessors();
    const auto& buffer_views = mdl.get_buffer_views();
    const auto& buffers = mdl.get_buffers();

    uint32_t vertex_buffer_size = 0;
    uint32_t index_buffer_size = 0;

    for (const auto& gltf_mesh : mdl.get_meshes()) {
        auto& new_mesh = meshes.emplace_back();
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
            auto vk_mesh = meshes.begin();
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

    auto [index_staging_buffer, index_buffer] = create_index_buffer(mdl, index_buffer_size, meshes, command_buffer, queue_family);

    new_geometry.m_primitives = std::move(meshes);
    new_geometry.m_vertex_staging_buffer = std::move(vertex_staging_buffer);
    new_geometry.m_vertex_buffer = std::move(vertex_buffer);
    new_geometry.m_index_staging_buffer = std::move(index_staging_buffer);
    new_geometry.m_index_buffer = std::move(index_buffer);

    return new_geometry;
}


void gltf::vk_geometry_builder::copy_attribute_data(
    const gltf::primitive::vertex_attribute& attribute,
    vk::Format desired_vk_format,
    uint64_t vtx_size,
    uint64_t offset,
    const uint8_t* dst)
{
    auto [desired_type, desired_component_type] = gltf::from_vk_format(desired_vk_format);

    switch (desired_type) {
        case accessor_type::scalar:
            switch (desired_component_type) {
                case component_type::signed_byte:
                    return ::copy_attribute_data<1, int8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_byte:
                    return ::copy_attribute_data<1, uint8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::signed_short:
                    return ::copy_attribute_data<1, int16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_short:
                    return ::copy_attribute_data<1, uint16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_int:
                    return ::copy_attribute_data<1, uint32_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::float32:
                    return ::copy_attribute_data<1, float>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                default:
                    return;
            }
        case accessor_type::vec2:
            switch (desired_component_type) {
                case component_type::signed_byte:
                    return ::copy_attribute_data<2, int8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_byte:
                    return ::copy_attribute_data<2, uint8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::signed_short:
                    return ::copy_attribute_data<2, int16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_short:
                    return ::copy_attribute_data<2, uint16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_int:
                    return ::copy_attribute_data<2, uint32_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::float32:
                    return ::copy_attribute_data<2, float>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                default:
                    return;
            }
        case accessor_type::vec3:
            switch (desired_component_type) {
                case component_type::signed_byte:
                    return ::copy_attribute_data<3, int8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_byte:
                    return ::copy_attribute_data<3, uint8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::signed_short:
                    return ::copy_attribute_data<3, int16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_short:
                    return ::copy_attribute_data<3, uint16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_int:
                    return ::copy_attribute_data<3, uint32_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::float32:
                    return ::copy_attribute_data<3, float>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                default:
                    return;
            }
        case accessor_type::vec4:
            switch (desired_component_type) {
                case component_type::signed_byte:
                    return ::copy_attribute_data<4, int8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_byte:
                    return ::copy_attribute_data<4, uint8_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::signed_short:
                    return ::copy_attribute_data<4, int16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_short:
                    return ::copy_attribute_data<4, uint16_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::unsigned_int:
                    return ::copy_attribute_data<4, uint32_t>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                case component_type::float32:
                    return ::copy_attribute_data<4, float>(attribute, desired_vk_format, desired_type, desired_component_type, vtx_size, offset, dst);
                default:
                    return;
            }
        default:
            return;
    }
}



std::pair<hal::render::avk::vma_buffer, hal::render::avk::vma_buffer> gltf::vk_geometry_builder::create_index_buffer(
    const gltf::model& mdl,
    size_t index_buffer_size,
    const std::vector<std::vector<vk_primitive>>& meshes,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    const auto& accessors = mdl.get_accessors();
    const auto& buffer_views = mdl.get_buffer_views();
    const auto& buffers = mdl.get_buffers();

    return avk::gen_buffer(
        command_buffer,
        queue_family,
        index_buffer_size,
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::AccessFlagBits::eIndexRead,
        [&](const uint8_t* dst) {
            auto vk_mesh = meshes.begin();
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
}
