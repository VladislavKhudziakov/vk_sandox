#pragma once

#include <gltf/gltf_base.hpp>
#include <render/vk/resources.hpp>

#include <string>

namespace sandbox::gltf
{
    class vk_model;

    class vk_texture
    {
        friend class vk_model_builder;

    public:
        const hal::render::avk::image_instance& get_image() const;
        const hal::render::avk::sampler_instance& get_sampler() const;

    private:
        hal::render::avk::image_instance m_image{};
        hal::render::avk::sampler_instance m_sampler{};
    };


    class vk_material
    {
        friend class vk_model_builder;

    public:
        const vk_texture& get_base_color(const vk_model&) const;
        const vk_texture& get_normal(const vk_model&) const;
        const vk_texture& get_metallic_roughness(const vk_model&) const;
        const vk_texture& get_occlusion(const vk_model&) const;
        const vk_texture& get_emissive(const vk_model&) const;

    private:
        hal::render::avk::buffer_instance m_material_info_buffer{};
        uint32_t m_base_color{};
        uint32_t m_normal{};
        uint32_t m_metallic_roughness{};
        uint32_t m_occlusion{};
        uint32_t m_emissive{};
    };


    class vk_animation
    {
        friend class vk_model_builder;

    public:
        const hal::render::avk::buffer_instance& get_meta_buffer() const;
        const hal::render::avk::buffer_instance& get_time_stamps_buffer() const;
        const hal::render::avk::buffer_instance& get_keys_buffer() const;
        const hal::render::avk::buffer_instance& get_nodes_buffer() const;
        const hal::render::avk::buffer_instance& get_exec_order_buffer() const;

    private:
        hal::render::avk::buffer_instance m_meta_buffer{};
        hal::render::avk::buffer_instance m_time_stamps_buffer{};
        hal::render::avk::buffer_instance m_keys_buffer{};
        hal::render::avk::buffer_instance m_nodes_buffer{};
        hal::render::avk::buffer_instance m_exec_order_buffer{};
    };


    class vk_skin
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

        const vk_material& get_material(const vk_model&) const;

    private:
        uint32_t m_material{};

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
        const vk_skin& get_skin() const;
        bool is_skinned() const;

    private:
        std::vector<vk_primitive> m_primitives{};
        vk_skin m_skin{};
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
        const std::vector<vk_animation>& get_animations() const;
        const std::vector<vk_material>& get_materials() const;
        const std::vector<vk_texture>& get_textures() const;

        vertex_format get_vertex_format();

    private:
        std::vector<vk::VertexInputAttributeDescription> m_attributes{};
        std::vector<vk::VertexInputBindingDescription> m_bindings{};

        std::vector<vk_mesh> m_meshes{};
        std::vector<vk_animation> m_animations{};
        std::vector<vk_texture> m_textures{};
        std::vector<vk_material> m_materials{};
    };


    class vk_model_builder
    {
    public:
        static vk_model load_from_file(
          const std::string&, 
          hal::render::avk::buffer_pool& buffer_pool, 
          hal::render::avk::image_pool& image_pool);

        vk_model_builder() = default;
        vk_model_builder& set_vertex_format(const std::array<vk::Format, 8>&);
        vk_model_builder& use_skin(bool use_skin);

        vk_model create(
            const gltf::model& mdl,
            hal::render::avk::buffer_pool& buffer_pool,
            hal::render::avk::image_pool& image_pool);

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
            glm::ivec4 interpolation_avg_frame_duration{-1, -1, -1, 0};
            std::vector<glm::vec4> time_stamps{};
            std::vector<gpu_trs> keys{};
        };

        struct stb_pixel_data
        {
            size_t width;
            size_t height;
            vk::Format format{};
            std::vector<uint8_t> pixels;
        };

        void create_geometry(
            const gltf::model& mdl,
            vk_model& model,
            hal::render::avk::buffer_pool& buffer_pool);

        void create_skins(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool);
        void create_skins_resources(
            const gltf::model& mdl,
            std::vector<vk_skin>& result,
            vk_model& model,
            hal::render::avk::buffer_pool& pool);
        void assing_skins_to_meshes(const std::vector<vk_skin>& skins, const gltf::model& mdl, vk_model& model);

        void create_animations(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool);

        std::vector<gpu_trs> get_default_keys(const gltf::model& mdl);
        gpu_animation create_gpu_animation(const model& mdl, const animation& curr_animation, const std::vector<gpu_trs>& default_keys);
        void create_anim_keys_buffers(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool);

        void create_anim_nodes_buffer(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool);
        void create_anim_exec_order_buffer(const gltf::model& mdl, vk_model& model, hal::render::avk::buffer_pool& pool);

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

        void create_textures(
            const gltf::model& mdl,
            hal::render::avk::image_pool& pool,
            vk_model& result);

        void create_materials(
            const gltf::model& mdl,
            hal::render::avk::buffer_pool& buffer_pool,
            hal::render::avk::image_pool& image_pool,
            vk_model& result);

        stb_pixel_data get_stb_pixel_data(const gltf::model& mdl, const gltf::image& image);

        uint32_t gen_texture_from_vec(
            glm::vec4 glm_data,
            std::vector<vk_texture>& textures,
            hal::render::avk::image_pool& pool);

        std::optional<std::array<vk::Format, 8>> m_fixed_format{};
        bool m_skinned = true;
    };


    class animation_instance
    {
        friend class animation_controller;

        enum class playback_state
        {
            playing,
            paused
        };

    public:
        explicit animation_instance(const gltf::model& model);

        void set_animation(uint64_t uint32_t);

        void set_start_positon(uint64_t pos_us);
        void set_end_positon(uint64_t pos_us);
        void set_current_position(uint64_t pos_us);

        void play();
        void pause();
        void resume();
        void stop();

    private:
        void update(uint64_t dt);

        const gltf::model& m_model;

        uint64_t m_start_position{0};
        uint64_t m_curr_position{0};
        uint64_t m_end_position{0};
        uint32_t m_current_animation{0};

        playback_state m_playback_state{playback_state::paused};
    };


    class animation_controller
    {
    public:
        animation_controller() = default;

        animation_controller(
            const gltf::model& gltf_model,
            const gltf::vk_model& vk_model);

        void init_resources(hal::render::avk::buffer_pool& pool, size_t instances_count);
        void init_pipelines();

        void update(uint64_t dt);
        void update(vk::CommandBuffer& command_buffer);

        const std::vector<hal::render::avk::buffer_instance>& get_hierarchies() const;
        const std::vector<hal::render::avk::buffer_instance>& get_progressions() const;

        animation_instance* instantiate_animation();

    private:
        const gltf::model* m_gltf_model{nullptr};
        const gltf::vk_model* m_vk_model{nullptr};

        size_t m_batch_size{};

        hal::render::avk::pipeline_instance m_pipeline{};
        hal::render::avk::shader_module m_shader{};

        std::vector<hal::render::avk::buffer_instance> m_hierarchies{};
        std::vector<hal::render::avk::buffer_instance> m_progressions{};
        std::vector<animation_instance> m_animation_instances{};
    };
} // namespace sandbox::gltf
