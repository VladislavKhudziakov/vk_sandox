#pragma once

#include <gltf/gltf_vk.hpp>
#include <render/vk/raii.hpp>

namespace sandbox::gltf
{
    struct instance_transform_data
    {
        glm::mat4 model{1};
        glm::mat4 view{1};
        glm::mat4 proj{1};
        glm::mat4 mvp{1};
    };

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

    class cpu_animation_controller
    {
    public:
        static glm::quat interpolate_quat(const glm::quat& x, const glm::quat& y, animation_interpolation, float delta);
        static glm::vec3 interpolate_vec3(const glm::vec3& x, const glm::vec3& y, animation_interpolation, float delta);

        explicit cpu_animation_controller(const gltf::animation& animation);
        void update(const gltf::model&, uint64_t dt);
        void play();
        void pause();

        const std::vector<glm::mat4> get_transformations() const;

    private:
        void calculate_frame(const gltf::model&);

        bool m_need_update = false;
        uint64_t m_curr_time{0};
        uint64_t m_curr_key{0};

        const gltf::animation& m_animation;

        std::vector<glm::mat4> m_transformations{};
        std::vector<std::pair<gltf::node::trs_transform, bool>> m_trs_transforms{};
    };

    vk::Format to_vk_format(accessor_type accessor_type, component_type component_type);
    std::pair<accessor_type, component_type> from_vk_format(vk::Format);

    vk::IndexType to_vk_index_type(accessor_type accessor_type, component_type component_type);
    std::pair<vk::Filter, vk::SamplerMipmapMode> to_vk_sampler_filter(sampler_filter_type filter);
    vk::SamplerAddressMode to_vk_sampler_wrap(sampler_wrap_type wrap);

    bool need_mips(sampler_filter_type filter);

    void draw_primitive(
        const gltf::vk_primitive& primitive,
        vk::CommandBuffer& command_buffer);

    vk::Format stb_channels_count_to_vk_format(int32_t);

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
    static_assert(sizeof(vk_material_info) == sizeof(float) * 4 * 12);
} // namespace sandbox::gltf
