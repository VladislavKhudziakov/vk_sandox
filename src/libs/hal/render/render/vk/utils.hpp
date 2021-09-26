#pragma once

#include <render/vk/raii.hpp>

#include <functional>
#include <vector>
#include <optional>
#include <array>

namespace sandbox::hal::render::avk
{
    namespace detail
    {
        template<typename T, typename = void>
        struct is_reservable
        {
            constexpr static bool value = false;
        };

        template<typename T>
        struct is_reservable<T, std::void_t<decltype(std::declval<T>().reserve(1))>>
        {
            constexpr static bool value = true;
        };

        template<typename T>
        constexpr bool is_reservable_v = is_reservable<T>::value;
    } // namespace detail

    template<
        typename CastType,
        typename IterType,
        template<typename T> typename AllocatorType = std::allocator,
        template<typename T, typename Allocator = AllocatorType<CastType>> typename Container = std::vector>
    Container<CastType, AllocatorType<CastType>> to_elements_list(IterType begin, IterType end)
    {
        using ConstainerT = Container<CastType, AllocatorType<CastType>>;
        ConstainerT res;

        if constexpr (detail::is_reservable_v<ConstainerT>) {
            res.reserve(std::distance(begin, end));
        }

        for (; begin != end; ++begin) {
            res.emplace_back(begin->template as<CastType>());
        }

        return res;
    }


    std::pair<avk::vma_image, avk::image_view> gen_attachment(
        uint32_t width,
        uint32_t height,
        vk::Format format,
        uint32_t queue_family,
        vk::ImageUsageFlags usage = {},
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);


    avk::vma_buffer gen_staging_buffer(
        uint32_t queue_family, size_t data_size, const std::function<void(uint8_t*)>& on_buffer_mapped = {});


    avk::framebuffer gen_framebuffer(
        uint32_t width, uint32_t height, const vk::RenderPass& pass, const vk::ImageView* attachments, uint32_t attachments_count);


    std::tuple<avk::framebuffer, std::vector<avk::vma_image>, std::vector<avk::image_view>>
    gen_framebuffer(
        uint32_t width,
        uint32_t height,
        uint32_t queue_family,
        const vk::RenderPass& pass,
        const std::vector<vk::AttachmentDescription>& attachments,
        const std::vector<vk::Extent2D>& attachment_sizes = {},
        const std::vector<std::pair<float, float>>& attachments_scales = {},
        const std::vector<vk::ImageUsageFlags>& attachments_usages = {});


    avk::descriptor_set_layout gen_descriptor_set_layout(
        uint32_t count, vk::DescriptorType type);


    std::pair<avk::descriptor_pool, avk::descriptor_set_list> gen_descriptor_sets(
        const std::vector<vk::DescriptorSetLayout>& layouts,
        const std::vector<std::pair<uint32_t, vk::DescriptorType>>& layouts_data);


    void write_texture_descriptors(
        vk::DescriptorSet dst_set,
        const std::vector<vk::ImageView>&,
        const std::vector<vk::Sampler>&);


    void write_buffer_descriptors(
        vk::DescriptorSet dst_set,
        const std::vector<vk::Buffer>&,
        const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>& offsets_ranges);


    avk::pipeline_layout gen_pipeline_layout(
        const std::vector<vk::PushConstantRange>&,
        const std::vector<vk::DescriptorSetLayout>&);


    std::pair<avk::vma_image, avk::image_view> gen_texture(
        uint32_t queue_family,
        vk::ImageViewType type,
        vk::Format format,
        uint32_t width,
        uint32_t height,
        uint32_t depth = 1,
        uint32_t levels = 1,
        uint32_t layers = 1);


    avk::sampler gen_sampler(
        vk::Filter min_filter,
        vk::Filter mag_filter,
        vk::SamplerMipmapMode mip_filter,
        vk::SamplerAddressMode addr_mode_u,
        vk::SamplerAddressMode addr_mode_v,
        vk::SamplerAddressMode addr_mode_w,
        float max_lod,
        bool anisotropy = false);


    void copy_buffer_to_image(
        vk::CommandBuffer command_buffer,
        const vk::Buffer& staging_buffer,
        const vk::Image& dst_image,
        uint32_t image_width,
        uint32_t image_height,
        uint32_t image_level_count,
        uint32_t image_layers_count,
        size_t buffer_offset = 0);


    std::pair<avk::vma_buffer, avk::vma_buffer> gen_buffer(
        vk::CommandBuffer& command_buffer,
        uint32_t queue_family,
        uint32_t buffer_size,
        vk::BufferUsageFlagBits buffer_usage,
        vk::PipelineStageFlagBits wait_stage = {},
        vk::AccessFlagBits access_flags = {},
        const std::function<void(const uint8_t* dst)>& on_mapped_callback = {});


    void upload_buffer_data(
        vk::CommandBuffer& command_buffer,
        const avk::vma_buffer& staging_buffer,
        const avk::vma_buffer& dst_buffer,
        vk::PipelineStageFlagBits dst_stage,
        vk::AccessFlagBits dst_access,
        VkDeviceSize size,
        const std::function<void(const uint8_t* dst)>& upload);


    struct vk_format_info
    {
        uint32_t size;
        uint32_t channel_count;
    };

    vk_format_info get_format_info(VkFormat format);


    struct pass_create_info
    {
        std::vector<vk::AttachmentDescription> attachments{};
        std::vector<vk::AttachmentReference> attachments_refs{};
        std::vector<vk::SubpassDescription> subpasses{};
        std::vector<vk::SubpassDependency> subpass_dependencies{};
    };

    class render_pass_wrapper
    {
    public:
        render_pass_wrapper() = default;

        render_pass_wrapper(pass_create_info info);
        const vk::RenderPassCreateInfo& get_info() const;
        vk::RenderPass get_native_pass() const;

    private:
        pass_create_info m_info{};
        std::vector<vk::AttachmentReference> m_attachments_refs{};
        vk::RenderPassCreateInfo m_pass_info{};
        avk::render_pass m_pass_handler{};
    };


    class _graphics_pipeline
    {
        friend class graphics_pipeline_builder;

    public:
        _graphics_pipeline() = default;

        template<typename T>
        void push_constant(vk::ShaderStageFlags stage, const T& value)
        {
            auto range = std::find_if(
                m_push_constant_ranges.begin(),
                m_push_constant_ranges.end(),
                [stage](const vk::PushConstantRange& range) {
                    return range.stageFlags == stage;
                });

            if (range == m_push_constant_ranges.end()) {
                // TODO
                throw std::runtime_error("Cannot find push constant stage.");
            }

            if (sizeof(T) != range->size) {
                // TODO
                throw std::runtime_error("Bad push constant value.");
            }

            auto dst = m_push_constant_buffer.data() + range->offset;
            std::copy(dst, &value, sizeof(T));
        }

        void activate(vk::CommandBuffer& cmd_buffer, const std::vector<uint32_t>& dyn_offsets = {});

    private:
        std::vector<vk::DescriptorSetLayoutBinding> m_buffers_descriptor_bindings{};
        std::vector<vk::DescriptorSetLayoutBinding> m_textures_descriptor_bindings{};

        std::vector<uint8_t> m_push_constant_buffer{};
        std::vector<vk::PushConstantRange> m_push_constant_ranges{};

        avk::descriptor_pool m_descriptor_pool{};
        avk::descriptor_set_list m_descriptor_sets{};
        avk::pipeline_layout m_pipeline_layout{};
        avk::pipeline m_pipeline{};
    };


    class graphics_pipeline_builder
    {
    public:
        graphics_pipeline_builder(vk::RenderPass pass, uint32_t subpass, size_t attachments_count);

        _graphics_pipeline create_pipeline();

        template<typename T>
        graphics_pipeline_builder& add_push_constant(vk::ShaderStageFlags stage, const T& value = T{})
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
        graphics_pipeline_builder& add_specialization_constant(const T& value)
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

        graphics_pipeline_builder& set_shader_stages(
            const std::vector<std::pair<vk::ShaderModule, vk::ShaderStageFlagBits>>& stages_list);

        graphics_pipeline_builder& set_vertex_format(
            const std::vector<vk::VertexInputAttributeDescription>& attrs,
            const std::vector<vk::VertexInputBindingDescription>& bingings);

        graphics_pipeline_builder& set_primitive_topology(vk::PrimitiveTopology);
        graphics_pipeline_builder& set_cull_mode(vk::CullModeFlags);
        graphics_pipeline_builder& set_polygon_mode(vk::PolygonMode);

        graphics_pipeline_builder& enable_depth_test(bool);
        graphics_pipeline_builder& enable_depth_write(bool);
        graphics_pipeline_builder& set_depth_compare_op(vk::CompareOp);

        graphics_pipeline_builder& add_buffer(vk::Buffer, VkDeviceSize offset, VkDeviceSize size, vk::DescriptorType type);
        graphics_pipeline_builder& add_buffers(
            const std::vector<vk::Buffer>&,
            const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>&,
            vk::DescriptorType type);

        graphics_pipeline_builder& add_texture(vk::ImageView, vk::Sampler);
        graphics_pipeline_builder& add_textures(const std::vector<vk::ImageView>&, const std::vector<vk::Sampler>&);

    private:
        std::vector<avk::descriptor_set_layout> create_descriptor_set_layouts();
        avk::descriptor_pool create_descriptor_pool(const std::vector<vk::DescriptorSetLayout>&);
        avk::descriptor_set_list create_descriptor_sets(const avk::descriptor_pool& pool, const std::vector<vk::DescriptorSetLayout>&);

        avk::pipeline_layout create_pipeline_layout(const std::vector<vk::DescriptorSetLayout>&);

        std::vector<uint8_t> m_push_constant_buffer{};
        std::vector<vk::PushConstantRange> m_push_constant_ranges{};

        std::unordered_map<vk::DescriptorType, uint32_t> m_descriptors_count{};
        std::vector<vk::DescriptorSetLayoutBinding> m_buffers_descriptor_bindings{};
        std::vector<vk::DescriptorSetLayoutBinding> m_textures_descriptor_bindings{};

        std::vector<vk::DescriptorImageInfo> m_descriptor_images_writes{};
        std::vector<vk::DescriptorBufferInfo> m_descriptor_buffers_writes{};

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


    class _render_pass
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
        _render_pass create_pass(uint32_t framebuffers_count = 1);

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
