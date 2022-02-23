
#include "buffer.hpp"

#include <render/vk/utils.hpp>

#include <utils/conditions_helpers.hpp>
#include <utils/scope_helpers.hpp>

using namespace sandbox::hal::render;


avk::buffer_instance::buffer_instance(avk::buffer_pool& pool)
    : m_pool(&pool)
{
}


void avk::buffer_instance::upload(std::function<void(uint8_t*)> cb)
{
    CHECK(is_updatable());

    m_pool->update_subresource(m_subresource_index, [offset = m_staging_buffer_offset, cb = std::move(cb)](uint8_t* dst) {
        cb(dst + offset);
    });
}


size_t avk::buffer_instance::get_size() const
{
    return m_size;
}


size_t avk::buffer_instance::get_offset() const
{
    return m_buffer_offset;
}


avk::buffer_instance::operator vk::Buffer() const
{
    return m_pool->get_buffer();
}


bool avk::buffer_instance::is_updatable() const
{
    return static_cast<bool>(m_usage & vk::BufferUsageFlagBits::eTransferDst);
}


avk::buffer_builder::buffer_builder(buffer_pool& pool)
    : m_pool(pool)
{
}


avk::buffer_builder& avk::buffer_builder::set_size(size_t size)
{
    m_size = size;
    return *this;
}


avk::buffer_builder& avk::buffer_builder::set_usage(vk::BufferUsageFlags usage)
{
    m_usage = usage;
    return *this;
}


avk::buffer_instance avk::buffer_builder::create(std::function<void(uint8_t*)> cb)
{
    buffer_instance result{m_pool};

    CHECK_MSG(m_size > 0, "Cannot create empty buffer.");
    CHECK_MSG(static_cast<uint32_t>(m_usage) != 0, "Buffer usage didn't specified.");

    result.m_size = m_size;
    result.m_usage = m_usage;

    m_pool.add_buffer_instance(&result);

    if (cb) {
        result.upload(std::move(cb));
    }

    return result;
}

void avk::buffer_pool::add_buffer_instance(buffer_instance* instance)
{
    if (instance->is_updatable()) {
        instance->m_staging_buffer_offset = m_staging_size;
        m_staging_size += instance->m_size;
    }

    auto alignment = avk::get_buffer_offset_alignment(instance->m_usage);

    if (alignment != 0) {
        m_size = get_aligned_size(m_size, alignment);
    }

    instance->m_buffer_offset = m_size;
    m_size += instance->m_size;

    instance->m_subresource_index = m_subresources.size();

    m_subresources.emplace_back(buffer_subresource{
        .size = instance->m_size,
        .offset = instance->m_buffer_offset,
        .staging_offset = instance->m_staging_buffer_offset,
        .usage = instance->m_usage});

    m_usage |= instance->m_usage;
}


void sandbox::hal::render::avk::buffer_pool::update_subresource(uint32_t subresource, std::function<void(uint8_t*)> cb)
{
    m_upload_callbacks.emplace_back(std::move(cb));
    m_subresources_to_update.emplace(subresource);
}


avk::submit_handler avk::buffer_pool::submit(vk::QueueFlagBits queue)
{

    const auto queue_family = avk::context::queue_family(queue);
    if (m_staging_size > 0) {
        m_staging_buffer = avk::gen_staging_buffer(queue_family, m_staging_size, [this](uint8_t* dst) {
            upload_staging_data(dst);
        });
    }

    m_resource = avk::create_vma_buffer(
        vk::BufferCreateInfo{
            .flags = {},
            .size = m_size,
            .usage = m_usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family},
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY});

    auto result = update_internal(queue, UPDATE_RESOURCE_BUFFER_BIT);
    m_queue_type = queue;
    m_upload_callbacks.clear();
    return result;
}


avk::submit_handler avk::buffer_pool::update()
{
    return update_internal(m_queue_type, UPDATE_STAGING_BUFFER_BIT | UPDATE_RESOURCE_BUFFER_BIT);
}


vk::Buffer avk::buffer_pool::get_buffer() const
{
    return m_resource.as<vk::Buffer>();
}


std::pair<vk::PipelineStageFlags, vk::AccessFlags>
avk::buffer_pool::get_pipeline_stages_acceses_by_usage(vk::BufferUsageFlags usage)
{
    vk::PipelineStageFlags dst_stage{};
    vk::AccessFlags dst_access{};

    if (usage & vk::BufferUsageFlagBits::eUniformBuffer || usage & vk::BufferUsageFlagBits::eStorageBuffer) {
        dst_stage |= vk::PipelineStageFlagBits::eComputeShader;
        dst_stage |= vk::PipelineStageFlagBits::eVertexShader;
        dst_stage |= vk::PipelineStageFlagBits::eTessellationControlShader;
        dst_stage |= vk::PipelineStageFlagBits::eTessellationEvaluationShader;
        dst_stage |= vk::PipelineStageFlagBits::eGeometryShader;
        dst_stage |= vk::PipelineStageFlagBits::eFragmentShader;

        if (usage & vk::BufferUsageFlagBits::eUniformBuffer) {
            dst_access |= vk::AccessFlagBits::eUniformRead;
        }

        if (usage & vk::BufferUsageFlagBits::eStorageBuffer) {
            dst_access |= vk::AccessFlagBits::eShaderRead;
        }
    }

    if (usage & vk::BufferUsageFlagBits::eIndexBuffer) {
        dst_stage |= vk::PipelineStageFlagBits::eVertexInput;
        dst_access |= vk::AccessFlagBits::eVertexAttributeRead;
    }

    if (usage & vk::BufferUsageFlagBits::eVertexBuffer) {
        dst_stage |= vk::PipelineStageFlagBits::eVertexInput;
        dst_access |= vk::AccessFlagBits::eIndexRead;
    }

    if (usage & vk::BufferUsageFlagBits::eIndirectBuffer) {
        dst_stage |= vk::PipelineStageFlagBits::eDrawIndirect;
    }

    return {dst_stage, dst_access};
}


avk::submit_handler avk::buffer_pool::update_internal(vk::QueueFlagBits queue, uint32_t update_state)
{
    if (update_state & UPDATE_STAGING_BUFFER_BIT) {
        if (!m_upload_callbacks.empty()) {
            void* dst_ptr{nullptr};
            auto res = vmaMapMemory(avk::context::allocator(), m_staging_buffer.as<VmaAllocation>(), &dst_ptr);

            utils::on_scope_exit scope_guard{[this, res]() {
                if (res == VK_SUCCESS) {
                    vmaUnmapMemory(avk::context::allocator(), m_staging_buffer.as<VmaAllocation>());
                    VkDeviceSize offset = 0;
                    VkDeviceSize size = m_staging_buffer->get_alloc_info().size;
                    VmaAllocation allocation = m_staging_buffer.as<VmaAllocation>();
                    vmaFlushAllocations(avk::context::allocator(), 1, &allocation, &offset, &size);
                }
            }};

            ASSERT(res == VK_SUCCESS);

            upload_staging_data(static_cast<uint8_t*>(dst_ptr));
        }
    }

    if (update_state & UPDATE_RESOURCE_BUFFER_BIT) {
        std::vector<vk::BufferCopy> buffer_copies{};
        std::vector<vk::BufferMemoryBarrier> buffer_barriers{};

        buffer_copies.reserve(m_subresources_to_update.size());
        buffer_barriers.reserve(m_subresources_to_update.size());

        vk::PipelineStageFlags dst_stages{};

        for (const auto subres_index : m_subresources_to_update) {
            const auto& subres = m_subresources[subres_index];

            auto [curr_dst_stage, dst_access] = get_pipeline_stages_acceses_by_usage(subres.usage);

            dst_stages |= curr_dst_stage;

            buffer_copies.emplace_back(vk::BufferCopy{
                .srcOffset = static_cast<VkDeviceSize>(subres.staging_offset),
                .dstOffset = static_cast<VkDeviceSize>(subres.offset),
                .size = static_cast<VkDeviceSize>(subres.size),
            });

            buffer_barriers.emplace_back(vk::BufferMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = dst_access,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = m_resource.as<vk::Buffer>(),
                .offset = static_cast<VkDeviceSize>(subres.offset),
                .size = static_cast<VkDeviceSize>(subres.size)});
        }

        if (!buffer_copies.empty()) {
            return avk::one_time_submit(queue, [&](vk::CommandBuffer& command_buffer) {
                command_buffer.copyBuffer(m_staging_buffer.as<vk::Buffer>(), m_resource.as<vk::Buffer>(), buffer_copies);
                command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, dst_stages, {}, {}, buffer_barriers, {});
            });
        }

        m_subresources_to_update.clear();

        return {};
    }
}

void sandbox::hal::render::avk::buffer_pool::upload_staging_data(uint8_t* dst)
{
    for (const auto& cp : m_upload_callbacks) {
        cp(static_cast<uint8_t*>(dst));
    }

    m_upload_callbacks.clear();
}

avk::buffer_builder sandbox::hal::render::avk::buffer_pool::get_builder()
{
    return buffer_builder(*this);
}