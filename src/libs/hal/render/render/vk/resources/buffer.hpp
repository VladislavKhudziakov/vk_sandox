#pragma once

#include <render/vk/utils.hpp>

#include <unordered_set>

namespace sandbox::hal::render::avk
{
    class buffer_pool;

    class buffer_instance
    {
        friend class buffer_builder;
        friend class buffer_pool;

    public:
        buffer_instance() = default;

        void upload(std::function<void(uint8_t*)>);
        size_t get_size() const;
        size_t get_offset() const;
        operator vk::Buffer() const;

        bool is_updatable() const;

    private:
        explicit buffer_instance(buffer_pool& pool);

        vk::BufferUsageFlags m_usage{};
        uint32_t m_subresource_index{};

        buffer_pool* m_pool{nullptr};

        size_t m_size{};
        size_t m_buffer_offset{};
        size_t m_staging_buffer_offset{size_t(-1)};
    };


    class buffer_builder
    {
    public:
        explicit buffer_builder(buffer_pool& pool);

        buffer_builder& set_size(size_t);
        buffer_builder& set_usage(vk::BufferUsageFlags);
        buffer_instance create(std::function<void(uint8_t*)> = {});

    private:
        buffer_pool& m_pool;

        size_t m_size{};
        vk::BufferUsageFlags m_usage{};
    };


    class buffer_pool
    {
    public:
        void add_buffer_instance(buffer_instance* instance);
        void update_subresource(uint32_t subresource, std::function<void(uint8_t*)> cb);

        avk::submit_handler submit(vk::QueueFlagBits queue);
        avk::submit_handler update();

        buffer_builder get_builder();
        vk::Buffer get_buffer() const;

    private:
        enum update_state_flags
        {
            UPDATE_NONE = 0,
            UPDATE_STAGING_BUFFER_BIT = 1 << 0,
            UPDATE_RESOURCE_BUFFER_BIT = 1 << 1,
        };

        struct buffer_subresource
        {
            size_t size{0};
            size_t offset{0};
            size_t staging_offset{0};
            vk::BufferUsageFlags usage{};
        };

        static std::pair<vk::PipelineStageFlags, vk::AccessFlags> get_pipeline_stages_acceses_by_usage(vk::BufferUsageFlags usage);

        avk::submit_handler update_internal(vk::QueueFlagBits queue, uint32_t update_state);
        void upload_staging_data(uint8_t* dst);

        uint32_t m_queue_family{};

        size_t m_size{};
        size_t m_staging_size{};
        std::vector<std::function<void(uint8_t*)>> m_upload_callbacks{};
        std::vector<buffer_subresource> m_subresources{};
        std::unordered_set<uint32_t> m_subresources_to_update{};

        vk::BufferUsageFlags m_usage{};
        avk::vma_buffer m_resource{};
        avk::vma_buffer m_staging_buffer{};

        vk::QueueFlagBits m_queue_type{};
    };
} // namespace sandbox::hal::render::avk