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
    public:
        static void create_meshes_data(
            const gltf::model& mdl,
            std::vector<std::vector<vk_primitive>>& primitives,
            hal::render::avk::vma_buffer& result_v_staging_buffer,
            hal::render::avk::vma_buffer& result_v_dst_buffer,
            hal::render::avk::vma_buffer& result_i_staging_buffer,
            hal::render::avk::vma_buffer& result_i_dst_buffer,
            vk::CommandBuffer& command_buffer,
            uint32_t queue_family);

        static vk_geometry from_gltf_model(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family);

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
        struct bone_data
        {
            glm::mat4 inv_bind_pose{1};
            alignas(sizeof(float) * 4) uint32_t joint{std::numeric_limits<uint32_t>::max()};
        };

        static_assert(sizeof(bone_data) == sizeof(float) * 4 * 4 + sizeof(float) * 4);

        std::vector<std::vector<vk_primitive>> m_primitives{};

        hal::render::avk::vma_buffer m_vertex_buffer{};
        hal::render::avk::vma_buffer m_index_buffer{};

        hal::render::avk::vma_buffer m_vertex_staging_buffer{};
        hal::render::avk::vma_buffer m_index_staging_buffer{};
    };

    struct vk_skin
    {
        uint64_t offset{0};
        uint64_t size{0};
    };


    class vk_geometry_skins
    {
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
        struct bone_data
        {
            glm::mat4 inv_bind_pose{1};
            alignas(sizeof(float) * 4) uint32_t joint{std::numeric_limits<uint32_t>::max()};
        };

        static_assert(sizeof(bone_data) == sizeof(float) * 4 * 4 + sizeof(float) * 4);

        std::vector<vk_skin> m_skins{};
        std::vector<glm::mat4> m_default_hierarchy_transforms{};

        hal::render::avk::vma_buffer m_skin_buffer{};
        hal::render::avk::vma_buffer m_hierarchy_buffer{};

        hal::render::avk::vma_buffer m_skin_staging_buffer{};
        hal::render::avk::vma_buffer m_hierarchy_staging_buffer{};
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

    //    class vk_pipeline
    //    {
    //    public:
    //        vk_pipeline(const vk_pipeline&) = delete;
    //        vk_pipeline& operator=(const vk_pipeline&) = delete;
    //        vk_pipeline(vk_pipeline&&) noexcept = default;
    //        vk_pipeline& operator=(vk_pipeline&&) noexcept = default;
    //
    //        void bind(vk::CommandBuffer& command_buffer, const std::vector<VkDeviceSize>& dyn_offsets);
    //
    //        vk::Pipeline get_pipeline() const;
    //        vk::DescriptorPool get_descriptor_pool() const;
    //        vk::PipelineLayout get_pipeline_layout() const;
    //        const vk::DescriptorSet* get_descriptor_sets(uint32_t&) const;
    //
    //    private:
    //        vk_pipeline() = default;
    //
    //        friend class vk_pipeline_builder;
    //
    //        hal::render::avk::descriptor_pool m_descriptor_pool{};
    //        hal::render::avk::descriptor_set_list m_descriptor_set{};
    //        hal::render::avk::pipeline_layout m_layouts{};
    //        hal::render::avk::graphics_pipeline m_pipeline_handler{};
    //        std::vector<VkDeviceSize> m_dyn_offsets{};
    //    };
    //
    //
    //    class vk_pipeline_builder
    //    {
    //    public:
    //        vk_pipeline_builder(
    //            const vk_primitive& primitive,
    //            vk::RenderPass pass,
    //            uint32_t subpass);
    //
    //        vk_pipeline_builder& set_shaders(const std::vector<vk::ShaderModule>&);
    //        vk_pipeline_builder& use_hierarchy(bool);
    //        vk_pipeline_builder& use_skinning(bool);
    //        vk_pipeline_builder& use_back_faces(bool);
    //        vk_pipeline_builder& enable_z_write(bool);
    //        vk_pipeline_builder& enable_z_test(bool);
    //        vk_pipeline_builder& enable_color_write(bool);
    //        vk_pipeline_builder& set_polygon_mode(vk::PolygonMode);
    //        vk_pipeline_builder& set_samples_count(vk::SampleCountFlagBits);
    //        vk_pipeline_builder& add_textures_descriptors(const gltf::material& material, const vk_texture_atlas&);
    //        vk_pipeline_builder& add_textures_descriptors(const std::vector<vk::ImageView>&);
    //        vk_pipeline_builder& add_buffer_descriptors(const gltf::node&, const vk_geometry&);
    //        vk_pipeline_builder& add_constant_data(uint8_t* data, size_t size);
    //
    //        vk_pipeline_builder& add_buffer_descriptors(
    //            const std::vector<vk::Buffer>&,
    //            const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>&,
    //            vk::DescriptorType);
    //
    //        vk_pipeline generate_pipeline();
    //    private:
    //        vk::RenderPass m_pass;
    //        uint32_t m_subpass;
    //    };
} // namespace sandbox::gltf
