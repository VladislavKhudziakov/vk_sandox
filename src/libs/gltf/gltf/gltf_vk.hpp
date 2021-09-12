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
            const std::vector<vk::VertexInputAttributeDescription>& get_vertex_attributes() const;
            const std::vector<vk::VertexInputBindingDescription>& get_vertex_bindings() const;

            void set_vertex_buffer_offset(uint64_t);
            uint64_t get_vertex_buffer_offset() const;

            void set_index_type(vk::IndexType);
            vk::IndexType get_index_type() const;

            void set_index_buffer_offset(uint64_t);
            uint64_t get_index_buffer_offset() const;

        private:
            std::vector<vk::VertexInputAttributeDescription> m_attributes{};
            std::vector<vk::VertexInputBindingDescription> m_bindings{};

            uint64_t m_vertex_buffer_offset{0};

            vk::IndexType m_index_type{vk::IndexType::eNoneKHR};
            uint64_t m_index_buffer_offset{0};
        };

        class texture : public gltf::texture
        {
        public:
            texture(const nlohmann::json& gltf_json, const nlohmann::json& texture_json);
            void set_vk_sampler(hal::render::avk::sampler);
            vk::Sampler get_vk_sampler() const;

        private:
            hal::render::avk::sampler m_vk_sampler{};
        };

        class image : public gltf::image
        {
            public:
                image(
                    const std::vector<std::unique_ptr<gltf::buffer>>& buffers,
                    const nlohmann::json& gltf_json,
                    const nlohmann::json& image_json);
                void set_vk_image(hal::render::avk::vma_image);
                void set_vk_image_view(hal::render::avk::image_view);

                vk::Image get_vk_image() const;
                vk::ImageView get_vk_image_view() const;

            private:
                hal::render::avk::vma_image m_vk_image{};
                hal::render::avk::image_view m_vk_image_view{};
        };


        class gltf_renderer
        {
        public:
            virtual ~gltf_renderer() = default;
            virtual void draw_scene(
                const gltf::model& model,
                uint32_t scene,
                const vk::Buffer& vertex_buffer,
                const vk::Buffer& index_buffer) = 0;
        };

        static gltf_vk from_url(const std::string& url);

        void create_resources(vk::CommandBuffer& command_buffer, uint32_t queue_family);
        void clear_staging_resources();

        void draw(gltf_renderer&);

        vk::Buffer get_vertex_buffer() const;
        vk::Buffer get_index_buffer() const;

    private:
        utils::data m_file_data{};
        gltf::model m_model;

        hal::render::avk::vma_buffer m_vertex_buffer{};
        size_t m_vertex_buffer_size{};

        hal::render::avk::vma_buffer m_index_buffer{};
        size_t m_index_buffer_size{};
    };
} // namespace sandbox::gltf
