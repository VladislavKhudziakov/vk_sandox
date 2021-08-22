

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

    class vk_assets_factory : public gltf::assets_factory
    {
        public:
            ~vk_assets_factory() override = default;

            sandbox::hal::render::avk::vma_buffer create_vertex_buffer(vk::CommandBuffer& command_buffer, uint32_t queue_family)
            {
                return create_buffer(
                    command_buffer,
                    queue_family,
                    m_vertex_buffer_size,
                    vk::BufferUsageFlagBits::eVertexBuffer,
                    vk::AccessFlagBits::eVertexAttributeRead,
                    m_copy_vertices_data_callbacks);
            }


            sandbox::hal::render::avk::vma_buffer create_index_buffer(vk::CommandBuffer& command_buffer, uint32_t queue_family)
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
                const std::vector<std::unique_ptr<gltf::buffer>>& buffers,
                const nlohmann::json& gltf_json,
                const nlohmann::json& primitive_json) override
            {
                auto get_buffer_data = [&buffers](const auto& meta_src) {
                    const auto& buffer = buffers[meta_src.buffer];
                    const auto* src = buffer->get_data().get_data();
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
                const std::vector<std::function<void(uint8_t*)>>& copy_callbacks)
            {
                m_staging_buffers.emplace_back() = create_staging_buffer(
                    queue_family,
                    buffer_size,
                    [&copy_callbacks](uint8_t* dst) {
                        for (auto& callback : copy_callbacks) {
                            callback(dst);
                        }
                    });

                auto buffer = avk::create_vma_buffer(
                    vk::BufferCreateInfo{
                        .flags = {},
                        .size = buffer_size,
                        .usage = vk::BufferUsageFlagBits::eTransferDst | buffer_usage,
                        .sharingMode = vk::SharingMode::eExclusive,
                        .queueFamilyIndexCount = 1,
                        .pQueueFamilyIndices = &queue_family
                            },
                            VmaAllocationCreateInfo{
                        .usage = VMA_MEMORY_USAGE_GPU_ONLY
                    });

                command_buffer.copyBuffer(
                    m_staging_buffers.back().as<vk::Buffer>(),
                    buffer.as<vk::Buffer>(),
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

                return buffer;
            }

            uint64_t m_vertex_buffer_size{0};
            uint64_t m_index_buffer_size{0};

            std::vector<std::function<void(uint8_t*)>> m_copy_vertices_data_callbacks{};
            std::vector<std::function<void(uint8_t*)>> m_copy_indices_data_callbacks{};

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

    if (std::filesystem::path(url).extension() == ".gltf") {
        const auto file_data = file.read_all();
        const auto gltf_json = nlohmann::json::parse(file_data.get_data(), file_data.get_data() + file_data.get_size());
        result.m_model = gltf::model(gltf_json, std::make_unique<vk_assets_factory>());
    } else {
        auto glb_data = parse_glb_file(file.read_all());
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
    auto& assets_factory = static_cast<vk_assets_factory&>(m_model.get_assets_factory());

    m_vertex_buffer = assets_factory.create_vertex_buffer(command_buffer, queue_family);
    m_vertex_buffer_size = assets_factory.get_vertex_buffer_size();

    m_index_buffer_size = assets_factory.create_index_buffer(command_buffer, queue_family);
    m_index_buffer_size = assets_factory.get_index_buffer_size();
}


void gltf::gltf_vk::draw(vk::CommandBuffer& command_buffer)
{
    const auto& scenes = m_model.get_scenes();
    const auto& all_nodes = m_model.get_nodes();
    const auto& all_meshes = m_model.get_meshes();
    const auto& all_materials = m_model.get_materials();

    const auto& scene_nodes = scenes[m_model.get_current_scene()]->get_nodes();

    draw(scene_nodes, all_nodes, all_meshes, all_materials, command_buffer);
}


void gltf::gltf_vk::draw(
    const std::vector<int32_t>& curr_nodes,
    const std::vector<std::unique_ptr<node>>& all_nodes,
    const std::vector<std::unique_ptr<mesh>>& all_meshes,
    const std::vector<std::unique_ptr<material>>& all_materials,
    vk::CommandBuffer& command_buffer)
{
    for (int32_t node : curr_nodes) {
        const auto& node_impl = all_nodes[node];

        if (node_impl->mesh < 0) {
            continue;
        }

        const auto& mesh_impl = all_meshes[node_impl->mesh];

        for (const auto& primitive : mesh_impl->get_primitives()) {
            const auto material_index = primitive->get_material();
            const auto& material = material_index >= 0 ? all_materials[material_index] : all_materials.back();
            m_primitive_renderer->draw(*primitive, *material, command_buffer);
        }

        draw(node_impl->children, all_nodes, all_meshes, all_materials, command_buffer);
    }
}


void gltf::gltf_vk::primitive::set_vertices_format(const std::vector<vk::VertexInputAttributeDescription>& new_format)
{
    m_attributes = new_format;

    m_vertices_format_stride = 0;

    for (const auto& attribute : m_attributes) {
        m_vertices_format_stride += avk::get_format_info(static_cast<VkFormat>(attribute.format)).size;
    }
}


const std::vector<vk::VertexInputAttributeDescription>& gltf::gltf_vk::primitive::get_vertices_format() const
{
    return m_attributes;
}


size_t gltf::gltf_vk::primitive::get_vertices_format_stride() const
{
    return m_vertices_format_stride;
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
