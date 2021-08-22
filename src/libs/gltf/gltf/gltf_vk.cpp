

#include "gltf_vk.hpp"

#include <gltf/glb_parser.hpp>

#include <render/vk/utils.hpp>

#include <utils/scope_helpers.hpp>

#include <filesystem/common_file.hpp>

#include <filesystem>

using namespace sandbox;
using namespace sandbox::hal::render;

namespace
{
    avk::vma_buffer create_staging_buffer(uint32_t queue_family, size_t data_size, const std::function<void(uint8_t*)>& on_buffer_mapped = {})
    {
        auto buffer = avk::create_vma_buffer(
            vk::BufferCreateInfo{
                .size = data_size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &queue_family,
            },
            VmaAllocationCreateInfo{
                .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            });

        if (!on_buffer_mapped) {
            return buffer;
        }

        {
            utils::on_scope_exit exit([&buffer]() {
                vmaUnmapMemory(avk::context::allocator(), buffer.as<VmaAllocation>());
            });

            void* mapped_data{nullptr};
            VK_C_CALL(vmaMapMemory(avk::context::allocator(), buffer.as<VmaAllocation>(), &mapped_data));
            assert(mapped_data != nullptr);
            on_buffer_mapped(reinterpret_cast<uint8_t*>(mapped_data));
        }

        return buffer;
    }
} // namespace


gltf::gltf_vk gltf::gltf_vk::from_url(const std::string& url)
{
    gltf::gltf_vk result{};

    hal::filesystem::common_file file{};
    file.open(url);

    if (std::filesystem::path(url).extension() == ".gltf") {
        const auto file_data = file.read_all();
        const auto gltf_json = nlohmann::json::parse(file_data.get_data(), file_data.get_data() + file_data.get_size());
        result.m_model = gltf::model(gltf_json);
    } else {
        auto glb_data = parse_glb_file(file.read_all());
        const auto gltf_json = nlohmann::json::parse(glb_data.json.data, glb_data.json.data + glb_data.json.size);
        result.m_model = gltf::model(gltf_json, utils::data::create_non_owning(const_cast<uint8_t*>(glb_data.bin.data), glb_data.bin.size));
    }

    return result;
}


void gltf::gltf_vk::create_resources(vk::CommandBuffer& command_buffer, uint32_t queue_family)
{
    parse_meshes(command_buffer, queue_family);
}


void gltf::gltf_vk::parse_meshes(vk::CommandBuffer& command_buffer, uint32_t queue_family)
{
    auto get_buffer_data = [this](const auto& meta_src) {
        const auto& buffer = m_model.get_buffers()[meta_src.buffer];
        const auto* src = buffer.get_data().get_data();
        const auto src_offset = meta_src.buffer_offset;
        const auto src_size = meta_src.buffer_length;

        return std::make_tuple(src, src_offset, src_size);
    };

    auto get_copy_data_callback = [&get_buffer_data](const auto& meta_src, uint64_t dst_offset) {
        return [&meta_src, &get_buffer_data, dst_offset](uint8_t* dst) {
            auto [src, src_offset, src_size] = get_buffer_data(meta_src);
            std::memcpy(dst + dst_offset, src + src_offset, src_size);
        };
    };

    auto get_on_buffer_mapped_callback = [](const std::vector<std::function<void(uint8_t*)>>& callbacks_list) {
        return [&callbacks_list](uint8_t* dst) {
            for (auto& callback : callbacks_list) {
                callback(dst);
            }
        };
    };

    std::vector<std::function<void(uint8_t*)>> copy_vertices_data_callbacks{};
    std::vector<std::function<void(uint8_t*)>> copy_indices_data_callbacks{};

    uint64_t vertex_buffer_size{0};
    uint64_t index_buffer_size{0};

    for (const auto& mesh : m_model.get_meshes()) {
        for (const auto& primitive : mesh.get_primitives()) {
            uint32_t vertex_attr_offset{0};
            uint32_t attr_location{0};
            uint32_t vertex_attr_stride{0};

            // TODO
            gltf_vk::primitive vk_primitive{};
            vk_primitive.m_vertex_buffer_offset = vertex_attr_offset;
            vk_primitive.m_attributes.reserve(primitive.get_attributes_data().size());

            for (const auto& attribute : primitive.get_attributes_data()) {
                copy_vertices_data_callbacks.emplace_back(get_copy_data_callback(attribute, vertex_buffer_size));
                vertex_buffer_size += attribute.buffer_length;

                vk_primitive.m_attributes.emplace_back() = {
                    .location = attr_location++,
                    .binding = 0,
                    .format = to_vk_format(attribute.accessor_type, attribute.component_type),
                    .offset = vertex_attr_offset,
                };

                vertex_attr_offset += attribute.buffer_length;
                vertex_attr_stride += avk::get_format_info(static_cast<VkFormat>(vk_primitive.m_attributes.back().format)).size;
            }

            vk_primitive.m_vertices_format_stride = vertex_attr_stride;

            const auto& indices = primitive.get_indices_data();

            if (indices.buffer >= 0) {
                vk_primitive.m_index_buffer_offset = index_buffer_size;
                vk_primitive.m_index_type = to_vk_index_type(indices.accessor_type, indices.component_type);

                copy_indices_data_callbacks.emplace_back(get_copy_data_callback(indices, index_buffer_size));
                index_buffer_size += indices.buffer_length;
            }
        }
    }


    // TODO
    auto vertex_staging_buffer = create_staging_buffer(
        queue_family, vertex_buffer_size, get_on_buffer_mapped_callback(copy_vertices_data_callbacks));

    m_vertex_buffer = avk::create_vma_buffer(
        vk::BufferCreateInfo{
            .flags = {},
            .size = vertex_buffer_size,
            .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        });

    command_buffer.copyBuffer(
        vertex_staging_buffer.as<vk::Buffer>(),
        m_vertex_buffer.as<vk::Buffer>(),
        {vk::BufferCopy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = vertex_buffer_size,
        }});

    if (index_buffer_size > 0) {
        auto index_staging_buffer = create_staging_buffer(
            queue_family, vertex_buffer_size, get_on_buffer_mapped_callback(copy_indices_data_callbacks));

        m_index_buffer = avk::create_vma_buffer(
            vk::BufferCreateInfo{
                .flags = {},
                .size = vertex_buffer_size,
                .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                .sharingMode = vk::SharingMode::eExclusive,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &queue_family
            },
            VmaAllocationCreateInfo{
                .usage = VMA_MEMORY_USAGE_GPU_ONLY
            });

        command_buffer.copyBuffer(
            index_staging_buffer.as<vk::Buffer>(),
            m_index_buffer.as<vk::Buffer>(),
            {vk::BufferCopy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size,
                }});
    }

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eVertexInput,
        {},
        {vk::MemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eIndexRead,
        }},
        {},
        {});
}


const std::vector<vk::VertexInputAttributeDescription>& gltf::gltf_vk::primitive::get_attributes() const
{
    return m_attributes;
}


size_t gltf::gltf_vk::primitive::get_vertices_format_stride() const
{
    return m_vertices_format_stride;
}


uint64_t gltf::gltf_vk::primitive::get_vertex_buffer_offset() const
{
    return m_vertex_buffer_offset;
}


vk::IndexType gltf::gltf_vk::primitive::get_index_type() const
{
    return m_index_type;
}


uint64_t gltf::gltf_vk::primitive::get_index_buffer_offset() const
{
    return m_index_buffer_offset;
}
