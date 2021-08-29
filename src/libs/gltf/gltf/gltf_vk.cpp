

#include "gltf_vk.hpp"

#include <gltf/glb_parser.hpp>

#include <render/vk/errors_handling.hpp>
#include <render/vk/utils.hpp>
#include <utils/scope_helpers.hpp>

#include <filesystem/common_file.hpp>

#include <filesystem>

using namespace sandbox;
using namespace sandbox::hal::render;
#include <iostream>

namespace
{
    class vk_assets_factory : public gltf::assets_factory
    {
    public:
        ~vk_assets_factory() override = default;

        sandbox::hal::render::avk::vma_buffer create_vertex_buffer(
            const std::vector<std::unique_ptr<gltf::buffer>>& gltf_buffers,
            vk::CommandBuffer& command_buffer,
            uint32_t queue_family)
        {
            return create_buffer(
                command_buffer,
                queue_family,
                m_vertex_buffer_size,
                vk::BufferUsageFlagBits::eVertexBuffer,
                vk::AccessFlagBits::eVertexAttributeRead,
                gltf_buffers,
                m_copy_vertices_data_callbacks);
        }


        sandbox::hal::render::avk::vma_buffer create_index_buffer(
            const std::vector<std::unique_ptr<gltf::buffer>>& gltf_buffers,
            vk::CommandBuffer& command_buffer,
            uint32_t queue_family)
        {
            if (m_index_buffer_size == 0) {
                return {};
            }

            return create_buffer(
                command_buffer,
                queue_family,
                m_index_buffer_size,
                vk::BufferUsageFlagBits::eIndexBuffer,
                vk::AccessFlagBits::eIndexRead,
                gltf_buffers,
                m_copy_indices_data_callbacks);
        }

        size_t get_vertex_buffer_size() const
        {
            return m_vertex_buffer_size;
        }


        size_t get_index_buffer_size() const
        {
            return m_index_buffer_size;
        }


        void clear_staging_resources()
        {
            m_staging_buffers.clear();
        }


        std::unique_ptr<gltf::primitive> create_primitive(
            const nlohmann::json& gltf_json,
            const nlohmann::json& primitive_json) override
        {
            auto get_buffer_data = [](const auto& meta_src, const std::vector<std::unique_ptr<gltf::buffer>>& buffers) {
                const auto& buffer = buffers[meta_src.buffer];
                const auto* src = buffer->get_data().get_data();
                const auto src_offset = meta_src.buffer_offset;
                const auto src_size = meta_src.buffer_length;

                return std::make_tuple(src, src_offset, src_size);
            };

            auto get_copy_data_callback = [get_buffer_data](const auto& meta_src, uint64_t dst_offset) {
                return [&meta_src, get_buffer_data, dst_offset](const std::vector<std::unique_ptr<gltf::buffer>>& buffers, uint8_t* dst) {
                    auto [src, src_offset, src_size] = get_buffer_data(meta_src, buffers);
                    std::memcpy(dst + dst_offset, src + src_offset, src_size);
                };
            };

            uint32_t vertex_attr_offset{0};
            uint32_t attr_location{0};

            auto vk_primitive = std::make_unique<gltf::gltf_vk::primitive>(gltf_json, primitive_json);

            std::vector<vk::VertexInputAttributeDescription> vertex_attributes{};

            vertex_attributes.reserve(vk_primitive->get_attributes_data().size());

            for (const auto& attribute : vk_primitive->get_attributes_data()) {
                m_copy_vertices_data_callbacks.emplace_back(get_copy_data_callback(attribute, m_vertex_buffer_size));
                m_vertex_buffer_size += attribute.buffer_length;

                vertex_attributes.emplace_back() = {
                    .location = attr_location++,
                    .binding = 0,
                    .format = to_vk_format(attribute.accessor_type, attribute.component_type),
                    .offset = vertex_attr_offset,
                };

                vertex_attr_offset += attribute.buffer_length;
            }

            vk_primitive->set_vertices_format(vertex_attributes);

            const auto& indices = vk_primitive->get_indices_data();

            if (indices.buffer >= 0) {
                vk_primitive->set_vertex_buffer_offset(m_index_buffer_size);
                vk_primitive->set_index_type(to_vk_index_type(indices.accessor_type, indices.component_type));

                m_copy_indices_data_callbacks.emplace_back(get_copy_data_callback(indices, m_index_buffer_size));
                m_index_buffer_size += indices.buffer_length;
            }

            return vk_primitive;
        }

    private:
        avk::vma_buffer create_buffer(
            vk::CommandBuffer& command_buffer,
            uint32_t queue_family,
            uint32_t buffer_size,
            vk::BufferUsageFlagBits buffer_usage,
            vk::AccessFlagBits access_flags,
            const std::vector<std::unique_ptr<gltf::buffer>>& buffers,
            const std::vector<std::function<void(const std::vector<std::unique_ptr<gltf::buffer>>&, uint8_t*)>>& copy_callbacks)
        {
            auto staging_buffer = avk::create_staging_buffer(
                queue_family,
                buffer_size,
                [&copy_callbacks, &buffers](uint8_t* dst) {
                    for (auto& callback : copy_callbacks) {
                        callback(buffers, dst);
                    }
                });

            auto result_buffer = avk::create_vma_buffer(
                vk::BufferCreateInfo{
                    .flags = {},
                    .size = buffer_size,
                    .usage = vk::BufferUsageFlagBits::eTransferDst | buffer_usage,
                    .sharingMode = vk::SharingMode::eExclusive,
                    .queueFamilyIndexCount = 1,
                    .pQueueFamilyIndices = &queue_family},
                VmaAllocationCreateInfo{
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY});

            command_buffer.copyBuffer(
                staging_buffer.as<vk::Buffer>(),
                result_buffer.as<vk::Buffer>(),
                {vk::BufferCopy{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = buffer_size,
                }});

            command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eVertexInput,
                {},
                {vk::MemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = access_flags,
                }},
                {},
                {});

            m_staging_buffers.emplace_back(std::move(staging_buffer));

            return result_buffer;
        }

        uint64_t m_vertex_buffer_size{0};
        uint64_t m_index_buffer_size{0};

        std::vector<std::function<void(const std::vector<std::unique_ptr<gltf::buffer>>&, uint8_t*)>> m_copy_vertices_data_callbacks{};
        std::vector<std::function<void(const std::vector<std::unique_ptr<gltf::buffer>>&, uint8_t*)>> m_copy_indices_data_callbacks{};

        std::vector<avk::vma_buffer> m_staging_buffers{};
    };
} // namespace


gltf::gltf_vk::primitive::primitive(const nlohmann::json& gltf_json, const nlohmann::json& primitive_json)
    : gltf::primitive(gltf_json, primitive_json)
{
}


gltf::gltf_vk gltf::gltf_vk::from_url(const std::string& url)
{
    gltf::gltf_vk result{};

    hal::filesystem::common_file file{};
    file.open(url);
    result.m_file_data = file.read_all_and_move();

    if (std::filesystem::path(url).extension() == ".gltf") {
        const auto file_data = file.read_all();
        const auto gltf_json = nlohmann::json::parse(
            result.m_file_data.get_data(),
            result.m_file_data.get_data() + result.m_file_data.get_size());
        result.m_model = gltf::model(gltf_json, std::make_unique<vk_assets_factory>());
    } else {
        auto glb_data = parse_glb_file(result.m_file_data);

        const auto gltf_json = nlohmann::json::parse(glb_data.json.data, glb_data.json.data + glb_data.json.size);
        result.m_model = gltf::model(
            gltf_json,
            utils::data::create_non_owning(const_cast<uint8_t*>(glb_data.bin.data), glb_data.bin.size),
            std::make_unique<vk_assets_factory>());
    }

    return result;
}


void gltf::gltf_vk::create_resources(vk::CommandBuffer& command_buffer, uint32_t queue_family)
{
    m_model.get_buffers().front()->get_data();

    auto& assets_factory = static_cast<vk_assets_factory&>(m_model.get_assets_factory());

    m_vertex_buffer = assets_factory.create_vertex_buffer(
        m_model.get_buffers(), command_buffer, queue_family);
    m_vertex_buffer_size = assets_factory.get_vertex_buffer_size();

    m_index_buffer = assets_factory.create_index_buffer(
        m_model.get_buffers(), command_buffer, queue_family);
    m_index_buffer_size = assets_factory.get_index_buffer_size();
}


void gltf::gltf_vk::clear_staging_resources()
{
    static_cast<vk_assets_factory&>(m_model.get_assets_factory()).clear_staging_resources();
}


void gltf::gltf_vk::draw(gltf_renderer& renderer)
{
    renderer.draw_scene(
        m_model,
        m_model.get_current_scene(),
        m_vertex_buffer.as<vk::Buffer>(),
        m_index_buffer.as<vk::Buffer>());
}


vk::Buffer gltf::gltf_vk::get_vertex_buffer() const
{
    return m_vertex_buffer.as<vk::Buffer>();
}


vk::Buffer gltf::gltf_vk::get_index_buffer() const
{
    return m_index_buffer.as<vk::Buffer>();
}


void gltf::gltf_vk::primitive::set_vertices_format(const std::vector<vk::VertexInputAttributeDescription>& new_format)
{
    m_attributes = new_format;

    uint32_t curr_binding = 0;

    m_bindings.reserve(m_attributes.size());

    for (auto& attribute : m_attributes) {
        attribute.binding = curr_binding;

        m_bindings.emplace_back(vk::VertexInputBindingDescription{
                .binding = curr_binding,
                .stride = avk::get_format_info(static_cast<VkFormat>(attribute.format)).size,
                .inputRate = vk::VertexInputRate::eVertex
            });

        curr_binding++;
    }
}


const std::vector<vk::VertexInputAttributeDescription>& gltf::gltf_vk::primitive::get_vertex_attributes() const
{
    return m_attributes;
}


const std::vector<vk::VertexInputBindingDescription>& gltf::gltf_vk::primitive::get_vertex_bindings() const
{
    return m_bindings;
}


void gltf::gltf_vk::primitive::set_vertex_buffer_offset(uint64_t offset)
{
    m_vertex_buffer_offset = offset;
}


uint64_t gltf::gltf_vk::primitive::get_vertex_buffer_offset() const
{
    return m_vertex_buffer_offset;
}


void gltf::gltf_vk::primitive::set_index_type(vk::IndexType new_index_type)
{
    m_index_type = new_index_type;
}


vk::IndexType gltf::gltf_vk::primitive::get_index_type() const
{
    return m_index_type;
}

void gltf::gltf_vk::primitive::set_index_buffer_offset(uint64_t offset)
{
    m_index_buffer_offset = offset;
}


uint64_t gltf::gltf_vk::primitive::get_index_buffer_offset() const
{
    return m_index_buffer_offset;
}
