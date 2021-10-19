#pragma once

#include <gltf/gltf_base.hpp>
#include <render/vk/resources.hpp>

#include <string>

namespace sandbox::gltf
{
    class vk_skin_
    {
        friend class vk_model_builder;
    public:
        const hal::render::avk::buffer_instance& get_joints_buffer() const;

        uint32_t get_joints_count() const;
        uint32_t get_hierarchy_size() const;

    private:
        hal::render::avk::buffer_instance m_joints_buffer{};
        uint32_t m_joints_count{};
        uint32_t m_hierarchy_size{};
    };
    

    class vk_primitive
    {
        friend class vk_model_builder;

    public:
        const hal::render::avk::buffer_instance& get_vertex_buffer() const;
        uint32_t get_vertices_count() const;

        const hal::render::avk::buffer_instance& get_index_buffer() const;
        vk::IndexType get_indices_type() const;
        uint32_t get_indices_count() const;

    private:
        hal::render::avk::buffer_instance m_vertex_buffer{};
        hal::render::avk::buffer_instance m_index_buffer{};

        uint32_t m_vertices_count{};
        uint32_t m_indices_count{};

        vk::IndexType m_index_type{vk::IndexType::eNoneKHR};

        std::pair<glm::vec3, glm::vec3> m_box_bound{};
    };
    

    class vk_mesh
    {
        friend class vk_model_builder;
    public:
        const std::vector<vk_primitive>& get_primitives() const;
        const vk_skin_& get_skin() const;
        bool is_skinned() const;

    private:
        std::vector<vk_primitive> m_primitives{};
        vk_skin_ m_skin{};
        bool m_skinned = false;

        std::pair<glm::vec3, glm::vec3> m_box_bound{};
    };
    

    class vk_model
    {
        friend class vk_model_builder;

    public:
        using attribute_description = vk::VertexInputAttributeDescription;
        using binding_description = vk::VertexInputBindingDescription;
        using vertex_format = std::tuple<const attribute_description*, uint32_t, const binding_description*, uint32_t>;

        const std::vector<vk_mesh>& get_meshes() const;
        vertex_format get_vertex_format(); 

    private:
        std::vector<vk::VertexInputAttributeDescription> m_attributes{};
        std::vector<vk::VertexInputBindingDescription> m_bindings{};

        std::vector<vk_mesh> m_meshes{};
    };


    class vk_model_builder
    {
    public:
        vk_model_builder() = default;
        vk_model_builder& set_vertex_format(const std::array<vk::Format, 8>&);
        vk_model_builder& use_skin(bool use_skin);

        vk_model create(const gltf::model& mdl, hal::render::avk::buffer_pool& pool);

    private:
        void create_geometry(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool);
        void create_skins(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool);
        
        void create_skins_resources(
          const gltf::model& mdl, 
          std::vector<vk_skin_>& result, 
          vk_model& model, 
          hal::render::avk::buffer_pool& pool);

        void assing_skins_to_meshes(const std::vector<vk_skin_>& skins, const gltf::model& mdl, vk_model& model);

        void get_vertex_attributes_data_from_fixed_format(
          std::vector<vk::VertexInputAttributeDescription>& out_attributres,
          std::vector<vk::VertexInputBindingDescription>& out_bindings,
          uint32_t& out_vertex_size);

        void copy_attribute_data(
            const gltf::primitive::vertex_attribute& attribute,
            vk::Format desired_vk_format,
            uint64_t vtx_size,
            uint64_t offset,
            uint8_t* dst);

        std::optional<std::array<vk::Format, 8>> m_fixed_format{};
        bool m_skinned = true;
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
        std::tuple<vk::Buffer, uint32_t, uint32_t> get_anim_meta_buffer() const;
        std::tuple<vk::Buffer, uint32_t, uint32_t> get_nodes_buffer() const;
        std::tuple<vk::Buffer, uint32_t, uint32_t> get_exec_order_buffer() const;
        uint32_t get_nodes_count() const;

    private:
        std::vector<vk_animation> m_animations_list{};

        hal::render::avk::vma_buffer m_anim_meta_staging_buffer{};
        hal::render::avk::vma_buffer m_anim_meta_buffer{};
        std::pair<uint32_t, uint32_t> m_anim_meta_sub_buffer{};

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
            glm::vec4 translation{0, 0, 0, 0};
            glm::vec4 rotation{0, 0, 0, 1};
            glm::vec4 scale{1, 1, 1, 1};
        };

        static_assert(sizeof(gpu_trs) == sizeof(float) * 4 * 3);


        struct gpu_anim_sampler
        {
            float ts = 0;
            std::vector<gpu_trs> nodes_keys{};
        };

        struct gpu_animation
        {
            uint64_t offset{0};
            uint64_t size{0};
            glm::ivec4 interpolation_avg_frame_duration{-1, -1, -1, 0};
            std::vector<gpu_anim_sampler> samplers{};
        };

        std::vector<gpu_trs> get_default_keys(const gltf::model& mdl);

        gpu_animation create_animation(
            const gltf::model& mdl,
            const gltf::animation& animation,
            const std::vector<gpu_trs>& default_keys);

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
