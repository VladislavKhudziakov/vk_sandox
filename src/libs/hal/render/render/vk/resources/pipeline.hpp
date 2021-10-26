#pragma once

#include <render/vk/raii.hpp>

#include <utils/conditions_helpers.hpp>

#include <algorithm>

namespace sandbox::hal::render::avk
{
    class buffer_instance;

    class pipeline_instance
    {
        friend class pipeline_builder;

    public:
        pipeline_instance() = default;

        template<typename T>
        void push_constant(vk::ShaderStageFlags stage, const T& value)
        {
            auto range = std::find_if(
                m_push_constant_ranges.begin(),
                m_push_constant_ranges.end(),
                [stage](const vk::PushConstantRange& range) {
                    return range.stageFlags == stage;
                });

            ASSERT(sizeof(T) != range->size);
            ASSERT(range != m_push_constant_ranges.end());

            auto dst = m_push_constant_buffer.data() + range->offset;
            std::copy(dst, &value, sizeof(T));
        }

        void activate(vk::CommandBuffer& cmd_buffer, const std::vector<uint32_t>& dyn_offsets = {});

    private:
        std::vector<uint8_t> m_push_constant_buffer{};
        std::vector<vk::PushConstantRange> m_push_constant_ranges{};

        avk::descriptor_pool m_descriptor_pool{};
        avk::descriptor_set_list m_descriptor_sets{};
        avk::pipeline_layout m_pipeline_layout{};
        avk::pipeline m_pipeline{};
        vk::PipelineBindPoint m_bind_point{};
    };


    class pipeline_builder
    {
    public:
        pipeline_builder();

        pipeline_instance create_graphics_pipeline(vk::RenderPass pass, uint32_t subpass);
        pipeline_instance create_compute_pipeline();

        template<typename T>
        pipeline_builder& add_push_constant(vk::ShaderStageFlags stage, const T& value = T{})
        {
            auto range = std::find_if(
                m_push_constant_ranges.begin(),
                m_push_constant_ranges.end(),
                [stage](const vk::PushConstantRange& range) {
                    return range.stageFlags == stage;
                });

            assert(range == m_push_constant_ranges.end());

            uint32_t offset = m_push_constant_buffer.size();
            m_push_constant_buffer.reserve(m_push_constant_buffer.size() + sizeof(value));
            auto value_begin = reinterpret_cast<const uint8_t*>(&value);
            std::copy(value_begin, value_begin + sizeof(T), std::back_inserter(m_push_constant_buffer));

            m_push_constant_ranges.emplace_back(vk::PushConstantRange{
                .stageFlags = stage,
                .offset = offset,
                .size = sizeof(T),
            });

            return *this;
        }


        template<typename T>
        pipeline_builder& add_specialization_constant(const T& value)
        {
            const uint32_t constant_id = m_specializations_map.size();

            m_specializations_map.emplace_back(vk::SpecializationMapEntry{
                .constantID = constant_id,
                .offset = static_cast<uint32_t>(m_spec_data.size()),
                .size = sizeof(T),
            });

            m_spec_data.reserve(m_spec_data.size() + sizeof(value));

            auto value_begin = reinterpret_cast<const uint8_t*>(&value);
            std::copy(value_begin, value_begin + sizeof(T), std::back_inserter(m_spec_data));

            return *this;
        }

        pipeline_builder& begin_descriptor_set();
        pipeline_builder& finish_descriptor_set();

        pipeline_builder& set_shader_stages(
            const std::vector<std::pair<vk::ShaderModule, vk::ShaderStageFlagBits>>& stages_list);

        pipeline_builder& set_vertex_format(
            std::tuple<
                const vk::VertexInputAttributeDescription*,
                uint32_t,
                const vk::VertexInputBindingDescription*,
                uint32_t>);

        pipeline_builder& set_vertex_format(
            const std::vector<vk::VertexInputAttributeDescription>& attrs,
            const std::vector<vk::VertexInputBindingDescription>& bingings);

        pipeline_builder& set_primitive_topology(vk::PrimitiveTopology);
        pipeline_builder& set_cull_mode(vk::CullModeFlags);
        pipeline_builder& set_polygon_mode(vk::PolygonMode);

        pipeline_builder& enable_depth_test(bool);
        pipeline_builder& enable_depth_write(bool);
        pipeline_builder& set_depth_compare_op(vk::CompareOp);

        pipeline_builder& add_buffer(vk::Buffer, VkDeviceSize offset, VkDeviceSize size, vk::DescriptorType type);
        pipeline_builder& add_buffers(
            const std::vector<vk::Buffer>&,
            const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>&,
            vk::DescriptorType type);

        pipeline_builder& add_buffer(const buffer_instance&, vk::DescriptorType type);

        pipeline_builder& add_buffers(
            const std::vector<buffer_instance>&,
            vk::DescriptorType type);

        pipeline_builder& add_texture(vk::ImageView, vk::Sampler);
        pipeline_builder& add_textures(const std::vector<vk::ImageView>&, const std::vector<vk::Sampler>&);
        // TODO: add blending modes
        pipeline_builder& add_blend_state();

    private:
        pipeline_instance create_pipeline(vk::PipelineBindPoint);

        std::vector<avk::descriptor_set_layout> create_descriptor_set_layouts();
        avk::descriptor_pool create_descriptor_pool(const std::vector<vk::DescriptorSetLayout>&);
        avk::descriptor_set_list create_descriptor_sets(const avk::descriptor_pool& pool, const std::vector<vk::DescriptorSetLayout>&);

        avk::pipeline_layout create_pipeline_layout(const std::vector<vk::DescriptorSetLayout>&);

        std::vector<uint8_t> m_push_constant_buffer{};
        std::vector<vk::PushConstantRange> m_push_constant_ranges{};

        struct descriptor_set_data
        {
            std::unordered_map<vk::DescriptorType, uint32_t> descriptors_count{};
            std::vector<vk::DescriptorImageInfo> image_writes{};
            std::vector<vk::DescriptorBufferInfo> buffer_writes{};
            std::vector<vk::DescriptorSetLayoutBinding> bindings{};
            uint32_t curr_binding{0};
        };

        std::vector<descriptor_set_data> m_descriptor_sets{};

        std::vector<uint8_t> m_spec_data{};
        std::vector<vk::SpecializationMapEntry> m_specializations_map{};

        vk::SpecializationInfo m_specialization_info{
            .mapEntryCount = 0,
            .pMapEntries = nullptr,
            .dataSize = 0,
            .pData = nullptr};

        std::vector<vk::PipelineShaderStageCreateInfo> m_shader_stages{};

        std::vector<vk::VertexInputAttributeDescription> m_vertex_attributes{};
        std::vector<vk::VertexInputBindingDescription> m_vertex_bingings{};

        vk::PipelineVertexInputStateCreateInfo m_vertex_input_state{
            .flags = {},
            .vertexBindingDescriptionCount = 0,
            .vertexAttributeDescriptionCount = 0};

        vk::PipelineInputAssemblyStateCreateInfo m_input_assembly_state{
            .flags = {},
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE};

        std::vector<vk::PipelineColorBlendAttachmentState> m_attachments_blend_states{};

        vk::PipelineColorBlendStateCreateInfo m_color_blend_state{
            .flags = {},
            .logicOpEnable = VK_FALSE,
            .logicOp = {},
            .attachmentCount = 0};

        vk::PipelineRasterizationStateCreateInfo m_rasterization_state{
            .flags = {},
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
        };

        vk::PipelineViewportStateCreateInfo m_viewports_state{
            .flags = {},
            .viewportCount = 1,
            .scissorCount = 1,
        };

        vk::PipelineDepthStencilStateCreateInfo m_depth_stencil_state{
            .flags = {},
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLessOrEqual,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE};

        vk::PipelineMultisampleStateCreateInfo m_multisample_state{
            .flags = {},
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        inline static vk::DynamicState dynamic_states[]{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };

        vk::PipelineDynamicStateCreateInfo m_dynamic_state{
            .flags = {},
            .dynamicStateCount = std::size(dynamic_states),
            .pDynamicStates = dynamic_states,
        };

        vk::RenderPass m_pass{};
        uint32_t m_subpass{};
    };
} // namespace sandbox::hal::render::avk