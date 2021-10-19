#include "vk_utils.hpp"

#include <render/vk/utils.hpp>
#include <utils/conditions_helpers.hpp>

using namespace sandbox;
using namespace sandbox::hal::render;


vk::IndexType sandbox::gltf::to_vk_index_type(
    sandbox::gltf::accessor_type accessor_type,
    sandbox::gltf::component_type component_type)
{
    CHECK_MSG(
        accessor_type == accessor_type::scalar,
        "Cannot convert non scalar accessor type into vulkan index type.");

    switch (component_type) {
        case component_type::unsigned_byte:
            return vk::IndexType::eUint8EXT;
        case component_type::unsigned_short:
            return vk::IndexType::eUint16;
        case component_type::unsigned_int:
            return vk::IndexType::eUint32;
        default:
            throw std::runtime_error(
                "Cannot convert " + to_string(component_type) + " component type into vulkan index type.");
    }
}


vk::Format sandbox::gltf::to_vk_format(
    sandbox::gltf::accessor_type accessor_type,
    sandbox::gltf::component_type component_type)
{
    switch (accessor_type) {
        case accessor_type::scalar:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32Uint;
                case component_type::float32:
                    return vk::Format::eR32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        case accessor_type::vec2:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8G8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8G8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16G16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16G16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32G32Uint;
                case component_type::float32:
                    return vk::Format::eR32G32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        case accessor_type::vec3:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8G8B8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8G8B8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16G16B16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16G16B16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32G32B32Uint;
                case component_type::float32:
                    return vk::Format::eR32G32B32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        case accessor_type::vec4:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8G8B8A8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8G8B8A8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16G16B16A16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16G16B16A16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32G32B32A32Uint;
                case component_type::float32:
                    return vk::Format::eR32G32B32A32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        default:
            throw std::runtime_error("Bad accessor type " + to_string(accessor_type));
    }
}


void sandbox::gltf::draw_primitive(const gltf::vk_primitive& primitive, vk::CommandBuffer& command_buffer)
{
    const auto& vert_buffer = primitive.get_vertex_buffer();
    
    command_buffer.bindVertexBuffers(0, {vert_buffer}, {vert_buffer.get_offset()});

    if (primitive.get_indices_count() > 0) {
        command_buffer.bindIndexBuffer(primitive.get_index_buffer(), primitive.get_index_buffer().get_offset(), primitive.get_indices_type());
        command_buffer.drawIndexed(primitive.get_indices_count(), 1, 0, 0, 0);
    } else {
        command_buffer.draw(primitive.get_indices_count(), 1, 0, 0);
    }
}


vk::SamplerAddressMode sandbox::gltf::to_vk_sampler_wrap(sandbox::gltf::sampler_wrap_type wrap)
{
    switch (wrap) {
        case sampler_wrap_type::repeat:
            return vk::SamplerAddressMode::eRepeat;
        case sampler_wrap_type::clamp_to_edge:
            return vk::SamplerAddressMode::eClampToEdge;
        case sampler_wrap_type::mirrored_repeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        default:
            throw std::runtime_error("Bad sampler wrap type.");
    }
}


std::pair<vk::Filter, vk::SamplerMipmapMode> sandbox::gltf::to_vk_sampler_filter(sandbox::gltf::sampler_filter_type filter)
{
    switch (filter) {
        case sampler_filter_type::nearest:
            [[fallthrough]];
        case sampler_filter_type::near_mipmap_nearest:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest};
        case sampler_filter_type::linear:
            [[fallthrough]];
        case sampler_filter_type::linear_mipmap_nearest:
            return {vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest};
        case sampler_filter_type::near_mipmap_linear:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};
        case sampler_filter_type::linear_mipmap_linear:
            return {vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
        default:
            throw std::runtime_error("Bad sampler filter type.");
    }
}


bool sandbox::gltf::need_mips(sandbox::gltf::sampler_filter_type filter)
{
    // clang-format off
    return filter == sampler_filter_type::linear_mipmap_linear ||
           filter == sampler_filter_type::near_mipmap_nearest ||
           filter == sampler_filter_type::linear_mipmap_nearest ||
           filter == sampler_filter_type::near_mipmap_linear;
    // clang-format on
}


vk::Format sandbox::gltf::stb_channels_count_to_vk_format(int32_t count)
{
    switch (count) {
        case 1:
            return vk::Format::eR8Srgb;
        case 2:
            return vk::Format::eR8G8Srgb;
        case 3:
            return vk::Format::eR8G8B8Srgb;
        case 4:
            return vk::Format::eR8G8B8A8Srgb;
        default:
            throw std::runtime_error("Bad components count");
    }
}


std::pair<gltf::accessor_type, gltf::component_type> gltf::from_vk_format(vk::Format format)
{
    switch (format) {
        case vk::Format::eR8Sint:
            return {accessor_type::scalar, component_type::signed_byte};
        case vk::Format::eR8G8Sint:
            return {accessor_type::vec2, component_type::signed_byte};
        case vk::Format::eR8G8B8Sint:
            return {accessor_type::vec3, component_type::signed_byte};
        case vk::Format::eR8G8B8A8Sint:
            return {accessor_type::vec4, component_type::signed_byte};

        case vk::Format::eR8Uint:
            return {accessor_type::scalar, component_type::unsigned_byte};
        case vk::Format::eR8G8Uint:
            return {accessor_type::vec2, component_type::unsigned_byte};
        case vk::Format::eR8G8B8Uint:
            return {accessor_type::vec3, component_type::unsigned_byte};
        case vk::Format::eR8G8B8A8Uint:
            return {accessor_type::vec4, component_type::unsigned_byte};

        case vk::Format::eR16Sint:
            return {accessor_type::scalar, component_type::signed_short};
        case vk::Format::eR16G16Sint:
            return {accessor_type::vec2, component_type::signed_short};
        case vk::Format::eR16G16B16Sint:
            return {accessor_type::vec3, component_type::signed_short};
        case vk::Format::eR16G16B16A16Sint:
            return {accessor_type::vec4, component_type::signed_short};

        case vk::Format::eR16Uint:
            return {accessor_type::scalar, component_type::unsigned_short};
        case vk::Format::eR16G16Uint:
            return {accessor_type::vec2, component_type::unsigned_short};
        case vk::Format::eR16G16B16Uint:
            return {accessor_type::vec3, component_type::unsigned_short};
        case vk::Format::eR16G16B16A16Uint:
            return {accessor_type::vec4, component_type::unsigned_short};

        case vk::Format::eA2B10G10R10SintPack32:
            [[fallthrough]];
        case vk::Format::eA2B10G10R10SnormPack32:
            [[fallthrough]];
        case vk::Format::eA2R10G10B10UintPack32:
            [[fallthrough]];
        case vk::Format::eA2B10G10R10UnormPack32:
            [[fallthrough]];
        case vk::Format::eR32Uint:
            return {accessor_type::scalar, component_type::unsigned_int};
        case vk::Format::eR32G32Uint:
            return {accessor_type::vec2, component_type::unsigned_int};
        case vk::Format::eR32G32B32Uint:
            return {accessor_type::vec3, component_type::unsigned_int};
        case vk::Format::eR32G32B32A32Uint:
            return {accessor_type::vec4, component_type::unsigned_int};

        case vk::Format::eR32Sfloat:
            return {accessor_type::scalar, component_type::float32};
        case vk::Format::eR32G32Sfloat:
            return {accessor_type::vec2, component_type::float32};
        case vk::Format::eR32G32B32Sfloat:
            return {accessor_type::vec3, component_type::float32};
        case vk::Format::eR32G32B32A32Sfloat:
            return {accessor_type::vec4, component_type::float32};
        default:
            throw std::runtime_error("Unsupported type.");
    }
}


sandbox::gltf::vk_material_info sandbox::gltf::vk_material_info::from_gltf_material(const sandbox::gltf::material& material)
{
    auto get_texture_cords_set = [](const gltf::material::texture_data& tex_data) {
        if (tex_data.index < 0) {
            return -1;
        } else {
            return static_cast<int32_t>(tex_data.coord_set);
        }
    };

    const auto& pbr_metallic_roughness = material.get_pbr_metallic_roughness();

    return {
        .base_color = pbr_metallic_roughness.base_color,
        .base_color_texture_set = get_texture_cords_set(pbr_metallic_roughness.base_color_texture),
        .metallic_factor = pbr_metallic_roughness.metallic_factor,
        .roughness_factor = pbr_metallic_roughness.roughness_factor,
        .metallic_roughness_texture_set = get_texture_cords_set(pbr_metallic_roughness.metallic_roughness_texture),
        .normal_texture_set = get_texture_cords_set(material.get_normal_texture()),
        .occlusion_texture_set = get_texture_cords_set(material.get_occlusion_texture()),
        .emissive_texture_set = get_texture_cords_set(material.get_emissive_texture()),
        .emissive_factor = material.get_emissive_factor(),
        .alpha_mode = static_cast<int32_t>(material.get_alpha_mode()),
        .alpha_cutoff = material.get_alpha_cutoff(),
        .double_sided = material.is_double_sided()};
}


gltf::cpu_animation_controller::cpu_animation_controller(const gltf::animation& animation)
    : m_animation(animation)
{
}


void gltf::cpu_animation_controller::update(const gltf::model& model, uint64_t dt)
{
    if (!m_need_update) {
        return;
    }

    calculate_frame(model);

    m_curr_time += dt;
}


void gltf::cpu_animation_controller::play()
{
    m_need_update = true;
}


void gltf::cpu_animation_controller::pause()
{
    m_need_update = false;
}


void gltf::cpu_animation_controller::calculate_frame(const gltf::model& model)
{
    const auto& accessors = model.get_accessors();
    const auto& buffers = model.get_buffers();
    const auto& buffer_views = model.get_buffer_views();

    const auto& channels = m_animation.get_channels();
    const auto& samplers = m_animation.get_samplers();

    m_trs_transforms.resize(model.get_nodes().size());
    m_transformations.resize(model.get_nodes().size(), glm::mat4{1});

    float curr_time_s = double(m_curr_time) / 1e6;

    for (const auto& channel : channels) {
        const auto& sampler = samplers[channel.get_sampler()];

        auto input_accessor = accessors[sampler.get_input()];
        auto output_accessor = accessors[sampler.get_output()];

        if (m_curr_key >= input_accessor.get_count()) {
            continue;
        }

        const auto* keys = (const float*) input_accessor.get_data(buffers.data(), buffers.size(), buffer_views.data(), buffer_views.size());
        const auto* vals = output_accessor.get_data(buffers.data(), buffers.size(), buffer_views.data(), buffer_views.size());

        assert(input_accessor.get_component_type() == component_type::float32);
        assert(input_accessor.get_type() == accessor_type::scalar);

        auto next_key = std::min(m_curr_key + 1, input_accessor.get_count() - 1);

        if (curr_time_s > keys[next_key]) {
            m_curr_key = next_key;
            next_key = std::min(m_curr_key + 1, input_accessor.get_count() - 1);
        }

        if (m_curr_key == next_key) {
            m_need_update = false;
        }

        auto cc = keys[m_curr_key];
        auto nc = keys[next_key];

        auto frame_duration = keys[next_key] - keys[m_curr_key];
        float frame_progress{1};
        if (frame_duration > 0) {
            frame_progress = (curr_time_s - keys[m_curr_key]) / frame_duration;
        }

        auto v4_to_quat = [](const glm::vec4& v) {
            return glm::quat{v.w, v.x, v.y, v.z};
        };

        switch (channel.get_path()) {
            case animation_path::rotation:
                assert(output_accessor.get_component_type() == component_type::float32);
                assert(output_accessor.get_type() == accessor_type::vec4);

                m_trs_transforms[channel.get_node()].first.rotation = interpolate_quat(
                    v4_to_quat(reinterpret_cast<const glm::vec4*>(vals)[m_curr_key]),
                    v4_to_quat(reinterpret_cast<const glm::vec4*>(vals)[next_key]),
                    sampler.get_interpolation(),
                    frame_progress);

                m_trs_transforms[channel.get_node()].second = true;
                break;
            case animation_path::translation:
                assert(output_accessor.get_component_type() == component_type::float32);
                assert(output_accessor.get_type() == accessor_type::vec3);

                m_trs_transforms[channel.get_node()].first.translation = interpolate_vec3(
                    reinterpret_cast<const glm::vec3*>(vals)[m_curr_key],
                    reinterpret_cast<const glm::vec3*>(vals)[next_key],
                    sampler.get_interpolation(),
                    frame_progress);
                m_trs_transforms[channel.get_node()].second = true;
                break;
            case animation_path::scale:
                assert(output_accessor.get_component_type() == component_type::float32);
                assert(output_accessor.get_type() == accessor_type::vec3);

                m_trs_transforms[channel.get_node()].first.scale = interpolate_vec3(
                    reinterpret_cast<const glm::vec3*>(vals)[m_curr_key],
                    reinterpret_cast<const glm::vec3*>(vals)[next_key],
                    sampler.get_interpolation(),
                    frame_progress);
                m_trs_transforms[channel.get_node()].second = true;
                break;
            default:
                throw std::runtime_error("Bad path.");
        }
    }

    glm::mat4 parent_transform{1};
    for_each_scene_node(
        model,
        [this](const gltf::node& node, int32_t node_index, const glm::mat4& parent_transform) {
            if (m_trs_transforms[node_index].second) {
                m_transformations[node_index] = parent_transform * node::gen_matrix(m_trs_transforms[node_index].first);
                return m_transformations[node_index];
            } else {
                m_transformations[node_index] = parent_transform * node.get_matrix();
                return m_transformations[node_index];
            }
        },
        parent_transform);
}


glm::quat gltf::cpu_animation_controller::interpolate_quat(
    const glm::quat& x, const glm::quat& y, gltf::animation_interpolation interpolation_type, float delta)
{
    if (interpolation_type != animation_interpolation::step && delta > 0) {
        return glm::slerp(x, y, delta);
    }
    return x;
}


glm::vec3 gltf::cpu_animation_controller::interpolate_vec3(
    const glm::vec3& x,
    const glm::vec3& y,
    gltf::animation_interpolation interpolation_type,
    float delta)
{
    if (interpolation_type == animation_interpolation::linear && delta > 0) {
        return glm::mix(x, y, delta);
    } else if (interpolation_type == animation_interpolation::cubic_spline && delta > 0) {
        return glm::smoothstep(x, y, glm::vec3{delta, delta, delta});
    }

    return x;
}


const std::vector<glm::mat4> gltf::cpu_animation_controller::get_transformations() const
{
    return m_transformations;
}
