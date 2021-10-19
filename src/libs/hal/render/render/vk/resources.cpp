#include "resources.hpp"

#include <render/vk/utils.hpp>
#include<utils/scope_helpers.hpp>

using namespace sandbox::hal::render;

void avk::pipeline_instance::activate(vk::CommandBuffer& cmd_buffer, const std::vector<uint32_t>& dyn_offsets)
{
    for (const auto& range : m_push_constant_ranges) {
        cmd_buffer.pushConstants(m_pipeline_layout, range.stageFlags, 0, range.size, m_push_constant_buffer.data());
    }

    if (!m_descriptor_sets->empty()) {
        cmd_buffer.bindDescriptorSets(
            m_bind_point,
            m_pipeline_layout,
            0,
            m_descriptor_sets->size(),
            m_descriptor_sets->data(),
            dyn_offsets.size(),
            dyn_offsets.data());
    }

    cmd_buffer.bindPipeline(m_bind_point, m_pipeline);
}


avk::pipeline_builder::pipeline_builder()
{
}


avk::pipeline_instance avk::pipeline_builder::create_graphics_pipeline(vk::RenderPass pass, uint32_t subpass)
{
    m_pass = pass;
    m_subpass = subpass;

    return create_pipeline(vk::PipelineBindPoint::eGraphics);
}


avk::pipeline_instance avk::pipeline_builder::create_compute_pipeline()
{
    return create_pipeline(vk::PipelineBindPoint::eCompute);
}


avk::pipeline_builder& avk::pipeline_builder::set_vertex_format(
    const std::vector<vk::VertexInputAttributeDescription>& attrs, const std::vector<vk::VertexInputBindingDescription>& bingings)
{
    m_vertex_attributes = attrs;
    m_vertex_bingings = bingings;
    m_vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
        .flags = {},
        .vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertex_bingings.size()),
        .pVertexBindingDescriptions = m_vertex_bingings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertex_attributes.size()),
        .pVertexAttributeDescriptions = m_vertex_attributes.data()};

    return *this;
}


avk::buffer_instance::buffer_instance(avk::buffer_pool& pool)
    : m_pool(pool)
{
}


void avk::buffer_instance::upload(const std::function<void(uint8_t*)>& upd_callback)
{
    CHECK(is_updatable());

    m_pool.update_subresource(m_subresource_index, [this, upd_callback](uint8_t* dst) {
        upd_callback(dst + m_staging_buffer_offset);
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
    return m_pool.get_buffer();
}


bool avk::buffer_instance::is_updatable() const
{
    return static_cast<bool>(m_usage & vk::BufferUsageFlagBits::eTransferDst);
}


avk::pipeline_builder& avk::pipeline_builder::begin_descriptor_set()
{
    m_descriptor_sets.emplace_back();
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::finish_descriptor_set()
{
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::set_shader_stages(
    const std::vector<std::pair<vk::ShaderModule, vk::ShaderStageFlagBits>>& stages_list)
{
    m_shader_stages.clear();
    m_shader_stages.reserve(stages_list.size());

    for (const auto& [module, stage] : stages_list) {
        m_shader_stages.emplace_back(vk::PipelineShaderStageCreateInfo{
            .flags = {},
            .stage = stage,
            .module = module,
            .pName = "main",
            .pSpecializationInfo = &m_specialization_info,
        });
    }

    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::set_primitive_topology(vk::PrimitiveTopology topology)
{
    m_input_assembly_state.topology = topology;
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::set_cull_mode(vk::CullModeFlags cull_mode)
{
    m_rasterization_state.cullMode = cull_mode;
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::set_polygon_mode(vk::PolygonMode polygon_mode)
{
    m_rasterization_state.polygonMode = polygon_mode;
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::enable_depth_test(bool enable)
{
    m_depth_stencil_state.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::enable_depth_write(bool enable)
{
    m_depth_stencil_state.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::set_depth_compare_op(vk::CompareOp compare_op)
{
    m_depth_stencil_state.depthCompareOp = compare_op;
    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::add_buffer(
    vk::Buffer buffer,
    VkDeviceSize offset,
    VkDeviceSize size,
    vk::DescriptorType type)
{
    auto& curr_set = m_descriptor_sets.back();
    curr_set.descriptors_count[type]++;

    curr_set.bindings.emplace_back(vk::DescriptorSetLayoutBinding{
        .binding = curr_set.curr_binding++,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    });

    curr_set.buffer_writes.emplace_back(vk::DescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size,
    });

    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::add_buffers(
    const std::vector<vk::Buffer>& buffers,
    const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>& ranges,
    vk::DescriptorType type)
{
    auto& curr_set = m_descriptor_sets.back();
    ASSERT(buffers.size() == ranges.size());

    curr_set.descriptors_count[type] += buffers.size();

    curr_set.bindings.emplace_back(vk::DescriptorSetLayoutBinding{
        .binding = curr_set.curr_binding++,
        .descriptorType = type,
        .descriptorCount = static_cast<uint32_t>(buffers.size()),
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    });

    auto buffers_begin = buffers.begin();
    auto ranges_begin = ranges.begin();

    for (; buffers_begin != buffers.end(); ++buffers_begin, ++ranges_begin) {
        curr_set.buffer_writes.emplace_back(vk::DescriptorBufferInfo{
            .buffer = *buffers_begin,
            .offset = ranges_begin->first,
            .range = ranges_begin->second});
    }

    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::add_buffer(const buffer_instance& instance, vk::DescriptorType type)
{
    return add_buffer(instance, instance.get_size(), instance.get_offset(), type);
}

avk::pipeline_builder& avk::pipeline_builder::add_buffers(const std::vector<buffer_instance>& instance_list, vk::DescriptorType type)
{
    std::vector<vk::Buffer> vk_buffers{instance_list.size()};
    std::transform(instance_list.begin(), instance_list.end(), vk_buffers.begin(), [](const buffer_instance& i) { return i;});
    std::vector<std::pair<VkDeviceSize, VkDeviceSize>> size_offsets{instance_list.size()};
    std::transform(instance_list.begin(), instance_list.end(), size_offsets.begin(), [](const buffer_instance& i) { return std::make_pair<VkDeviceSize, VkDeviceSize>(i.get_size(), i.get_offset()); });

    return add_buffers(vk_buffers, size_offsets, type);
}


avk::pipeline_builder& avk::pipeline_builder::add_texture(vk::ImageView image_view, vk::Sampler image_sampler)
{
    auto& curr_set = m_descriptor_sets.back();
    curr_set.descriptors_count[vk::DescriptorType::eCombinedImageSampler]++;

    curr_set.bindings.emplace_back(vk::DescriptorSetLayoutBinding{
        .binding = curr_set.curr_binding++,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    });

    curr_set.image_writes.emplace_back(vk::DescriptorImageInfo{
        .sampler = image_sampler,
        .imageView = image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal});

    return *this;
}


avk::pipeline_builder& avk::pipeline_builder::add_textures(
    const std::vector<vk::ImageView>& images,
    const std::vector<vk::Sampler>& samplers)
{
    auto& curr_set = m_descriptor_sets.back();
    ASSERT(images.size() == samplers.size());

    curr_set.descriptors_count[vk::DescriptorType::eCombinedImageSampler] += images.size();

    curr_set.bindings.emplace_back(vk::DescriptorSetLayoutBinding{
        .binding = curr_set.curr_binding++,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = static_cast<uint32_t>(images.size()),
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    });

    auto images_begin = images.begin();
    auto samplers_begin = samplers.begin();

    for (; images_begin != images.end(); ++images_begin, ++samplers_begin) {
        curr_set.image_writes.emplace_back(vk::DescriptorImageInfo{
            .sampler = *samplers_begin,
            .imageView = *images_begin,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal});
    }

    return *this;
}

avk::pipeline_builder& avk::pipeline_builder::add_blend_state()
{
    // clang-format off
    m_attachments_blend_states.emplace_back(vk::PipelineColorBlendAttachmentState{
        .blendEnable = VK_FALSE, 
        .colorWriteMask = 
        vk::ColorComponentFlagBits::eR | 
        vk::ColorComponentFlagBits::eG | 
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA
    });
    // clang-format on
    return *this;
}


avk::pipeline_instance avk::pipeline_builder::create_pipeline(vk::PipelineBindPoint bind_point)
{
    avk::pipeline_instance new_pipeline{};

    auto desc_set_layouts = create_descriptor_set_layouts();
    auto desc_set_layouts_native = avk::to_elements_list<vk::DescriptorSetLayout>(desc_set_layouts.begin(), desc_set_layouts.end());

    auto desc_pool = create_descriptor_pool(desc_set_layouts_native);
    auto desc_list = create_descriptor_sets(desc_pool, desc_set_layouts_native);
    auto pipeline_layout = create_pipeline_layout(desc_set_layouts_native);

    new_pipeline.m_push_constant_buffer = m_push_constant_buffer;
    new_pipeline.m_push_constant_ranges = m_push_constant_ranges;

    m_specialization_info.dataSize = m_spec_data.size();
    m_specialization_info.pData = m_spec_data.data();
    m_specialization_info.mapEntryCount = m_specializations_map.size();
    m_specialization_info.pMapEntries = m_specializations_map.data();

    m_color_blend_state.pAttachments = m_attachments_blend_states.data();
    m_color_blend_state.attachmentCount = m_attachments_blend_states.size();

    avk::pipeline pipeline{};

    switch (bind_point) {
        case vk::PipelineBindPoint::eGraphics:
            // clang-format off
            pipeline = avk::create_graphics_pipeline(
                avk::context::device()->createGraphicsPipeline(
                    {},
                    vk::GraphicsPipelineCreateInfo{
                        .stageCount = static_cast<uint32_t>(m_shader_stages.size()),
                        .pStages = m_shader_stages.data(),
                        .pVertexInputState = &m_vertex_input_state,
                        .pInputAssemblyState = &m_input_assembly_state,
                        .pTessellationState = nullptr,
                        .pViewportState = &m_viewports_state,
                        .pRasterizationState = &m_rasterization_state,
                        .pMultisampleState = &m_multisample_state,
                        .pDepthStencilState = &m_depth_stencil_state,
                        .pColorBlendState = &m_color_blend_state,
                        .pDynamicState = &m_dynamic_state,
                        .layout = pipeline_layout,

                        .renderPass = m_pass,
                        .subpass = m_subpass,
                        .basePipelineHandle = {},
                        .basePipelineIndex = -1,
                    })
                    .value);
            // clang-format on
            break;
        case vk::PipelineBindPoint::eCompute:
            // clang-format off
            pipeline = avk::create_compute_pipeline(avk::context::device()->createComputePipeline(
                {},
                vk::ComputePipelineCreateInfo{
                    .flags = {},
                    .stage = m_shader_stages.front(),
                    .layout = pipeline_layout,
                    .basePipelineHandle = {},
                    .basePipelineIndex = -1
                }).value);
            // clang-format on
            break;
        default:
            break;
    }

    new_pipeline.m_pipeline = std::move(pipeline);
    new_pipeline.m_pipeline_layout = std::move(pipeline_layout);
    new_pipeline.m_descriptor_pool = std::move(desc_pool);
    new_pipeline.m_descriptor_sets = std::move(desc_list);
    new_pipeline.m_bind_point = bind_point;

    return new_pipeline;
}


avk::pipeline_layout avk::pipeline_builder::create_pipeline_layout(const std::vector<vk::DescriptorSetLayout>& layouts)
{
    return avk::gen_pipeline_layout(
        m_push_constant_ranges,
        layouts);
}


avk::descriptor_pool avk::pipeline_builder::create_descriptor_pool(
    const std::vector<vk::DescriptorSetLayout>& layouts)
{
    std::vector<vk::DescriptorPoolSize> descriptor_pool_sizes{};
    std::unordered_map<vk::DescriptorType, uint32_t> all_descriptors_sizes{};

    for (const auto& set : m_descriptor_sets) {
        for (const auto [type, count] : set.descriptors_count) {
            all_descriptors_sizes[type] += count;
        }
    };

    descriptor_pool_sizes.reserve(all_descriptors_sizes.size());

    for (const auto [type, count] : all_descriptors_sizes) {
        descriptor_pool_sizes.emplace_back(vk::DescriptorPoolSize{
            .type = type,
            .descriptorCount = count});
    }

    if (!descriptor_pool_sizes.empty()) {
        return avk::create_descriptor_pool(avk::context::device()->createDescriptorPool(
            vk::DescriptorPoolCreateInfo{
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = static_cast<uint32_t>(layouts.size()),
                .poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size()),
                .pPoolSizes = descriptor_pool_sizes.data(),
            }));
    }

    return {};
}


std::vector<avk::descriptor_set_layout> avk::pipeline_builder::create_descriptor_set_layouts()
{
    std::vector<avk::descriptor_set_layout> layouts_list{};

    for (const auto& set : m_descriptor_sets) {
        layouts_list.emplace_back(avk::create_descriptor_set_layout(
            avk::context::device()->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<uint32_t>(set.bindings.size()),
                .pBindings = set.bindings.data()})));
    }

    return layouts_list;
}


avk::descriptor_set_list avk::pipeline_builder::create_descriptor_sets(
    const avk::descriptor_pool& pool,
    const std::vector<vk::DescriptorSetLayout>& layouts)
{
    if (layouts.empty()) {
        return {};
    }

    auto new_sets = avk::allocate_descriptor_sets(vk::DescriptorSetAllocateInfo{
        .descriptorPool = pool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()});

    auto curr_set = new_sets->begin();

    std::vector<vk::WriteDescriptorSet> write_ops{};

    for (const auto& set : m_descriptor_sets) {
        auto images_write_begin = set.image_writes.begin();
        auto buffer_write_begin = set.buffer_writes.begin();
        write_ops.reserve(write_ops.size() + set.image_writes.size() + set.buffer_writes.size());

        for (const auto& binding : set.bindings) {
            switch (binding.descriptorType) {
                case vk::DescriptorType::eCombinedImageSampler:
                    for (uint32_t i = 0; i < binding.descriptorCount; i++) {
                        write_ops.emplace_back(vk::WriteDescriptorSet{
                            .dstSet = *curr_set,
                            .dstBinding = binding.binding,
                            .dstArrayElement = i,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .pImageInfo = &*images_write_begin++,
                            .pBufferInfo = nullptr,
                            .pTexelBufferView = nullptr});
                    }
                    break;
                case vk::DescriptorType::eUniformBuffer:
                case vk::DescriptorType::eStorageBuffer:
                case vk::DescriptorType::eUniformBufferDynamic:
                case vk::DescriptorType::eStorageBufferDynamic:
                    for (uint32_t i = 0; i < binding.descriptorCount; i++) {
                        write_ops.emplace_back(vk::WriteDescriptorSet{
                            .dstSet = *curr_set,
                            .dstBinding = binding.binding,
                            .dstArrayElement = i,
                            .descriptorCount = 1,
                            .descriptorType = binding.descriptorType,
                            .pImageInfo = nullptr,
                            .pBufferInfo = &*buffer_write_begin++,
                            .pTexelBufferView = nullptr});
                    }
                    break;
                default:
                    break;
            }
        }

        curr_set++;
    }

    avk::context::device()->updateDescriptorSets(write_ops.size(), write_ops.data(), 0, nullptr);

    return new_sets;
}


void avk::render_pass_instance::resize(uint32_t queue_family, uint32_t width, uint32_t height)
{
    m_framebuffer_width = width;
    m_framebuffer_height = height;

    if (m_attachments_usages.empty()) {
        m_attachments_usages.reserve(m_attachments_descriptions.size());

        for (const auto& _ : m_attachments_descriptions) {
            m_attachments_usages.emplace_back(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);
        }
    }

    m_framebuffers.clear();
    m_framebuffers.reserve(m_framebuffers_count);
    m_images.clear();
    m_images.reserve(m_attachments_descriptions.size() * m_framebuffers_count);
    m_images_views.clear();
    m_images_views.reserve(m_attachments_descriptions.size() * m_framebuffers_count);

    for (int i = 0; i < m_framebuffers_count; ++i) {
        auto [framebuffer, images, images_views] = avk::gen_framebuffer(
            width,
            height,
            queue_family,
            m_pass,
            m_attachments_descriptions,
            m_attachments_sizes,
            m_attachments_scales,
            m_attachments_usages);

        m_framebuffers.emplace_back(std::move(framebuffer));

        for (auto& img : images) {
            m_images.emplace_back(std::move(img));
        }

        for (auto& img_view : images_views) {
            m_images_views.emplace_back(std::move(img_view));
        }
    }
}


void avk::render_pass_instance::begin(vk::CommandBuffer& command_buffer, vk::SubpassContents content)
{
    command_buffer.setViewport(
        0,
        {vk::Viewport{
            .x = 0,
            .y = 0,
            .width = static_cast<float>(m_framebuffer_width),
            .height = static_cast<float>(m_framebuffer_height),
            .minDepth = 0,
            .maxDepth = 1}});

    command_buffer.setScissor(
        0, {vk::Rect2D{
               .offset = {.x = 0, .y = 0},
               .extent = {
                   .width = m_framebuffer_width,
                   .height = m_framebuffer_height,
               },
           }});

    command_buffer.beginRenderPass(
        vk::RenderPassBeginInfo{
            .renderPass = m_pass,
            .framebuffer = m_framebuffers[m_current_framebuffer_index],
            .renderArea = {
                .offset = {
                    .x = 0,
                    .y = 0,
                },
                .extent = {
                    .width = m_framebuffer_width,
                    .height = m_framebuffer_height,
                },
            },

            .clearValueCount = static_cast<uint32_t>(m_clear_values.size()),
            .pClearValues = m_clear_values.data(),
        },
        content);
}


void avk::render_pass_instance::next_sub_pass(vk::CommandBuffer& command_buffer, vk::SubpassContents content)
{
    command_buffer.nextSubpass(content);
}


void avk::render_pass_instance::finish(vk::CommandBuffer& command_buffer)
{
    command_buffer.endRenderPass();
    m_current_framebuffer_index = (m_current_framebuffer_index + 1) % m_framebuffers_count;
}


vk::RenderPass avk::render_pass_instance::get_native_pass() const
{
    return m_pass.as<vk::RenderPass>();
}


vk::Image avk::render_pass_instance::get_framebuffer_image(uint32_t image, uint32_t framebuffer_index)
{
    return m_images[m_attachments_descriptions.size() * framebuffer_index + image].as<vk::Image>();
}


avk::render_pass_builder& avk::render_pass_builder::begin_sub_pass(vk::SampleCountFlagBits samples)
{
    m_sub_passes_data.emplace_back(sub_pass_data{
        .samples = samples});

    return *this;
}


avk::render_pass_builder& avk::render_pass_builder::add_color_attachment(
    vk::Extent2D size,
    scale_fator scale,
    vk::Format format,
    vk::ImageLayout init_layout,
    vk::ImageLayout subpass_layout,
    vk::ImageLayout final_layout,
    std::optional<std::array<float, 4>> clear_values)
{
    m_attachments_sizes.emplace_back(size);
    m_attachments_scales.emplace_back(scale);

    m_attachments.emplace_back(vk::AttachmentDescription{
        .flags = {},
        .format = format,
        .samples = m_sub_passes_data.back().samples,
        .loadOp = clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .initialLayout = init_layout,
        .finalLayout = final_layout});

    m_sub_passes_data.back().color_attachments.emplace_back(vk::AttachmentReference{
        .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
        .layout = subpass_layout,
    });

    if (clear_values) {
        m_clear_values.emplace_back(vk::ClearColorValue{*clear_values});
    }

    if (m_sub_passes_data.back().samples != vk::SampleCountFlagBits::e1) {
        m_attachments_sizes.emplace_back(size);
        m_attachments_scales.emplace_back(scale);

        m_attachments.emplace_back(vk::AttachmentDescription{
            .flags = {},
            .format = format,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .initialLayout = init_layout,
            .finalLayout = final_layout});

        m_sub_passes_data.back().resolve_attachments.emplace_back(vk::AttachmentReference{
            .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
            .layout = subpass_layout,
        });

        if (clear_values) {
            m_clear_values.emplace_back(vk::ClearColorValue{*clear_values});
        }
    }

    return *this;
}


avk::render_pass_builder& avk::render_pass_builder::set_depth_stencil_attachment(
    vk::Extent2D size,
    scale_fator scale,
    vk::Format format,
    vk::ImageLayout init_layout,
    vk::ImageLayout subpass_layout,
    vk::ImageLayout final_layout,
    std::optional<float> clear_values,
    std::optional<uint32_t> stencil_clear_values)
{
    m_attachments_sizes.emplace_back(size);
    m_attachments_scales.emplace_back(scale);

    ASSERT(!m_sub_passes_data.back().depth_stencil_attachment_ref);

    // TODO MSAA
    m_attachments.emplace_back(vk::AttachmentDescription{
        .flags = {},
        .format = format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = stencil_clear_values ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
        .stencilStoreOp = vk::AttachmentStoreOp::eStore,
        .initialLayout = init_layout,
        .finalLayout = final_layout});

    m_sub_passes_data.back().depth_stencil_attachment_ref = vk::AttachmentReference{
        .attachment = static_cast<uint32_t>(m_attachments.size() - 1),
        .layout = subpass_layout,
    };

    if (clear_values || stencil_clear_values) {
        m_clear_values.emplace_back(vk::ClearDepthStencilValue{
            clear_values ? *clear_values : 1,
            stencil_clear_values ? *stencil_clear_values : 0});
    }

    return *this;
}


avk::render_pass_builder& avk::render_pass_builder::finish_sub_pass()
{
    return *this;
}


avk::render_pass_instance avk::render_pass_builder::create_pass(uint32_t framebuffers_count)
{
    avk::render_pass_instance new_pass{};

    for (const auto& pass_data : m_sub_passes_data) {
        m_sub_passes.emplace_back(vk::SubpassDescription{
            .flags = {},
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = static_cast<uint32_t>(pass_data.color_attachments.size()),
            .pColorAttachments = pass_data.color_attachments.data(),
            .pResolveAttachments = pass_data.resolve_attachments.data(),
            .pDepthStencilAttachment = pass_data.depth_stencil_attachment_ref ? &*pass_data.depth_stencil_attachment_ref : nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr,
        });
    }

    new_pass.m_pass = avk::create_render_pass(avk::context::device()->createRenderPass(vk::RenderPassCreateInfo{
        .flags = {},
        .attachmentCount = static_cast<uint32_t>(m_attachments.size()),
        .pAttachments = m_attachments.data(),
        .subpassCount = static_cast<uint32_t>(m_sub_passes.size()),
        .pSubpasses = m_sub_passes.data(),
        .dependencyCount = static_cast<uint32_t>(m_sub_pass_dependencies.size()),
        .pDependencies = m_sub_pass_dependencies.data(),
    }));

    new_pass.m_attachments_scales = m_attachments_scales;
    new_pass.m_attachments_sizes = m_attachments_sizes;
    new_pass.m_clear_values = m_clear_values;
    new_pass.m_attachments_descriptions = m_attachments;
    new_pass.m_framebuffers_count = framebuffers_count;

    return new_pass;
}


avk::render_pass_builder& avk::render_pass_builder::add_sub_pass_dependency(const vk::SubpassDependency& dep)
{
    m_sub_pass_dependencies.emplace_back(dep);
    return *this;
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


avk::buffer_instance avk::buffer_builder::create(const std::function<void(uint8_t*)>& copy_callback)
{
    buffer_instance result{m_pool};

    CHECK_MSG(m_size > 0, "Cannot create empty buffer.");
    CHECK_MSG(static_cast<uint32_t>(m_usage) != 0, "Buffer usage didn't specified.");

    result.m_size = m_size;
    result.m_usage = m_usage;

    m_pool.add_buffer_instance(&result);

    if (copy_callback) {
        result.upload(copy_callback);
    }

    return result;
}


void avk::buffer_pool::add_buffer_instance(buffer_instance* instance)
{
    instance->m_buffer_offset = m_size;
    m_size += instance->m_size;

    if (instance->is_updatable()) {
        instance->m_staging_buffer_offset = m_staging_size;
        m_staging_size += instance->m_buffer_offset;
    }

    instance->m_subresource_index = m_subresources.size();

    m_subresources.emplace_back(buffer_subresource{
        .size = instance->m_size,
        .offset = instance->m_buffer_offset,
        .staging_offset = instance->m_staging_buffer_offset,
        .usage = instance->m_usage
    });
}


void sandbox::hal::render::avk::buffer_pool::update_subresource(uint32_t subresource, const std::function<void(uint8_t*)>& cb)
{
    m_upload_callbacks.emplace_back(cb);
    m_subresources_to_update.emplace(subresource);
}


void avk::buffer_pool::flush(uint32_t queue_family, vk::CommandBuffer& command_buffer)
{
    if (m_staging_size > 0) {
        m_staging_buffer = avk::gen_staging_buffer(queue_family, m_staging_size, [this](uint8_t* dst) {
            for (const auto& cp : m_upload_callbacks) {
                cp(dst);
            }
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

    update_internal(command_buffer, UPDATE_RESOURCE_BUFFER_BIT);
    m_upload_callbacks.clear();
}


void avk::buffer_pool::update(vk::CommandBuffer& command_buffer)
{
    update_internal(command_buffer, UPDATE_STAGING_BUFFER_BIT | UPDATE_RESOURCE_BUFFER_BIT);
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


void avk::buffer_pool::update_internal(
  vk::CommandBuffer& command_buffer, 
  uint32_t update_state)
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

            for (const auto& cp : m_upload_callbacks) {
                cp(static_cast<uint8_t*>(dst_ptr));
            }

            m_upload_callbacks.clear();
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
            command_buffer.copyBuffer(m_staging_buffer.as<vk::Buffer>(), m_resource.as<vk::Buffer>(), buffer_copies);
            command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, dst_stages, {}, {}, buffer_barriers, {});
        }

        m_subresources_to_update.clear();
    }
}
