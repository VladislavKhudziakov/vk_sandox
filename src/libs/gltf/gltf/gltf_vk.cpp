#include "gltf_vk.hpp"

#include <gltf/vk_utils.hpp>
#include <render/vk/utils.hpp>
#include <utils/conditions_helpers.hpp>

#include <stb/stb_image.h>

#include <filesystem>
#include <numeric>

using namespace sandbox;
using namespace sandbox::gltf;
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
            accessor_type actual_accessor_type,
            component_type actual_component_type,
            accessor_type /* ignored */,
            component_type /* ignored */) const
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

        ElementType convert_element(component_type actual_component_type, size_t element_index, const uint8_t* data) const
        {
            // TODO: add pack/unpack elements data relative actual vk_format
            if constexpr (std::is_same_v<ElementType, float>) {
                switch (actual_component_type) {
                    case component_type::float32:
                        return reinterpret_cast<const float*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, uint32_t>) {
                switch (actual_component_type) {
                    case component_type::unsigned_int:
                        return reinterpret_cast<const uint32_t*>(data)[element_index];
                    case component_type::unsigned_short:
                        return uint32_t(reinterpret_cast<const uint16_t*>(data)[element_index]);
                    case component_type::signed_short:
                        return uint32_t(reinterpret_cast<const int16_t*>(data)[element_index]);
                    case component_type::signed_byte:
                        return uint32_t(reinterpret_cast<const int8_t*>(data)[element_index]);
                    case component_type::unsigned_byte:
                        return uint32_t(data[element_index]);
                    default:
                        throw std::runtime_error("Unsupported convert type.");
                }
            } else if constexpr (std::is_same_v<ElementType, int16_t>) {
                switch (actual_component_type) {
                    case component_type::signed_short:
                        return reinterpret_cast<const int16_t*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, uint16_t>) {
                switch (actual_component_type) {
                    case component_type::unsigned_short:
                        return reinterpret_cast<const uint16_t*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, int8_t>) {
                switch (actual_component_type) {
                    case component_type::signed_byte:
                        return reinterpret_cast<const int8_t*>(data)[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            } else if constexpr (std::is_same_v<ElementType, uint8_t>) {
                switch (actual_component_type) {
                    case component_type::unsigned_byte:
                        return data[element_index];
                    default:
                        throw std::runtime_error("Elements converting temporary unsupported.");
                }
            }
        }
    };


    template<size_t ElementCount, typename ElementT>
    void copy_attribute_data(
        const primitive::vertex_attribute& attribute,
        vk::Format desired_vk_format,
        accessor_type desired_type,
        component_type desired_component_type,
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


vk_texture_atlas vk_texture_atlas::from_gltf_model(
    const model& mdl,
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

        vk::Format format = stb_channels_count_to_vk_format(pix_data.c);
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
            to_vk_sampler_filter(gltf_sampler.get_mag_filter()).first,
            to_vk_sampler_filter(gltf_sampler.get_min_filter()).first,
            to_vk_sampler_filter(gltf_sampler.get_min_filter()).second,
            to_vk_sampler_wrap(gltf_sampler.get_wrap_s()),
            to_vk_sampler_wrap(gltf_sampler.get_wrap_t()),
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


const vk_texture& vk_texture_atlas::get_texture(uint32_t index) const
{
    return m_vk_textures[index];
}


vk_model_builder& vk_model_builder::set_vertex_format(
    const std::array<vk::Format, 8>& fmt)
{
    m_fixed_format = fmt;
    return *this;
}


vk_model_builder& vk_model_builder::use_skin(bool use_skin)
{
    m_skinned = use_skin;
    return *this;
}


vk_model vk_model_builder::create(const model& mdl, avk::buffer_pool& pool)
{
    vk_model result;
    create_geometry(mdl, result, pool);

    if (m_skinned && !mdl.get_skins().empty()) {
        create_skins(mdl, result, pool);
    }

    if (/* TODO: m_animated && */ !mdl.get_animations().empty()) {
        create_animations(mdl, result, pool);
    }

    return result;
}


void vk_model_builder::create_geometry(const model& mdl, vk_model& result, hal::render::avk::buffer_pool& pool)
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
                    to_vk_format(accessor_type::scalar, indices_type))).size;
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


void vk_model_builder::create_skins(const model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool)
{
    std::vector<vk_skin> skins;
    create_skins_resources(mdl, skins, model, pool);
    assing_skins_to_meshes(skins, mdl, model);
}


void vk_model_builder::create_skins_resources(const model& mdl, std::vector<vk_skin>& result, vk_model& model, hal::render::avk::buffer_pool& pool)
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


void vk_model_builder::assing_skins_to_meshes(const std::vector<vk_skin>& skins, const model& mdl, vk_model& model)
{
    auto& meshes = model.m_meshes;

    for_each_scene_node(mdl, [this, &meshes, &skins](const node& node, uint32_t node_index) {
        if (node.get_skin() >= 0) {
            meshes[node.get_mesh()].m_skin = skins[node.get_skin()];
            meshes[node.get_mesh()].m_skinned = true;
        }
    });
}


void sandbox::gltf::vk_model_builder::create_animations(const gltf::model& mdl, vk_model& result, hal::render::avk::buffer_pool& pool)
{
    create_anim_keys_buffers(mdl, result, pool);
    create_anim_nodes_buffer(mdl, result, pool);
    create_anim_exec_order_buffer(mdl, result, pool);
}


std::vector<vk_model_builder::gpu_trs> vk_model_builder::get_default_keys(const gltf::model& mdl)
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


vk_model_builder::gpu_animation vk_model_builder::create_gpu_animation(
    const model& mdl,
    const animation& curr_animation,
    const std::vector<gpu_trs>& default_keys)
{
    gpu_animation result{};

    const auto& samplers = curr_animation.get_samplers();

    for (const auto& channel : curr_animation.get_channels()) {
        auto [keys_count, keys] = channel.get_keys(mdl);

        if (keys_count > result.time_stamps.size()) {
            result.time_stamps.reserve(keys_count);
            auto sz = keys_count - result.time_stamps.size();
            result.keys.reserve(sz * default_keys.size());

            for (auto i = result.time_stamps.size(); i < keys_count; ++i) {
                result.time_stamps.emplace_back(glm::vec4{keys[i], 0, 0, 0});
                std::copy(default_keys.begin(), default_keys.end(), std::back_inserter(result.keys));
            }
        }
    }

    float frame_duration = -1;

    for (size_t sampler = 0; sampler < result.time_stamps.size(); ++sampler) {
        auto curr_ts = result.time_stamps[sampler];
        auto next_ts = result.time_stamps[std::min(result.time_stamps.size() - 1, sampler + 1)];

        if (sampler + 1 < result.time_stamps.size()) {
            float curr_frame_duration = next_ts.x - curr_ts.x;
            assert(curr_frame_duration != 0);
            if (frame_duration >= 0) {
                auto delta = curr_frame_duration - frame_duration;
                CHECK_MSG(delta < 2.0f / 1e6, " delta " + std::to_string(delta));
            }
            frame_duration = curr_frame_duration;
        }

        auto keys_size = default_keys.size();

        for (size_t node = 0; node < keys_size; ++node) {
            const auto channels = curr_animation.channels_for_node(node);
            
            // take values from prev frame
            auto curr_sampler_index = sampler * keys_size + node;
            auto prev_sampler_index = (sampler - 1) * keys_size + node;
            const auto& prev_node = sampler == 0 ? default_keys[node] : result.keys[(sampler - 1) * keys_size + node];
            auto& curr_key = result.keys[curr_sampler_index];
            curr_key = prev_node;
            

            for (const int32_t channel : channels) {
                if (channel < 0) {
                    continue;
                }

                auto& curr_channel = curr_animation.get_channels()[channel];

                auto [values_count, values_a_type, values_type, values] = curr_channel.get_values(mdl);
                const auto& curr_gltf_sampler = curr_animation.get_samplers()[curr_channel.get_sampler()];

                if (sampler < values_count) {
                    switch (curr_channel.get_path()) {
                        case animation_path::rotation:
                            CHECK(result.interpolation_avg_frame_duration.y < 0 || result.interpolation_avg_frame_duration.y == int32_t(curr_gltf_sampler.get_interpolation()));
                            result.interpolation_avg_frame_duration.y = int32_t(curr_gltf_sampler.get_interpolation());
                            curr_key.rotation = reinterpret_cast<const glm::vec4*>(values)[sampler];
                            break;
                        case animation_path::translation:
                            CHECK(result.interpolation_avg_frame_duration.x < 0 || result.interpolation_avg_frame_duration.x == int32_t(curr_gltf_sampler.get_interpolation()));
                            result.interpolation_avg_frame_duration.x = int32_t(curr_gltf_sampler.get_interpolation());
                            curr_key.translation = glm::vec4(reinterpret_cast<const glm::vec3*>(values)[sampler], 0.);
                            break;
                        case animation_path::scale:
                            CHECK(result.interpolation_avg_frame_duration.z < 0 || result.interpolation_avg_frame_duration.z == int32_t(curr_gltf_sampler.get_interpolation()));
                            result.interpolation_avg_frame_duration.z = int32_t(curr_gltf_sampler.get_interpolation());
                            curr_key.scale = glm::vec4(reinterpret_cast<const glm::vec3*>(values)[sampler], 1.);
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


void vk_model_builder::create_anim_keys_buffers(const model& mdl, vk_model& result, hal::render::avk::buffer_pool& pool)
{
    auto default_keys = get_default_keys(mdl);
    result.m_animations.reserve(mdl.get_animations().size());

    for (const auto& anim : mdl.get_animations()) {
        auto& new_anim = result.m_animations.emplace_back();
        auto gpu_anim = create_gpu_animation(mdl, anim, default_keys);

        // clang-format off 
        new_anim.m_time_stamps_buffer = pool.get_builder()
          .set_size(gpu_anim.time_stamps.size() * sizeof(gpu_anim.time_stamps.front()))
          .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
          .create([time_stamps = std::move(gpu_anim.time_stamps)](uint8_t* dst) {
              std::memcpy(dst, time_stamps.data(), time_stamps.size() * sizeof(time_stamps.front()));
          });

        new_anim.m_keys_buffer = pool.get_builder()
          .set_size(gpu_anim.keys.size() * sizeof(gpu_anim.keys.front()))
          .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
          .create([keys = std::move(gpu_anim.keys)](uint8_t* dst) {
              std::memcpy(dst, keys.data(), keys.size() * sizeof(keys.front()));
          });

        new_anim.m_meta_buffer = pool.get_builder()
          .set_size(sizeof(gpu_anim.interpolation_avg_frame_duration))
          .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
          .create([meta = gpu_anim.interpolation_avg_frame_duration](uint8_t* dst) {
              std::memcpy(dst, &meta, sizeof(meta));
          });
        // clang-format on
    }
}


void sandbox::gltf::vk_model_builder::create_anim_nodes_buffer(const gltf::model& mdl, vk_model& result, hal::render::avk::buffer_pool& pool)
{
    std::vector<glm::ivec4> children_indices{};
    children_indices.emplace_back(-1, -1, -1, -1);
    size_t children_write_index = 0;

    std::vector<glm::ivec4> input_nodes_data;
    input_nodes_data.reserve(mdl.get_nodes().size());

    for (const auto& node : mdl.get_nodes()) {
        if (children_write_index >= children_indices.back().length()) {
            input_nodes_data.emplace_back(
                children_indices.size(),
                0,
                node.get_children().size(),
                -1);
        } else {
            input_nodes_data.emplace_back(
                children_indices.size() - 1,
                std::min<int32_t>(children_write_index, children_indices.back().length() - 1),
                node.get_children().size(),
                -1);
        }

        for (const auto& child : node.get_children()) {
            if (children_write_index >= children_indices.back().length()) {
                children_write_index = 0;
                children_indices.emplace_back(-1, -1, -1, -1);
            }

            children_indices.back()[children_write_index++] = child;
        }
    }

    input_nodes_data.reserve(input_nodes_data.size() + children_indices.size());
    std::copy(children_indices.begin(), children_indices.end(), std::back_inserter(input_nodes_data));

    auto iput_nodes_buffer = pool.get_builder()
                                 .set_size(input_nodes_data.size() * sizeof(glm::ivec4))
                                 .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
                                 .create([ind = std::move(input_nodes_data)](uint8_t* dst) {
                                     std::memcpy(dst, ind.data(), ind.size() * sizeof(ind.front()));
                                 });

    for (auto& anim : result.m_animations) {
        anim.m_nodes_buffer = iput_nodes_buffer;
    }
}


void sandbox::gltf::vk_model_builder::create_anim_exec_order_buffer(const gltf::model& mdl, vk_model& result, hal::render::avk::buffer_pool& pool)
{
    std::vector<glm::ivec4> exec_order;

    exec_order.reserve((mdl.get_nodes().size() / 4) + 1);
    exec_order.emplace_back(-1, -1, -1, -1);
    size_t write_index = 0;

    for_each_scene_node(mdl, [&write_index, &exec_order](const node& node, int32_t node_index) {
        if (write_index >= exec_order.back().length()) {
            write_index = 0;
            exec_order.emplace_back(-1, -1, -1, -1);
        }

        exec_order.back()[write_index++] = node_index;
    });

    auto exec_order_buffer = pool.get_builder()
                                 .set_size(exec_order.size() * sizeof(glm::ivec4))
                                 .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
                                 .create([eo = std::move(exec_order)](uint8_t* dst) {
                                     std::memcpy(dst, eo.data(), eo.size() * sizeof(eo.front()));
                                 });

    for (auto& anim : result.m_animations) {
        anim.m_exec_order_buffer = exec_order_buffer;
    }
}


void vk_model_builder::get_vertex_attributes_data_from_fixed_format(
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


void vk_model_builder::copy_attribute_data(
    const primitive::vertex_attribute& attribute,
    vk::Format desired_vk_format,
    uint64_t vtx_size,
    uint64_t offset,
    uint8_t* dst)
{
    auto [desired_type, desired_component_type] = from_vk_format(desired_vk_format);

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


void animation_instance::update(uint64_t dt)
{
    if (m_playback_state == playback_state::paused) {
        return;
    }

    m_curr_position += dt;

    m_curr_position = m_start_position + (m_curr_position % (m_end_position - m_start_position));
}


animation_instance::animation_instance(const model& model)
    : m_model(model)
{
    set_animation(0);
}


void animation_instance::play()
{
    m_playback_state = playback_state::playing;
}


void sandbox::gltf::animation_instance::set_current_position(uint64_t curr_pos_us)
{
    m_curr_position = std::clamp(curr_pos_us, m_start_position, m_end_position);
}


void sandbox::gltf::animation_instance::set_start_positon(uint64_t start_pos_us)
{
    const auto& current_animation = m_model.get_animations()[m_current_animation];
    const uint64_t duration_us = current_animation.get_duration() * 1e6;
    m_start_position = std::min(start_pos_us, duration_us);
    set_current_position(m_start_position);
}


void sandbox::gltf::animation_instance::set_end_positon(uint64_t end_pos_us)
{
    const uint64_t duration_us = m_model.get_animations()[m_current_animation].get_duration() * 1e6;
    m_end_position = std::min(end_pos_us, duration_us);
}


void sandbox::gltf::animation_instance::set_animation(uint64_t animation_index)
{
    m_current_animation = animation_index;
    set_end_positon(-1);
    set_start_positon(0);
}


void animation_instance::pause()
{
    m_playback_state = playback_state::paused;
}


void animation_instance::stop()
{
    m_playback_state = playback_state::paused;
    set_current_position(m_start_position);
}


void animation_instance::resume()
{
    m_playback_state = playback_state::playing;
}


const std::vector<vk_mesh>& vk_model::get_meshes() const
{
    return m_meshes;
}


const std::vector<vk_animation>& sandbox::gltf::vk_model::get_animations() const
{
    return m_animations;
}


vk_model::vertex_format vk_model::get_vertex_format()
{
    return {m_attributes.data(), uint32_t(m_attributes.size()), m_bindings.data(), uint32_t(m_bindings.size())};
}


const std::vector<vk_primitive>& vk_mesh::get_primitives() const
{
    return m_primitives;
}


const vk_skin& vk_mesh::get_skin() const
{
    return m_skin;
}


bool vk_mesh::is_skinned() const
{
    return m_skinned;
}


const avk::buffer_instance& vk_primitive::get_vertex_buffer() const
{
    return m_vertex_buffer;
}


uint32_t vk_primitive::get_vertices_count() const
{
    return m_vertices_count;
}


const avk::buffer_instance& vk_primitive::get_index_buffer() const
{
    return m_index_buffer;
}


vk::IndexType vk_primitive::get_indices_type() const
{
    return m_index_type;
}


uint32_t vk_primitive::get_indices_count() const
{
    return m_indices_count;
}


const hal::render::avk::buffer_instance& vk_skin::get_joints_buffer() const
{
    return m_joints_buffer;
}


uint32_t vk_skin::get_joints_count() const
{
    return m_joints_count;
}


uint32_t vk_skin::get_hierarchy_size() const
{
    return m_hierarchy_size;
}

const hal::render::avk::buffer_instance& sandbox::gltf::vk_animation::get_meta_buffer() const
{
    return m_meta_buffer;
}

const hal::render::avk::buffer_instance& sandbox::gltf::vk_animation::get_time_stamps_buffer() const
{
    return m_time_stamps_buffer;
}

const hal::render::avk::buffer_instance& sandbox::gltf::vk_animation::get_keys_buffer() const
{
    return m_keys_buffer;
}

const hal::render::avk::buffer_instance& sandbox::gltf::vk_animation::get_nodes_buffer() const
{
    return m_nodes_buffer;
}

const hal::render::avk::buffer_instance& sandbox::gltf::vk_animation::get_exec_order_buffer() const
{
    return m_exec_order_buffer;
}


sandbox::gltf::animation_controller::animation_controller(
    const gltf::model& gltf_model,
    const gltf::vk_model& vk_model)
    : m_gltf_model(&gltf_model)
    , m_vk_model(&vk_model)
{
}


void sandbox::gltf::animation_controller::init_resources(hal::render::avk::buffer_pool& pool, size_t instances_count)
{
    ASSERT(m_gltf_model != nullptr && m_vk_model != nullptr);

    m_batch_size = instances_count + 64 - (instances_count % 64);

    m_progressions.reserve(m_batch_size);
    m_hierarchies.reserve(m_batch_size);

    for (uint32_t i = 0; i < m_batch_size; i++) {
        auto prog_buffer = pool
            .get_builder()
            .set_size(sizeof(glm::uvec4))
            .set_usage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer)
            .create([i](uint8_t* dst) {
                glm::uvec4 v{0, 0, i, 0};
                std::memcpy(dst, glm::value_ptr(v), sizeof(v));
            });

        auto h_buffer = pool
            .get_builder()
            .set_size(m_gltf_model->get_nodes().size() * sizeof(glm::mat4))
            .set_usage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer)
            .create();

        m_progressions.emplace_back(prog_buffer);
        m_hierarchies.emplace_back(h_buffer);
    }

    m_animation_instances.reserve(m_batch_size);
}


void sandbox::gltf::animation_controller::init_pipelines()
{
    ASSERT(m_gltf_model != nullptr && m_vk_model != nullptr);

    sandbox::hal::filesystem::common_file file{};
    file.open(WORK_DIR "/gltf/resources/skinning.comp.spv");
    ASSERT(file.get_size());

    m_shader = avk::create_shader_module(avk::context::device()->createShaderModule(vk::ShaderModuleCreateInfo{
        .flags = {},
        .codeSize = file.get_size(),
        .pCode = reinterpret_cast<const uint32_t*>(file.read_all().get_data())}));

    std::vector<avk::buffer_instance> meta_buffers{};
    meta_buffers.reserve(m_vk_model->get_animations().size());
    std::vector<avk::buffer_instance> time_stamps_buffers{};
    time_stamps_buffers.reserve(m_vk_model->get_animations().size());
    std::vector<avk::buffer_instance> keys_buffers{};
    keys_buffers.reserve(m_vk_model->get_animations().size());
    std::vector<avk::buffer_instance> exec_order_buffers{};
    exec_order_buffers.reserve(m_vk_model->get_animations().size());
    std::vector<avk::buffer_instance> nodes_buffers{};
    nodes_buffers.reserve(m_vk_model->get_animations().size());

    for (const auto& anim : m_vk_model->get_animations()) {
        meta_buffers.emplace_back(anim.get_meta_buffer());
        time_stamps_buffers.emplace_back(anim.get_time_stamps_buffer());
        keys_buffers.emplace_back(anim.get_keys_buffer());
        exec_order_buffers.emplace_back(anim.get_exec_order_buffer());
        nodes_buffers.emplace_back(anim.get_nodes_buffer());
    }

    // clang-format off
    m_pipeline = avk::pipeline_builder().set_shader_stages({{m_shader, vk::ShaderStageFlagBits::eCompute}})
      .add_specialization_constant<uint32_t>(m_gltf_model->get_animations().size())
      .add_specialization_constant<uint32_t>(m_gltf_model->get_nodes().size())
      .add_specialization_constant<uint32_t>(m_batch_size)
      .begin_descriptor_set()
      .add_buffers(m_progressions, vk::DescriptorType::eStorageBuffer)
      .add_buffers(meta_buffers, vk::DescriptorType::eStorageBuffer)
      .add_buffers(time_stamps_buffers, vk::DescriptorType::eStorageBuffer)
      .add_buffers(keys_buffers, vk::DescriptorType::eStorageBuffer)
      .add_buffers(exec_order_buffers, vk::DescriptorType::eStorageBuffer)
      .add_buffers(nodes_buffers, vk::DescriptorType::eStorageBuffer)
      .add_buffers(m_hierarchies, vk::DescriptorType::eStorageBuffer)
      .create_compute_pipeline();
    // clang-format on
}


void sandbox::gltf::animation_controller::update(uint64_t dt)
{
    for (size_t i = 0; i < m_animation_instances.size(); i++) {
        auto& anim_instance = m_animation_instances[i];
        anim_instance.update(dt);

        m_progressions[i].upload([i, curr_anim = anim_instance.m_current_animation, curr_pos = anim_instance.m_curr_position](uint8_t* dst) {
            glm::uvec4 v{curr_anim, curr_pos, i, 0};
            std::memcpy(dst, glm::value_ptr(v), sizeof(v));
        });
    }
}


void sandbox::gltf::animation_controller::update(vk::CommandBuffer& command_buffer)
{
    m_pipeline.activate(command_buffer);
    command_buffer.dispatch(m_batch_size, 1, 1);

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eVertexShader,
        {},
        {vk::MemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        }},
        {},
        {});
}


const std::vector<hal::render::avk::buffer_instance>& sandbox::gltf::animation_controller::get_hierarchies() const
{
    return m_hierarchies;
}


const std::vector<hal::render::avk::buffer_instance>& sandbox::gltf::animation_controller::get_progressions() const
{
    return m_progressions;
}

animation_instance* sandbox::gltf::animation_controller::instantiate_animation()
{
    return &m_animation_instances.emplace_back(*m_gltf_model);
}


