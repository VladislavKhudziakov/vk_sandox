#pragma once

#include <gltf/gltf_base.hpp>
#include <render/vk/raii.hpp>

#include <string>

namespace sandbox::gltf
{
    class gltf_vk
    {
    public:
        class primitive
        {
            friend gltf_vk;
        public:
            ~primitive() = default;

            const std::vector<vk::VertexInputAttributeDescription>& get_attributes() const;

            size_t get_vertices_format_stride() const;
            uint64_t get_vertex_buffer_offset() const;
            vk::IndexType get_index_type() const;
            uint64_t get_index_buffer_offset() const;

        private:
            primitive() = default;

            std::vector<vk::VertexInputAttributeDescription> m_attributes{};

            size_t m_vertices_format_stride{0};
            uint64_t m_vertex_buffer_offset{0};

            vk::IndexType m_index_type{vk::IndexType::eNoneKHR};
            uint64_t m_index_buffer_offset{0};
        };

        static gltf_vk from_url(const std::string& url);

        void create_resources(vk::CommandBuffer& command_buffer, uint32_t queue_family);

    private:
        void parse_meshes(vk::CommandBuffer& command_buffer, uint32_t queue_family);

        gltf::model m_model;

        hal::render::avk::vma_buffer m_vertex_buffer{};
        size_t m_vertex_buffer_size{};

        hal::render::avk::vma_buffer m_index_buffer{};
        size_t m_index_buffer_size{};
    };
} // namespace sandbox::gltf
