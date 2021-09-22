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
        static vk_geometry from_gltf_model(const gltf::model& mdl, vk::CommandBuffer& command_buffer, uint32_t queue_family);

        vk_geometry() = default;

        vk_geometry(const vk_geometry&) = delete;
        vk_geometry& operator=(const vk_geometry&) = delete;

        vk_geometry(vk_geometry&&) noexcept = default;
        vk_geometry& operator=(vk_geometry&&) noexcept = default;

        ~vk_geometry() = default;

        template<typename Functional>
        void for_each_mesh_primitive(uint32_t mesh, Functional callback) const
        {
            uint32_t primitive_index = 0;

            for (const auto& p : m_primitives[mesh]) {
                callback(primitive_index, p);
                primitive_index++;
            }
        }

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
