#include "vk_utils.hpp"

#include "gltf_vk.hpp"


#include <utils/conditions_helpers.hpp>


vk::IndexType sandbox::gltf::to_vk_index_type(sandbox::gltf::accessor_type_value accessor_type, sandbox::gltf::component_type component_type)
{
    CHECK_MSG(accessor_type == accessor_type::scalar, "Cannot convert non scalar accessor type into vulkan index type.");

    switch (component_type) {
        case component_type::unsigned_byte:
            return vk::IndexType::eUint8EXT;
        case component_type::unsigned_short:
            return vk::IndexType::eUint16;
        case component_type::unsigned_int:
            return vk::IndexType::eUint32;
        default:
            throw std::runtime_error("Cannot convert " + to_string(component_type) + " component type into vulkan index type.");
    }
}


vk::Format sandbox::gltf::to_vk_format(sandbox::gltf::accessor_type_value accessor_type, sandbox::gltf::component_type component_type)
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


void sandbox::gltf::draw_primitive(
    const sandbox::gltf::primitive& primitive,
    const vk::Buffer& vertex_buffer,
    const vk::Buffer& index_buffer,
    vk::CommandBuffer& command_buffer)
{
    const auto& primitive_impl = static_cast<const gltf::gltf_vk::primitive&>(primitive);
    const vk::DeviceSize vertex_buffer_offset = primitive_impl.get_vertex_buffer_offset();

    thread_local static std::vector<vk::Buffer> buffers{};
    thread_local static std::vector<vk::DeviceSize> offsets{};

    buffers.clear();
    offsets.clear();

    buffers.insert(buffers.begin(), primitive_impl.get_vertex_bindings().size(), vertex_buffer);
    offsets.insert(offsets.begin(), primitive_impl.get_vertex_bindings().size(), vertex_buffer_offset);

    command_buffer.bindVertexBuffers(0, buffers.size(), buffers.data(), offsets.data());

    if (primitive_impl.get_index_type() != vk::IndexType::eNoneKHR) {
        command_buffer.bindIndexBuffer(index_buffer, primitive_impl.get_index_buffer_offset(), primitive_impl.get_index_type());
        command_buffer.drawIndexed(primitive_impl.get_indices_data().indices_count, 1, 0, 0, 0);
    } else {
        command_buffer.draw(primitive_impl.get_vertices_count(), 1, 0, 0);
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
        .double_sided = material.is_double_sided()
    };
}
