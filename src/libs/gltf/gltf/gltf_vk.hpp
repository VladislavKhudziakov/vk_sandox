#pragma once

#include <gltf/gltf_base.hpp>
#include <render/vk/raii.hpp>

#include <string>

namespace sandbox::gltf
{
    //    class vk_primitive
    //    {
    //    public:
    //        vk_primitive(
    //            const std::vector<vk::VertexInputAttributeDescription>& attributes,
    //            const std::vector<vk::VertexInputBindingDescription>& bindings,
    //            hal::render::avk::vma_buffer& vertex_buffer,
    //            uint64_t vertex_buffer_offset,
    //            vk::IndexType index_type,
    //            hal::render::avk::vma_buffer& index_buffer,
    //            uint64_t index_buffer_offset);
    //
    //        const std::vector<vk::VertexInputAttributeDescription>& get_vertex_attributes() const;
    //        const std::vector<vk::VertexInputBindingDescription>& get_vertex_bindings() const;
    //        vk::IndexType get_index_type() const;
    //
    //        vk::Buffer get_vertex_buffer() const;
    //        uint64_t get_vertex_buffer_offset() const;
    //        vk::Buffer get_index_buffer() const;
    //        uint64_t get_index_buffer_offset() const;
    //
    //    private:
    //        std::vector<vk::VertexInputAttributeDescription> m_attributes{};
    //        std::vector<vk::VertexInputBindingDescription> m_bindings{};
    //        hal::render::avk::vma_buffer& m_vertex_buffer;
    //        uint64_t m_vertex_buffer_offset{0};
    //        vk::IndexType m_index_type{vk::IndexType::eNoneKHR};
    //        hal::render::avk::vma_buffer& m_index_buffer;
    //        uint64_t m_index_buffer_offset{0};
    //    };
    //
    //    class vk_mesh : public mesh
    //    {
    //        friend class gltf_vk_assets_factory;
    //    public:
    //        ~vk_mesh() override = default;
    //        const std::vector<gltf::vk_primitive>& get_vk_primitives() const;
    //
    //    private:
    //        explicit vk_mesh(const nlohmann::json& json);
    //
    //        std::vector<vk_primitive> m_vk_primitives{};
    //    };

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


    //    class gltf_vk_assets_factory
    //    {
    //    public:
    //        using BufferT = buffer;
    //        using BufferViewT = buffer_view;
    //        using AccesssorT = accessor;
    //        using SceneT = scene;
    //        using NodeT = node;
    //        using MaterialT = material;
    //        using TextureT = texture;
    //        using ImageT = image;
    //        using SamplerT = sampler;
    //        using MeshT = vk_mesh;
    //        using CameraT = camera;
    //        using AnimationT = animation;
    //        using SkinT = skin;
    //
    //        MeshT create_mesh(const nlohmann::json& json);
    //
    //    private:
    //        vk_primitive create_primitive(const gltf::primitive&);
    //        void copy_accessor_data(const uint8_t* dst, size_t dst_offset, const accessor*);
    //
    //        std::vector<const accessor*> m_accessors{};
    //        hal::render::avk::vma_buffer m_vertex_buffer{};
    //        hal::render::avk::vma_buffer m_index_buffer{};
    //
    //        size_t m_vertex_buffer_size{0};
    //        size_t m_index_buffer_size{0};
    //
    //        std::vector<std::function<void(const uint8_t*)>> m_copy_vertices_data_callbacks{};
    //        std::vector<std::function<void(const uint8_t*)>> m_copy_indices_data_callbacks{};
    //    };

    //    class gltf_vk_model : public gltf::model<gltf_vk_assets_factory>
    //    {
    //    public:
    //        class primitive : public gltf::primitive
    //        {
    //        public:
    //            primitive(const nlohmann::json& gltf_json, const nlohmann::json& primitive_json);
    //            ~primitive() override = default;
    //
    //            void set_vertices_format(const std::vector<vk::VertexInputAttributeDescription>&);
    //            const std::vector<vk::VertexInputAttributeDescription>& get_vertex_attributes() const;
    //            const std::vector<vk::VertexInputBindingDescription>& get_vertex_bindings() const;
    //
    //            void set_vertex_buffer_offset(uint64_t);
    //            uint64_t get_vertex_buffer_offset() const;
    //
    //            void set_index_type(vk::IndexType);
    //            vk::IndexType get_index_type() const;
    //
    //            void set_index_buffer_offset(uint64_t);
    //            uint64_t get_index_buffer_offset() const;
    //
    //        private:
    //            std::vector<vk::VertexInputAttributeDescription> m_attributes{};
    //            std::vector<vk::VertexInputBindingDescription> m_bindings{};
    //
    //            uint64_t m_vertex_buffer_offset{0};
    //
    //            vk::IndexType m_index_type{vk::IndexType::eNoneKHR};
    //            uint64_t m_index_buffer_offset{0};
    //        };
    //
    //        class texture : public gltf::texture
    //        {
    //        public:
    //            texture(const nlohmann::json& gltf_json, const nlohmann::json& texture_json);
    //            void set_vk_sampler(hal::render::avk::sampler);
    //            vk::Sampler get_vk_sampler() const;
    //
    //        private:
    //            hal::render::avk::sampler m_vk_sampler{};
    //        };
    //
    //        class image : public gltf::image
    //        {
    //        public:
    //            image(
    //                const std::vector<std::unique_ptr<gltf::buffer>>& buffers,
    //                const nlohmann::json& gltf_json,
    //                const nlohmann::json& image_json);
    //            void set_vk_image(hal::render::avk::vma_image);
    //            void set_vk_image_view(hal::render::avk::image_view);
    //
    //            vk::Image get_vk_image() const;
    //            vk::ImageView get_vk_image_view() const;
    //
    //        private:
    //            hal::render::avk::vma_image m_vk_image{};
    //            hal::render::avk::image_view m_vk_image_view{};
    //        };
    //
    //        class animation_controller
    //        {
    //        public:
    //            virtual ~animation_controller() = default;
    //
    //            virtual vk::Buffer get_animation_keys() = 0;
    //            virtual void update(uint64_t dt_us) = 0;
    //            virtual void play() = 0;
    //            virtual void pause() = 0;
    //        };
    //
    //        static gltf_vk from_url(const std::string& url);
    //
    //        void create_resources(vk::CommandBuffer& command_buffer, uint32_t queue_family);
    //        void clear_staging_resources();
    //
    //        const gltf::model& get_model() const;
    //        vk::Buffer get_vertex_buffer() const;
    //        vk::Buffer get_index_buffer() const;
    //
    //    private:
    //        utils::data m_file_data{};
    //        gltf::model m_model;
    //
    //        hal::render::avk::vma_buffer m_vertex_buffer{};
    //        size_t m_vertex_buffer_size{};
    //
    //        hal::render::avk::vma_buffer m_index_buffer{};
    //        size_t m_index_buffer_size{};
    //    };
} // namespace sandbox::gltf
