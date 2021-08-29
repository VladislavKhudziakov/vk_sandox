#pragma once

#include <gltf/gltf_base.hpp>
#include <render/vk/vulkan_dependencies.hpp>

namespace sandbox::gltf
{
    vk::Format to_vk_format(accessor_type_value accessor_type, component_type component_type);
    vk::IndexType to_vk_index_type(accessor_type_value accessor_type, component_type component_type);

    void draw_primitive(
        const gltf::primitive& primitive,
        const vk::Buffer& vertex_buffer,
        const vk::Buffer& index_buffer,
        vk::CommandBuffer& command_buffer);


    struct vk_material_info
    {
        vk_material_info static from_gltf_material(const gltf::material&);

        glm::vec4 base_color{1, 1, 1, 1};
        int32_t base_color_texture_set{};

        alignas(sizeof(float) * 4) float metallic_factor{1};
        alignas(sizeof(float) * 4) float roughness_factor{1};
        alignas(sizeof(float) * 4) int32_t metallic_roughness_texture_set{};

        alignas(sizeof(float) * 4) int32_t normal_texture_set{};
        alignas(sizeof(float) * 4) int32_t occlusion_texture_set{};
        alignas(sizeof(float) * 4) int32_t emissive_texture_set{};

        alignas(sizeof(float) * 4) glm::vec3 emissive_factor{0, 0, 0};
        alignas(sizeof(float) * 4) int32_t alpha_mode{};
        alignas(sizeof(float) * 4) float alpha_cutoff{0.5};

        alignas(sizeof(float) * 4) int32_t double_sided{0};
    };

    static_assert(std::is_same_v<glm::vec4::value_type, float>);
    static_assert(sizeof(float) == 4);
    static_assert(sizeof(vk_material_info) == sizeof(float) * 4 * 12);
} // namespace sandbox::gltf
