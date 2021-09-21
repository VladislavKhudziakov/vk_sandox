#pragma once

#include <gltf/gltf_vk.hpp>
#include <render/vk/raii.hpp>

namespace sandbox::gltf
{
    enum class pipeline_primitive_topology
    {
        triangles,
        lines,
        points,
    };

    enum class pipeline_blend_mode
    {
        none
    };

    vk::Format to_vk_format(accessor_type accessor_type, component_type component_type);
    vk::IndexType to_vk_index_type(accessor_type accessor_type, component_type component_type);
    std::pair<vk::Filter, vk::SamplerMipmapMode> to_vk_sampler_filter(sampler_filter_type filter);
    vk::SamplerAddressMode to_vk_sampler_wrap(sampler_wrap_type wrap);

    bool need_mips(sampler_filter_type filter);

    void draw_primitive(
        const gltf::vk_primitive& primitive,
        const vk::Buffer& vertex_buffer,
        const vk::Buffer& index_buffer,
        vk::CommandBuffer& command_buffer);

    sandbox::hal::render::avk::graphics_pipeline create_pipeline_from_primitive(
        const gltf::vk_primitive& primitive,
        const std::vector<std::pair<vk::ShaderModule, vk::ShaderStageFlagBits>>& stages,
        vk::PipelineLayout layout,
        vk::RenderPass pass,
        uint32_t subpass,

        pipeline_primitive_topology topology = pipeline_primitive_topology::triangles,
        pipeline_blend_mode blend = pipeline_blend_mode::none,
        bool backfaces = false,
        bool zwrite = true,
        bool ztest = true,
        bool color_write = true);

    vk::Format stb_channels_count_to_vk_format(int32_t);

    sandbox::hal::render::avk::descriptor_set_layout
    create_material_textures_layout(const gltf::material& material);

    void write_material_textures_descriptors(
        const gltf::material& material,
        vk::DescriptorSet dst_set,
        const gltf::vk_texture_atlas& tex_atlas);


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
