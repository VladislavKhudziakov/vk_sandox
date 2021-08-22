#pragma once

#include <gltf/gltf_base.hpp>
#include <render/vk/raii.hpp>

#include <string>

namespace sandbox::gltf
{
    class gltf_vk
    {
    public:
        class primitive : public gltf::primitive
        {
        public:
            primitive(const nlohmann::json& gltf_json, const nlohmann::json& primitive_json);
            ~primitive() override = default;

            void set_vertices_format(const std::vector<vk::VertexInputAttributeDescription>&);
            const std::vector<vk::VertexInputAttributeDescription>& get_vertices_format() const;
            size_t get_vertices_format_stride() const;

            void set_vertex_buffer_offset(uint64_t);
            uint64_t get_vertex_buffer_offset() const;

            void set_index_type(vk::IndexType);
            vk::IndexType get_index_type() const;

            void set_index_buffer_offset(uint64_t);
            uint64_t get_index_buffer_offset() const;

        private:
            std::vector<vk::VertexInputAttributeDescription> m_attributes{};

            size_t m_vertices_format_stride{0};
            uint64_t m_vertex_buffer_offset{0};

            vk::IndexType m_index_type{vk::IndexType::eNoneKHR};
            uint64_t m_index_buffer_offset{0};
        };

        class primitive_renderer
        {
        public:
            virtual ~primitive_renderer() = default;
            virtual void draw(const sandbox::gltf::primitive&, const sandbox::gltf::material&, vk::CommandBuffer&) = 0;
        };

        static gltf_vk from_url(const std::string& url);

        void create_resources(vk::CommandBuffer& command_buffer, uint32_t queue_family);

        void draw(vk::CommandBuffer& command_buffer);

    private:
        void draw(
            const std::vector<int32_t>& curr_nodes,
            const std::vector<std::unique_ptr<node>>& all_nodes,
            const std::vector<std::unique_ptr<mesh>>& all_meshes,
            const std::vector<std::unique_ptr<material>>& all_materials,
            vk::CommandBuffer& command_buffer);

        gltf::model m_model;

        hal::render::avk::vma_buffer m_vertex_buffer{};
        size_t m_vertex_buffer_size{};

        hal::render::avk::vma_buffer m_index_buffer{};
        size_t m_index_buffer_size{};

        std::unique_ptr<primitive_renderer> m_primitive_renderer{};
    };
} // namespace sandbox::gltf
