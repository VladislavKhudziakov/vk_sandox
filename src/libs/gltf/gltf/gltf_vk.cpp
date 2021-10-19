#include "gltf_vk.hpp"

#include <gltf/vk_utils.hpp>
#include <render/vk/utils.hpp>
#include <utils/conditions_helpers.hpp>

#include <stb/stb_image.h>

#include <filesystem>
#include <numeric>

using namespace sandbox;
using namespace sandbox::hal::render;

using namespace gltf;

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
        uint8_t* dst)
    {
        using T = glm::vec<ElementCount, ElementT>;
        attribute.for_each_element<T>(
            [dst, vtx_size, offset](T v, uint32_t index) {
                if constexpr (ElementCount == 1) {
                    std::memcpy((void*) (dst + vtx_size * index + offset), &v.x, sizeof(v));
                } else {
                    auto ptr = dst + vtx_size * index + offset;
                    auto last = ptr + sizeof(v);
                    auto vptr = reinterpret_cast<const uint8_t*>(glm::value_ptr(v));
                    std::memcpy((void*) (dst + vtx_size * index + offset), glm::value_ptr(v), sizeof(v));
                }
            },
            desired_type,
            desired_component_type,
            attribute_converter<T>{desired_vk_format});
    }
} // namespace


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


gltf::vk_model_builder& gltf::vk_model_builder::set_vertex_format(
    const std::array<vk::Format, 8>& fmt)
{
    m_fixed_format = fmt;
    return *this;
}


vk_model_builder& sandbox::gltf::vk_model_builder::use_skin(bool use_skin)
{
    m_skinned = use_skin;
    return *this;
}


gltf::vk_model sandbox::gltf::vk_model_builder::create(const gltf::model& mdl, avk::buffer_pool& pool)
{
    gltf::vk_model result;
    create_geometry(mdl, result, pool);

    if (m_skinned) {
        create_skins(mdl, result, pool);
    }

    return result;
}


void sandbox::gltf::vk_model_builder::create_geometry(const gltf::model& mdl, vk_model& result, hal::render::avk::buffer_pool& pool)
{
    uint32_t vertex_size = 0;

    get_vertex_attributes_data_from_fixed_format(result.m_attributes, result.m_bindings, vertex_size);

    result.m_meshes.reserve(mdl.get_meshes().size());

    for (const auto& mesh : mdl.get_meshes()) {
        auto& new_mesh = result.m_meshes.emplace_back();
        new_mesh.m_primitives.reserve(mesh.get_primitives().size());

        for (const auto& primitive : mesh.get_primitives()) {
            auto& new_primitive = new_mesh.m_primitives.emplace_back();
            auto vertex_buffer_bulder = pool.get_builder();
            vertex_buffer_bulder.set_size(primitive.get_vertices_count(mdl) * vertex_size);
            vertex_buffer_bulder.set_usage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

            new_primitive.m_vertex_buffer = vertex_buffer_bulder.create([this, &mdl, &primitive, vertex_size](uint8_t* dst) {
                uint32_t attr_offset = 0;

                for (uint32_t i = 0; i < m_fixed_format->size(); ++i) {
                    auto curr_path = static_cast<attribute_path>(i);
                    const auto attribute = primitive.attribute_at_path(mdl, static_cast<attribute_path>(i));
                    copy_attribute_data(attribute, m_fixed_format->at(i), vertex_size, attr_offset, dst);
                    attr_offset += avk::get_format_info(static_cast<VkFormat>(m_fixed_format->at(i))).size;
                }
            });

            new_primitive.m_vertices_count = primitive.get_vertices_count(mdl);

            if (primitive.get_indices_count(mdl) > 0) {
                auto index_buffer_bulder = pool.get_builder();
                primitive.get_indices_data(mdl);

                auto [index_data, indices_type] = primitive.get_indices_data(mdl);
                auto elements_count = primitive.get_indices_count(mdl);

                // clang-format off
                auto element_size = avk::get_format_info(static_cast<VkFormat>(
                    to_vk_format(gltf::accessor_type::scalar, indices_type))).size;
                // clang-format on

                index_buffer_bulder.set_size(elements_count * element_size);
                index_buffer_bulder.set_usage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

                new_primitive.m_index_buffer = index_buffer_bulder.create([index_data, elements_count, element_size](uint8_t* dst) {
                    std::memcpy(dst, index_data, elements_count * element_size);
                });

                new_primitive.m_indices_count = elements_count;
                new_primitive.m_index_type = to_vk_index_type(accessor_type::scalar, indices_type);
            }
        }
    }
}


void sandbox::gltf::vk_model_builder::create_skins(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool)
{
    std::vector<vk_skin_> skins;
    create_skins_resources(mdl, skins, model, pool);
    assing_skins_to_meshes(skins, mdl, model);
}


void sandbox::gltf::vk_model_builder::create_skins_resources(const gltf::model& mdl, std::vector<vk_skin_>& result, vk_model& model, hal::render::avk::buffer_pool& pool)
{
    struct joint_data
    {
        glm::mat4 inv_bind_pose{1};
        alignas(sizeof(float) * 4) uint32_t joint{uint32_t(-1)};
    };

    const auto& accessors = mdl.get_accessors();

    std::vector<std::vector<joint_data>> joints_data{};
    joints_data.reserve(mdl.get_skins().size() + 1);

    result.clear();
    result.reserve(mdl.get_skins().size());

    for (const auto& skin : mdl.get_skins()) {
        auto& new_skin = result.emplace_back();
        const auto& joints = skin.get_joints();

        std::vector<joint_data> vk_joints{};
        const auto ibp = accessors[skin.get_inv_bind_matrices()];
        const glm::mat4* ibp_data = reinterpret_cast<const glm::mat4*>(ibp.get_data(mdl));

        ASSERT(joints.size() == ibp.get_count());

        vk_joints.reserve(joints.size());

        for (const auto joint : joints) {
            vk_joints.emplace_back(joint_data{
                .inv_bind_pose = *ibp_data++,
                .joint = static_cast<uint32_t>(joint)});
        }

        auto joints_buffer_builder = pool.get_builder();
        joints_buffer_builder.set_size(vk_joints.size() * sizeof(joint_data));
        joints_buffer_builder.set_usage(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst);

        new_skin.m_joints_count = vk_joints.size();
        new_skin.m_hierarchy_size = mdl.get_nodes().size();

        new_skin.m_joints_buffer = joints_buffer_builder.create([joints = std::move(vk_joints)](uint8_t* dst) {
            std::memcpy(dst, joints.data(), joints.size() * sizeof(joint_data));
        });
    }
}


void sandbox::gltf::vk_model_builder::assing_skins_to_meshes(const std::vector<vk_skin_>& skins, const gltf::model& mdl, vk_model& model)
{
    auto& meshes = model.m_meshes;

    for_each_scene_node(mdl, [this, &meshes, &skins](const gltf::node& node, uint32_t node_index) {
        if (node.get_skin() >= 0) {
            meshes[node.get_mesh()].m_skin = skins[node.get_skin()];
            meshes[node.get_mesh()].m_skinned = true;
        }  
    });
}


void sandbox::gltf::vk_model_builder::get_vertex_attributes_data_from_fixed_format(
    std::vector<vk::VertexInputAttributeDescription>& out_attributres,
    std::vector<vk::VertexInputBindingDescription>& out_bindings,
    uint32_t& out_vertex_size)
{
    CHECK_MSG(m_fixed_format, "Fixed vertex format didn't specified.");

    out_attributres.clear();
    out_bindings.clear();

    out_attributres.reserve(m_fixed_format->size());
    out_bindings.reserve(1);

    out_vertex_size = 0;
    uint32_t attribute_location = 0;

    for (const vk::Format vk_fmt : *m_fixed_format) {
        out_attributres.emplace_back(vk::VertexInputAttributeDescription{
            .location = attribute_location++,
            .binding = 0,
            .format = vk_fmt,
            .offset = out_vertex_size});

        out_vertex_size += avk::get_format_info(static_cast<VkFormat>(vk_fmt)).size;
    }

    out_bindings.emplace_back(vk::VertexInputBindingDescription{
        .binding = 0,
        .stride = out_vertex_size,
        .inputRate = vk::VertexInputRate::eVertex});
}


void gltf::vk_model_builder::copy_attribute_data(
    const gltf::primitive::vertex_attribute& attribute,
    vk::Format desired_vk_format,
    uint64_t vtx_size,
    uint64_t offset,
    uint8_t* dst)
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


gltf::vk_animations_list gltf::animations_builder::create(
    const gltf::model& mdl,
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family)
{
    vk_animations_list result{};

    size_t anims_buffer_size = 0;
    const auto& animations = get_animations(mdl, anims_buffer_size);
    const auto& nodes_children = get_input_nodes(mdl);
    const auto& exec_order = get_execution_order(mdl);

    result.m_animations_list.reserve(animations.size());

    for (const auto& anim : animations) {
        result.m_animations_list.emplace_back(vk_animation{static_cast<uint32_t>(anim.offset), static_cast<uint32_t>(anim.size)});
    }

    auto [anim_meta_staging_buffer, anim_meta_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        animations.size() * sizeof(glm::ivec4),
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eShaderRead,
        [&animations](uint8_t* dst) {
            uint64_t offset = 0;
            for (const auto& anim : animations) {
                std::memcpy(dst + offset, glm::value_ptr(anim.interpolation_avg_frame_duration), sizeof(glm::ivec4));
                offset += sizeof(glm::ivec4);
            }
        });

    result.m_anim_meta_staging_buffer = std::move(anim_meta_staging_buffer);
    result.m_anim_meta_buffer = std::move(anim_meta_buffer);
    result.m_anim_meta_sub_buffer.first = 0;
    result.m_anim_meta_sub_buffer.second = animations.size() * sizeof(glm::ivec4);

    auto [anim_staging_buffer, anim_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        anims_buffer_size,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eShaderRead,
        [&animations](uint8_t* dst) {
            for (const auto& anim : animations) {
                auto* dst_ptr = dst + anim.offset;
                for (const auto& sampler : anim.samplers) {
                    std::memcpy(dst_ptr, &sampler.ts, sizeof(float));
                    dst_ptr += sizeof(float) * 4;
                    std::memcpy(dst_ptr, sampler.nodes_keys.data(), sampler.nodes_keys.size() * sizeof(sampler.nodes_keys.front()));
                    dst_ptr += sampler.nodes_keys.size() * sizeof(sampler.nodes_keys.front());
                }
            }
        });

    result.m_anim_sub_buffer.first = 0;
    result.m_anim_sub_buffer.second = anims_buffer_size;

    auto [nodes_staging_buffer, nodes_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        nodes_children.size() * sizeof(nodes_children.front()),
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eShaderRead,
        [&nodes_children](uint8_t* dst) {
            std::memcpy(dst, nodes_children.data(), nodes_children.size() * sizeof(nodes_children.front()));
        });

    result.m_nodes_sub_buffer.first = 0;
    result.m_nodes_sub_buffer.second = nodes_children.size() * sizeof(nodes_children.front());

    auto [exec_order_staging_buffer, exec_order_buffer] = avk::gen_buffer(
        command_buffer,
        queue_family,
        exec_order.size() * sizeof(exec_order.front()),
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eShaderRead,
        [&exec_order](uint8_t* dst) {
            std::memcpy(dst, exec_order.data(), exec_order.size() * sizeof(exec_order.front()));
        });

    result.m_exec_order_sub_buffer.first = 0;
    result.m_exec_order_sub_buffer.second = exec_order.size() * sizeof(exec_order.front());

    result.m_anim_staging_buffer = std::move(anim_staging_buffer);
    result.m_anim_buffer = std::move(anim_buffer);
    result.m_nodes_staging_buffer = std::move(nodes_staging_buffer);
    result.m_nodes_buffer = std::move(nodes_buffer);
    result.m_exec_order_staging_buffer = std::move(exec_order_staging_buffer);
    result.m_exec_order_buffer = std::move(exec_order_buffer);

    return result;
}


std::vector<animations_builder::gpu_trs> animations_builder::get_default_keys(const model& mdl)
{
    std::vector<gpu_trs> result{};
    result.reserve(mdl.get_nodes().size());

    for (const auto& node : mdl.get_nodes()) {
        auto& new_key = result.emplace_back();

        const auto node_local_transform = node.get_transform();
        auto& l_rot = node_local_transform.rotation;
        new_key.translation = glm::vec4(node_local_transform.translation, 0.);
        new_key.rotation = {l_rot.x, l_rot.y, l_rot.z, l_rot.w};
        new_key.scale = glm::vec4(node_local_transform.scale, 1.);
    }

    return result;
}

#include <glm/gtx/string_cast.hpp>
animations_builder::gpu_animation animations_builder::create_animation(
    const model& model,
    const animation& curr_animation,
    const std::vector<gpu_trs>& default_keys)
{
    animations_builder::gpu_animation result{};
    const auto& samplers = curr_animation.get_samplers();

    for (const auto& channel : curr_animation.get_channels()) {
        auto [keys_count, keys] = channel.get_keys(model);

        if (keys_count > result.samplers.size()) {
            result.samplers.reserve(keys_count);
            for (auto i = result.samplers.size(); i < keys_count; ++i) {
                auto& new_sampler = result.samplers.emplace_back();
                new_sampler.ts = keys[i];
                new_sampler.nodes_keys.reserve(default_keys.size());
                std::copy(default_keys.begin(), default_keys.end(), std::back_inserter(new_sampler.nodes_keys));
            }
        }
    }

    float frame_duration = -1;

    for (size_t sampler = 0; sampler < result.samplers.size(); ++sampler) {
        auto& curr_sampler = result.samplers[sampler];
        auto& next_sampler = result.samplers[std::min(result.samplers.size() - 1, sampler + 1)];

        if (sampler + 1 < result.samplers.size()) {
            float curr_frame_duration = next_sampler.ts - curr_sampler.ts;
            if (frame_duration >= 0) {
                auto delta = curr_frame_duration - frame_duration;
                CHECK_MSG(delta < 2.0f / 1e6, " delta " + std::to_string(delta));
            }
            frame_duration = curr_frame_duration;
        }

        for (size_t node = 0; node < curr_sampler.nodes_keys.size(); ++node) {
            const auto channels = curr_animation.channels_for_node(node);

            // take values from prev frame
            const auto& prev_node = sampler == 0 ? default_keys[node] : result.samplers[sampler - 1].nodes_keys[node];
            curr_sampler.nodes_keys[node] = prev_node;

            for (const int32_t channel : channels) {
                if (channel < 0) {
                    continue;
                }

                auto& curr_channel = curr_animation.get_channels()[channel];

                auto [values_count, values_a_type, values_type, values] = curr_channel.get_values(model);
                const auto& curr_gltf_sampler = curr_animation.get_samplers()[curr_channel.get_sampler()];

                if (sampler < values_count) {
                    switch (curr_channel.get_path()) {
                        case animation_path::rotation:
                            CHECK(result.interpolation_avg_frame_duration.y < 0 || result.interpolation_avg_frame_duration.y == int32_t(curr_gltf_sampler.get_interpolation()));
                            result.interpolation_avg_frame_duration.y = int32_t(curr_gltf_sampler.get_interpolation());
                            // TODO: vec4 -> quat conversion
                            curr_sampler.nodes_keys[node].rotation = reinterpret_cast<const glm::vec4*>(values)[sampler];
                            break;
                        case animation_path::translation:
                            CHECK(result.interpolation_avg_frame_duration.x < 0 || result.interpolation_avg_frame_duration.x == int32_t(curr_gltf_sampler.get_interpolation()));
                            result.interpolation_avg_frame_duration.x = int32_t(curr_gltf_sampler.get_interpolation());
                            curr_sampler.nodes_keys[node].translation = glm::vec4(reinterpret_cast<const glm::vec3*>(values)[sampler], 0.);
                            break;
                        case animation_path::scale:
                            CHECK(result.interpolation_avg_frame_duration.z < 0 || result.interpolation_avg_frame_duration.z == int32_t(curr_gltf_sampler.get_interpolation()));
                            result.interpolation_avg_frame_duration.z = int32_t(curr_gltf_sampler.get_interpolation());
                            curr_sampler.nodes_keys[node].scale = glm::vec4(reinterpret_cast<const glm::vec3*>(values)[sampler], 1.);
                            break;
                        case animation_path::weights:
                            continue;
                    }
                }
            }
        }
    }

    result.interpolation_avg_frame_duration.w = int32_t(frame_duration * 1e6);

    return result;
}


std::vector<animations_builder::gpu_animation> gltf::animations_builder::get_animations(const gltf::model& mdl, size_t& out_anims_buffer_size)
{
    std::vector<animations_builder::gpu_animation> result;
    result.reserve(mdl.get_animations().size());

    auto default_keys = get_default_keys(mdl);

    size_t anims_buffer_size = 0;

    for (const auto& curr_animation : mdl.get_animations()) {
        auto new_anim = create_animation(mdl, curr_animation, default_keys);
        new_anim.offset = anims_buffer_size;
        // samplers count * (size aligned ts size + anim key size * nodes count)
        new_anim.size = new_anim.samplers.size() * ((sizeof(float) * 4) + sizeof(gpu_trs) * mdl.get_nodes().size());
        anims_buffer_size += new_anim.size;
        result.emplace_back(std::move(new_anim));
    }

    out_anims_buffer_size = anims_buffer_size;

    return result;
}


std::vector<glm::ivec4> gltf::animations_builder::get_input_nodes(const gltf::model& mdl)
{
    std::vector<glm::ivec4> children_indices{};
    children_indices.emplace_back(-1, -1, -1, -1);
    size_t children_write_index = 0;

    std::vector<glm::ivec4> result;
    result.reserve(mdl.get_nodes().size());

    for (const auto& node : mdl.get_nodes()) {
        result.emplace_back(
            children_indices.size() - 1,
            children_write_index,
            node.get_children().size(),
            -1);

        for (const auto& child : node.get_children()) {
            if (children_write_index >= children_indices.back().length()) {
                children_write_index = 0;
                result.emplace_back(-1, -1, -1, -1);
            }

            children_indices.back()[children_write_index++] = child;
        }
    }

    result.reserve(result.size() + children_indices.size());
    std::copy(children_indices.begin(), children_indices.end(), std::back_inserter(result));

    return result;
}


std::vector<glm::ivec4> gltf::animations_builder::get_execution_order(const gltf::model& mdl)
{
    std::vector<glm::ivec4> result;
    result.reserve((mdl.get_nodes().size() / 4) + 1);
    result.emplace_back(-1, -1, -1, -1);
    size_t write_index = 0;

    for_each_scene_node(mdl, [&write_index, &result](const gltf::node& node, int32_t node_index) {
        if (write_index >= result.back().length()) {
            write_index = 0;
            result.emplace_back(-1, -1, -1, -1);
        }
        result.back()[write_index++] = node_index;
    });

    return result;
}


const std::vector<vk_animation>& vk_animations_list::get_animations() const
{
    return m_animations_list;
}


std::tuple<vk::Buffer, uint32_t, uint32_t> vk_animations_list::get_anim_buffer() const
{
    return {m_anim_buffer.as<vk::Buffer>(), m_anim_sub_buffer.first, m_anim_sub_buffer.second};
}

std::tuple<vk::Buffer, uint32_t, uint32_t> sandbox::gltf::vk_animations_list::get_anim_meta_buffer() const
{
    return {m_anim_meta_buffer.as<vk::Buffer>(), m_anim_meta_sub_buffer.first, m_anim_meta_sub_buffer.second};
}


std::tuple<vk::Buffer, uint32_t, uint32_t> vk_animations_list::get_nodes_buffer() const
{
    return {m_nodes_buffer.as<vk::Buffer>(), m_nodes_sub_buffer.first, m_nodes_sub_buffer.second};
}


std::tuple<vk::Buffer, uint32_t, uint32_t> vk_animations_list::get_exec_order_buffer() const
{
    return {m_exec_order_buffer.as<vk::Buffer>(), m_exec_order_sub_buffer.first, m_exec_order_sub_buffer.second};
}


uint32_t vk_animations_list::get_nodes_count() const
{
    return 0;
}


void animation_instance::update(uint64_t dt)
{
    if (m_playback_state == playback_state::paused) {
        return;
    }

    const auto& curr_anim = m_model.get_animations()[m_current_animation];
    m_curr_position += dt;
    if (m_curr_position >= m_end_position) {
        if (m_looped) {
            m_curr_position = m_start_position;
        } else {
            m_curr_position = m_end_position;
        }
    }
}


animation_instance::animation_instance(const model& model)
    : m_model(model)
{
}


void animation_instance::play(uint32_t animation_index, uint64_t start_position_us, uint64_t end_position_us, bool looped)
{
    const auto& curr_anim = m_model.get_animations()[m_current_animation];
    uint64_t anim_duration_us = curr_anim.get_duration() * 1e6;

    m_current_animation = animation_index;
    m_start_position = std::max(start_position_us, anim_duration_us);
    m_curr_position = m_start_position;
    m_end_position = std::max(anim_duration_us, end_position_us);
    m_looped = looped;

    m_playback_state = playback_state::playing;
}


void animation_instance::pause()
{
    m_playback_state = playback_state::paused;
}


void animation_instance::stop()
{
    m_playback_state = playback_state::paused;
}


void animation_instance::resume()
{
    m_playback_state = playback_state::playing;
}


gpu_animation_controller::gpu_animation_controller(const gltf::model& model, size_t batch_size)
    : m_animations_list()
    , m_batch_size(batch_size)
{
    uint32_t queue_family{0};

    m_anim_instances.reserve(m_batch_size);
    m_hierarchy_buffer_size = m_animations_list.get_nodes_count() * batch_size * sizeof(glm::mat4);

    auto result_buffer = avk::create_vma_buffer(
        vk::BufferCreateInfo{
            .flags = {},
            .size = m_hierarchy_buffer_size,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family},
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY});
}


void gpu_animation_controller::update(uint64_t dt)
{
    for (auto& i : m_anim_instances) {
        i.update(dt);
    }
}


std::tuple<vk::Buffer, uint32_t, uint32_t> gpu_animation_controller::get_hierarchy_buffer()
{
    return {m_hierarchy_buffer.as<vk::Buffer>(), 0, m_hierarchy_buffer_size};
}


animation_instance* gpu_animation_controller::create_animation_instance(const gltf::model& model)
{
    CHECK(m_anim_instances.size() < m_batch_size);
    return &m_anim_instances.emplace_back(model);
}


const std::vector<vk_mesh>& sandbox::gltf::vk_model::get_meshes() const
{
    return m_meshes;
}


vk_model::vertex_format sandbox::gltf::vk_model::get_vertex_format()
{
    return {m_attributes.data(), uint32_t(m_attributes.size()), m_bindings.data(), uint32_t(m_bindings.size())};
}


const std::vector<vk_primitive>& sandbox::gltf::vk_mesh::get_primitives() const
{
    return m_primitives;
}


const vk_skin_& sandbox::gltf::vk_mesh::get_skin() const
{
    return m_skin;
}


bool sandbox::gltf::vk_mesh::is_skinned() const
{
    return m_skinned;
}


const avk::buffer_instance& sandbox::gltf::vk_primitive::get_vertex_buffer() const
{
    return m_vertex_buffer;
}


uint32_t sandbox::gltf::vk_primitive::get_vertices_count() const
{
    return m_vertices_count;
}


const avk::buffer_instance& sandbox::gltf::vk_primitive::get_index_buffer() const
{
    return m_index_buffer;
}


vk::IndexType sandbox::gltf::vk_primitive::get_indices_type() const
{
    return m_index_type;
}


uint32_t sandbox::gltf::vk_primitive::get_indices_count() const
{
    return m_indices_count;
}


const hal::render::avk::buffer_instance& sandbox::gltf::vk_skin_::get_joints_buffer() const
{
    return m_joints_buffer;
}


uint32_t sandbox::gltf::vk_skin_::get_joints_count() const
{
    return m_joints_count;
}


uint32_t sandbox::gltf::vk_skin_::get_hierarchy_size() const
{
    return m_hierarchy_size;
}
