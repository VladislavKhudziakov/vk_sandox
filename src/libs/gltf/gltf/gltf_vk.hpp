#pragma once

#include <gltf/gltf_base.hpp>
#include <render/vk/raii.hpp>

#include <string>

namespace sandbox::gltf
{
    struct vk_primitive
    {
        std::vector<vk::VertexInputAttributeDescription> attributes{};
        std::vector<vk::VertexInputBindingDescription> bindings{};
        uint64_t vertex_buffer_offset{0};
        uint32_t vertices_count{0};
        vk::IndexType index_type{vk::IndexType::eNoneKHR};
        uint64_t index_buffer_offset{0};
        uint32_t indices_count{0};
    };


    class vk_geometry
    {
        friend class vk_geometry_builder;

    public:
        vk_geometry() = default;

        vk_geometry(const vk_geometry&) = delete;
        vk_geometry& operator=(const vk_geometry&) = delete;

        vk_geometry(vk_geometry&&) noexcept = default;
        vk_geometry& operator=(vk_geometry&&) noexcept = default;

        ~vk_geometry() = default;

        const std::vector<vk_primitive>& get_primitives(uint32_t mesh) const;

        vk::Buffer get_vertex_buffer() const;
        vk::Buffer get_index_buffer() const;

        void clear_staging_resources();

    private:
        std::vector<std::vector<vk_primitive>> m_primitives{};

        hal::render::avk::vma_buffer m_vertex_buffer{};
        hal::render::avk::vma_buffer m_index_buffer{};

        hal::render::avk::vma_buffer m_vertex_staging_buffer{};
        hal::render::avk::vma_buffer m_index_staging_buffer{};
    };

    class vk_geometry_builder
    {
    public:
        vk_geometry_builder() = default;
        vk_geometry_builder& set_fixed_vertex_format(const std::array<vk::Format, 8>&);
        vk_geometry create_with_fixed_format(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family);
        vk_geometry create(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family);

    private:
        void copy_attribute_data(
            const gltf::primitive::vertex_attribute& attribute,
            vk::Format desired_vk_format,
            uint64_t vtx_size,
            uint64_t offset,
            const uint8_t* dst);

        std::pair<hal::render::avk::vma_buffer, hal::render::avk::vma_buffer> create_index_buffer(
            const gltf::model& mdl,
            size_t index_buffer_size,
            const std::vector<std::vector<vk_primitive>>& meshes,
            vk::CommandBuffer& command_buffer,
            uint32_t queue_famil);

        std::optional<std::array<vk::Format, 8>> m_fixed_format{};
    };

    struct vk_skin
    {
        uint64_t offset{0};
        uint64_t size{0};
        uint64_t count{0};
    };


    class vk_geometry_skins
    {
        friend class skin_builder;

    public:
        static void create_skins_data(
            const gltf::model& mdl,
            std::vector<vk_skin>& skins,
            hal::render::avk::vma_buffer& result_staging_buffer,
            hal::render::avk::vma_buffer& result_dst_buffer,
            vk::CommandBuffer& command_buffer,
            uint32_t queue_family);

        static void create_hierarchy_data(
            const gltf::model& mdl,
            std::vector<glm::mat4>& default_data,
            hal::render::avk::vma_buffer& result_staging_buffer,
            hal::render::avk::vma_buffer& result_dst_buffer,
            vk::CommandBuffer& command_buffer,
            uint32_t queue_family);

        static vk_geometry_skins from_gltf_model(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family);

        const std::vector<vk_skin>& get_skins() const;
        const std::vector<glm::mat4>& get_hierarchy_transforms() const;
        const hal::render::avk::vma_buffer& get_hierarchy_staging_buffer() const;
        const hal::render::avk::vma_buffer& get_hierarchy_buffer() const;
        const hal::render::avk::vma_buffer& get_skin_buffer() const;

    private:
        struct joint_data
        {
            glm::mat4 inv_bind_pose{1};
            alignas(sizeof(float) * 4) uint32_t joint{std::numeric_limits<uint32_t>::max()};
        };

        static_assert(sizeof(joint_data) == sizeof(float) * 4 * 4 + sizeof(float) * 4);

        std::vector<vk_skin> m_skins{};
        std::vector<glm::mat4> m_default_hierarchy_transforms{};

        hal::render::avk::vma_buffer m_skin_buffer{};
        hal::render::avk::vma_buffer m_hierarchy_buffer{};

        hal::render::avk::vma_buffer m_skin_staging_buffer{};
        hal::render::avk::vma_buffer m_hierarchy_staging_buffer{};
    };


    struct vk_animation
    {
        uint32_t offset{0};
        uint32_t size{0};
    };


    class vk_animations_list
    {
        friend class animations_builder;

    public:
        const std::vector<vk_animation>& get_animations() const;

        std::tuple<vk::Buffer, uint32_t, uint32_t> get_anim_buffer() const;
        std::tuple<vk::Buffer, uint32_t, uint32_t> get_nodes_buffer() const;
        std::tuple<vk::Buffer, uint32_t, uint32_t> get_exec_order_buffer() const;
        uint32_t get_nodes_count() const;

    private:
        std::vector<vk_animation> m_animations_list{};

        hal::render::avk::vma_buffer m_anim_staging_buffer{};
        hal::render::avk::vma_buffer m_anim_buffer{};
        std::pair<uint32_t, uint32_t> m_anim_sub_buffer{};

        hal::render::avk::vma_buffer m_nodes_staging_buffer{};
        hal::render::avk::vma_buffer m_nodes_buffer{};
        std::pair<uint32_t, uint32_t> m_nodes_sub_buffer{};

        hal::render::avk::vma_buffer m_exec_order_staging_buffer{};
        hal::render::avk::vma_buffer m_exec_order_buffer{};
        std::pair<uint32_t, uint32_t> m_exec_order_sub_buffer{};
    };


    class animations_builder
    {
    public:
        vk_animations_list create(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family);

    private:
        struct gpu_trs
        {
            glm::vec3 translation{0, 0, 0};
            alignas(sizeof(float) * 4) glm::vec4 rotation{0, 0, 0, 1};
            alignas(sizeof(float) * 4) glm::vec3 scale{1, 1, 1};
        };

        static_assert(sizeof(gpu_trs) == sizeof(float) * 4 * 3);

        struct gpu_anim_key
        {
            gpu_trs trs{};
        };

        struct gpu_anim_sampler
        {
            float ts = 0;
            std::vector<gpu_anim_key> nodes_keys{};
        };

        struct gpu_animation
        {
            uint64_t offset{0};
            uint64_t size{0};
            glm::ivec4 interpolation_avg_frame_duration{-1, -1, -1, 0};
            std::vector<gpu_anim_sampler> samplers{};
        };

        std::vector<gpu_anim_key> get_default_keys(const gltf::model& mdl);

        gpu_animation create_animation(
            const gltf::model& mdl,
            const gltf::animation& animation,
            const std::vector<gpu_anim_key>& default_keys);

        std::vector<gpu_animation> get_animations(const gltf::model& mdl, size_t& anims_buffer_size);
        std::vector<glm::ivec4> get_input_nodes(const gltf::model& mdl);
        std::vector<glm::ivec4> get_execution_order(const gltf::model& mdl);
    };


    class animation_instance
    {
        friend class gpu_animation_controller;

        enum class playback_state
        {
            playing,
            paused
        };

    public:
        explicit animation_instance(const gltf::model& model);

        void update(uint64_t dt);
        void play(uint32_t animation_index, uint64_t start_position_us = 0, uint64_t end_position_us = -1, bool looped = false);
        void resume();
        void pause();
        void stop();

    private:
        const gltf::model& m_model;

        uint64_t m_start_position{0};
        uint64_t m_curr_position{0};
        uint64_t m_end_position{0};
        uint32_t m_current_animation{0};

        playback_state m_playback_state{playback_state::paused};

        bool m_looped{false};
    };


    class gpu_animation_controller
    {
    public:
        gpu_animation_controller(const gltf::model& model, size_t batch_size);
        void update(uint64_t dt);
        std::tuple<vk::Buffer, uint32_t, uint32_t> get_hierarchy_buffer();
        animation_instance* create_animation_instance(const gltf::model& model);

    private:
        vk_animations_list m_animations_list{};
        std::vector<animation_instance> m_anim_instances{};
        hal::render::avk::vma_buffer m_hierarchy_buffer{};
        size_t m_batch_size{};
        uint32_t m_hierarchy_buffer_size{};
    };


    struct vk_texture
    {
        vk::Image image;
        vk::ImageView image_view;
        vk::Sampler sampler;
    };


    class vk_texture_atlas
    {
    public:
        static vk_texture_atlas from_gltf_model(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family);

        vk_texture_atlas() = default;

        vk_texture_atlas(const vk_texture_atlas&) = delete;
        vk_texture_atlas& operator=(const vk_texture_atlas&) = delete;

        vk_texture_atlas(vk_texture_atlas&&) noexcept = default;
        vk_texture_atlas& operator=(vk_texture_atlas&&) noexcept = default;

        ~vk_texture_atlas() = default;

        const vk_texture& get_texture(uint32_t index) const;

    private:
        std::vector<hal::render::avk::vma_image> m_images{};
        std::vector<hal::render::avk::image_view> m_image_views{};
        std::vector<std::pair<size_t, size_t>> m_image_sizes{};

        std::vector<hal::render::avk::sampler> m_samplers{};
        std::vector<vk_texture> m_vk_textures{};

        std::vector<hal::render::avk::vma_buffer> m_staging_resources{};
    };
} // namespace sandbox::gltf
