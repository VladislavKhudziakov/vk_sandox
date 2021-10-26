#pragma once

#include <render/vk/raii.hpp>
#include <unordered_set>


namespace sandbox::hal::render::avk
{
    class image_pool;

    class image_instance
    {
        friend class image_pool;
        friend class image_builder;

    public:
        image_instance() = default;

        vk::Format get_format() const;
        uint32_t get_width() const;
        uint32_t get_height() const;
        uint32_t get_depth() const;
        uint32_t get_layers() const;
        uint32_t get_faces() const;
        uint32_t get_mips_levels() const;

        operator vk::Image() const;
        operator vk::ImageView() const;

        void upload(std::function<void(uint8_t*)> cb);

    private:
        image_instance(image_pool& pool);

        image_pool* m_pool{nullptr};
        uint32_t m_subresource_index{};

        vk::Format m_format{};

        uint32_t m_width{1};
        uint32_t m_height{1};
        uint32_t m_depth{1};
        uint32_t m_layers{1};
        uint32_t m_faces{1};
        uint32_t m_mips_levels{1};

        VkDeviceSize m_staging_offset{0};
    };


    class image_builder
    {
        friend class image_pool;

    public:
        image_builder& set_format(vk::Format);
        image_builder& set_width(size_t width);
        image_builder& set_height(size_t height);
        image_builder& set_depth(size_t depth);
        image_builder& set_layers(size_t layers);
        image_builder& set_faces(size_t faces);
        image_builder& set_mips_levels(size_t levels);
        image_builder& gen_mips(bool);

        image_instance create(std::function<void(uint8_t*)> cb = {});

    private:
        image_builder(image_pool&);

        image_pool& m_pool;

        uint32_t m_width{1};
        uint32_t m_height{1};
        uint32_t m_depth{1};
        uint32_t m_layers{1};
        uint32_t m_faces{1};
        uint32_t m_mips_levels{1};

        vk::Format m_format{vk::Format::eUndefined};

        bool m_gen_mips{false};
    };


    class image_pool
    {
    public:
        void add_image_instance(image_instance& instance, bool gen_mips, bool reserve_staging_space);
        void update_subresource(uint32_t subresource, std::function<void(uint8_t* dst)>);

        void flush(uint32_t queue_family, vk::CommandBuffer& command_buffer);
        void update(vk::CommandBuffer& command_buffer);

        vk::Image get_subresource_image(uint32_t) const;
        vk::ImageView get_subresource_image_view(uint32_t) const;

        image_builder get_builder();

    private:
        struct image_subresource
        {
            uint32_t width{1};
            uint32_t height{1};
            uint32_t depth{1};
            uint32_t layers{1};
            uint32_t faces{1};
            uint32_t levels{1};

            vk::Format format{};

            bool reserve_staging_space{false};
            bool gen_mips{false};

            avk::vma_image m_image{};
            avk::image_view m_image_view{};

            vk::ImageUsageFlags m_usage{};
            VkDeviceSize m_staging_offset{};
        };

        void update(vk::CommandBuffer& command_buffer, bool flush_staging_buffer);

        void gen_subresource_images(image_subresource& subres, uint32_t queue_family);
        void copy_subres_data(vk::CommandBuffer& command_buffer, const image_subresource& subres);
        void copy_subres_level(const image_subresource& subres, uint32_t level, vk::CommandBuffer& command_buffer, uint32_t& level_offset);
        void gen_subres_mips(vk::CommandBuffer& command_buffer, const image_subresource& subres, uint32_t level);
        VkDeviceSize get_subresource_level_size(const image_subresource& subres, uint32_t queue_family);

        void image_pipeline_barrier(
            vk::CommandBuffer& command_buffer,
            const image_subresource& subres,
            uint32_t subres_level,
            vk::PipelineStageFlags src_stage,
            vk::PipelineStageFlags dst_stage,
            vk::AccessFlags src_access,
            vk::AccessFlags dst_access,
            vk::ImageLayout old_layout,
            vk::ImageLayout new_layout);

        std::vector<image_subresource> m_subresources{};
        std::vector<std::function<void(uint8_t*)>> m_upload_callbacks{};

        std::unordered_set<uint32_t> m_subresources_to_update{};

        avk::vma_buffer m_staging_buffer{};
        VkDeviceSize m_staging_buffer_size{};
    };
} // namespace sandbox::hal::render::avk