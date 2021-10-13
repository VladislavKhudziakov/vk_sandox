#pragma once

#include <render/vk/raii.hpp>
#include <utils/conditions_helpers.hpp>

namespace sandbox::hal::render::avk
{
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


    class render_pass_instance
    {
        friend class render_pass_builder;

    public:
        using scale_fator = std::pair<float, float>;

        void begin(vk::CommandBuffer&, vk::SubpassContents content = vk::SubpassContents::eInline);
        void next_sub_pass(vk::CommandBuffer&, vk::SubpassContents content = vk::SubpassContents::eInline);
        void finish(vk::CommandBuffer&);

        void resize(uint32_t queue_family, uint32_t width, uint32_t height);

        vk::RenderPass get_native_pass() const;

        vk::Image get_framebuffer_image(uint32_t image, uint32_t framebuffer_index = 0);

    private:
        std::vector<avk::framebuffer> m_framebuffers{};
        std::vector<avk::vma_image> m_images{};
        std::vector<avk::image_view> m_images_views{};

        std::vector<vk::AttachmentDescription> m_attachments_descriptions{};
        std::vector<vk::Extent2D> m_attachments_sizes{};
        std::vector<scale_fator> m_attachments_scales{};
        std::vector<vk::ImageUsageFlags> m_attachments_usages{};
        std::vector<vk::ClearValue> m_clear_values{};

        avk::render_pass m_pass{};
        size_t m_framebuffers_count{1};
        uint32_t m_current_framebuffer_index{0};
        uint32_t m_framebuffer_width{};
        uint32_t m_framebuffer_height{};
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


    class render_pass_builder
    {
    public:
        using scale_fator = std::pair<float, float>;

        render_pass_builder& begin_sub_pass(vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

        render_pass_builder& add_color_attachment(
            vk::Extent2D size,
            scale_fator scale,
            vk::Format format,
            vk::ImageLayout init_layout,
            vk::ImageLayout subpass_layout,
            vk::ImageLayout final_layout,
            std::optional<std::array<float, 4>> clear_values = std::nullopt);

        render_pass_builder& set_depth_stencil_attachment(
            vk::Extent2D size,
            scale_fator scale,
            vk::Format format,
            vk::ImageLayout init_layout,
            vk::ImageLayout subpass_layout,
            vk::ImageLayout final_layout,
            std::optional<float> depth_clear_values = std::nullopt,
            std::optional<uint32_t> stencil_clear_values = std::nullopt);

        render_pass_builder& finish_sub_pass();
        render_pass_builder& add_sub_pass_dependency(const vk::SubpassDependency&);
        render_pass_instance create_pass(uint32_t framebuffers_count = 1);

    private:
        struct sub_pass_data
        {
            vk::SampleCountFlagBits samples{};
            std::vector<vk::AttachmentReference> color_attachments{};
            std::vector<vk::AttachmentReference> resolve_attachments{};
            std::optional<vk::AttachmentReference> depth_stencil_attachment_ref{};
        };

        std::vector<vk::Extent2D> m_attachments_sizes{};
        std::vector<scale_fator> m_attachments_scales{};
        std::vector<vk::ClearValue> m_clear_values{};

        std::vector<vk::AttachmentDescription> m_attachments{};
        std::vector<sub_pass_data> m_sub_passes_data{};
        std::vector<vk::SubpassDescription> m_sub_passes{};
        std::vector<vk::SubpassDependency> m_sub_pass_dependencies{};
    };
} // namespace sandbox::hal::render::avk