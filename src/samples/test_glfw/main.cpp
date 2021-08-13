
#include <render/vk/raii.hpp>
#include <render/vk/utils.hpp>
#include <window/vk_main_loop_update_listener.hpp>
#include <filesystem/common_file.hpp>

#include <stb/stb_image.h>
using namespace sandbox::hal::render;

class hello_triangle_listener : public sandbox::hal::vk_main_loop_update_listener
{
public:
    ~hello_triangle_listener() override = default;

    void on_swapchain_reset(size_t width, size_t height) override
    {
        std::call_once(m_resources_creation_flag, [this]() {
            create_resources();
        });

        create_framebuffer_data(width, height);
        write_commands();
    }

    void update(uint64_t dt) override
    {
        if (!m_semaphore) {
            m_semaphore = avk::create_semaphore(avk::context::device()->createSemaphore({}));
        }

        vk::Queue queue = avk::context::queue(vk::QueueFlagBits::eGraphics, 0);
        queue.submit(vk::SubmitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = m_command_buffer->data(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = m_semaphore.handler_ptr(),
        });
    }

    vk::Image get_present_image() const override
    {
        return m_attachment.as<vk::Image>();
    }

    vk::ImageLayout get_present_image_layout() const override
    {
        return vk::ImageLayout::eTransferSrcOptimal;
    }

    vk::Extent2D get_present_image_size() const override
    {
        return {
            m_attachment_width,
            m_attachment_height
        };
    }

    std::vector<vk::Semaphore> get_wait_semaphores() const override
    {
        return {m_semaphore};
    }

private:
    void create_resources()
    {
        create_pass();
        create_pipeline();
    }

    void create_framebuffer_data(size_t width, size_t height)
    {
        m_attachment_width = width;
        m_attachment_height = height;

        uint32_t queue_family = avk::context::queue_family(vk::QueueFlagBits::eGraphics);

        m_attachment = avk::create_vma_image(
            vk::ImageCreateInfo{
                .flags = {},
                .imageType = vk::ImageType::e2D,
                .format = vk::Format::eR8G8B8A8Srgb,
                .extent = {
                        .width = static_cast<uint32_t>(width),
                        .height = static_cast<uint32_t>(height),
                        .depth = 1,
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment,
                    .sharingMode = vk::SharingMode::eExclusive,
                    .queueFamilyIndexCount = 1,
                    .pQueueFamilyIndices = &queue_family,
                    .initialLayout = vk::ImageLayout::eUndefined,
                },

            VmaAllocationCreateInfo{
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            });

        m_attachment_view = avk::create_image_view(avk::context::device()->createImageView(vk::ImageViewCreateInfo {
            .flags = {},
            .image = m_attachment.as<vk::Image>(),
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR8G8B8A8Srgb,
            .components = {
                .r = vk::ComponentSwizzle::eR,
                .g = vk::ComponentSwizzle::eG,
                .b = vk::ComponentSwizzle::eB,
                .a = vk::ComponentSwizzle::eA,
            },
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        }));

        m_framebuffer = avk::create_framebuffer(avk::context::device()->createFramebuffer(vk::FramebufferCreateInfo{
            .flags = {},
            .renderPass = m_pass,

            .attachmentCount = 1,
            .pAttachments = m_attachment_view.handler_ptr(),

            .width = width,
            .height = height,
            .layers = 1,
        }));
    }


    void create_pass()
    {
        vk::AttachmentReference color_ref{
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
        };

        vk::SubpassDescription subpass {
            .flags = {},
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_ref,
        };

        vk::AttachmentDescription pass_attachment{
            .flags = {},
            .format = vk::Format::eR8G8B8A8Srgb,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eTransferSrcOptimal,
        };

        vk::SubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eTransfer,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead,
            .dependencyFlags = {},
        };

        m_pass = avk::create_render_pass(avk::context::device()->createRenderPass(
            vk::RenderPassCreateInfo {
                .flags = {},
                .attachmentCount = 1,
                .pAttachments = &pass_attachment,
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = 0,
                .pDependencies = &dependency,
            }
        ));
    }

    void create_shader(avk::shader_module& module, const std::string path) {
        if (module) {
            return;
        }

        sandbox::hal::filesystem::common_file file{};
        file.open(path);
        assert(file.get_size());

        module = avk::create_shader_module(avk::context::device()->createShaderModule(vk::ShaderModuleCreateInfo{
            .flags = {},
            .codeSize = file.get_size(),
            .pCode = reinterpret_cast<const uint32_t*>(file.read_all().get_data())
        }));
    }

    void create_pipeline()
    {
        m_layout = avk::create_pipeline_layout(avk::context::device()->createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = 0,
            .pushConstantRangeCount = 0
        }));

        create_shader(m_vertex_shader, "./resources/test.vert.spv");
        create_shader(m_fragment_shader, "./resources/test.frag.spv");

        vk::PipelineShaderStageCreateInfo stages[] = {
            vk::PipelineShaderStageCreateInfo {
                .flags = {},
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = m_vertex_shader,
                .pName = "main",
            },
            vk::PipelineShaderStageCreateInfo {
                .flags = {},
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = m_fragment_shader,
                .pName = "main",
            }
        };

        vk::PipelineVertexInputStateCreateInfo vertex_input {
            .flags = {},
            .vertexBindingDescriptionCount = 0,
            .vertexAttributeDescriptionCount = 0,
        };

        vk::PipelineInputAssemblyStateCreateInfo input_assembly {
            .flags = {},
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE
        };

        vk::PipelineColorBlendAttachmentState attachment_blend{
            .blendEnable = VK_FALSE,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

        vk::PipelineColorBlendStateCreateInfo color_blend {
            .flags = {},
            .logicOpEnable = VK_FALSE,
            .logicOp = {},
            .attachmentCount = 1,
            .pAttachments = &attachment_blend
        };

        vk::PipelineRasterizationStateCreateInfo rasterization {
            .flags = {},
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
        };

        vk::PipelineViewportStateCreateInfo viewport{
            .flags = {},
            .viewportCount = 1,
            .scissorCount = 1,
        };

        vk::PipelineDepthStencilStateCreateInfo depth_stencil {
            .flags = {},
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = vk::CompareOp::eLessOrEqual,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .minDepthBounds = 0,
            .maxDepthBounds = 1
        };

        vk::SampleMask sampleMask{};
        vk::PipelineMultisampleStateCreateInfo multisample {
            .flags = {},
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };


        vk::DynamicState dyn_states[]{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };

        vk::PipelineDynamicStateCreateInfo dynamic_state{
            .flags = {},
            .dynamicStateCount = std::size(dyn_states),
            .pDynamicStates = dyn_states,
        };

        m_pipeline = avk::create_graphics_pipeline(
            avk::context::device()->createGraphicsPipeline(
                {},
                vk::GraphicsPipelineCreateInfo{
                    .stageCount = std::size(stages),
                    .pStages = stages,
                    .pVertexInputState = &vertex_input,
                    .pInputAssemblyState = &input_assembly,
                    .pTessellationState = nullptr,
                    .pViewportState = &viewport,
                    .pRasterizationState = &rasterization,
                    .pMultisampleState = &multisample,
                    .pDepthStencilState = &depth_stencil,
                    .pColorBlendState = &color_blend,
                    .pDynamicState = &dynamic_state,
                    .layout = m_layout,

                    .renderPass = m_pass,
                    .subpass = 0,
                    .basePipelineHandle = {},
                    .basePipelineIndex = -1,
                }).value);
    }


    void write_commands()
    {
        std::call_once(m_create_buffer_flag, [this]() {
            m_command_pool = avk::create_command_pool(avk::context::device()->createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = avk::context::queue_family(vk::QueueFlagBits::eGraphics),
            }));

            m_command_buffer = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
                .commandPool = m_command_pool,
                .commandBufferCount = 1,
            });
        });

        vk::CommandBuffer command_buffer = m_command_buffer->at(0);
        command_buffer.reset();

        command_buffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse
        });

        command_buffer.setViewport(
            0, {
            vk::Viewport{
                .x = 0,
                .y = 0,
                .width = static_cast<float>(m_attachment_width),
                .height = static_cast<float>(m_attachment_height),
                .minDepth = 0,
                .maxDepth = 1
            }});

        command_buffer.setScissor(
            0, {vk::Rect2D {
                .offset = {
                    .x = 0,
                    .y = 0
                },
                .extent = {
                    .width = static_cast<uint32_t>(m_attachment_width),
                    .height = static_cast<uint32_t>(m_attachment_height),
                },
            }});

        vk::ClearValue clear_value{
            vk::ClearColorValue{std::array<float, 4>{0.1f, 0.2f, 0.6f, 1.0f}}
        };

        command_buffer.beginRenderPass(
            vk::RenderPassBeginInfo{
                .renderPass = m_pass,
                .framebuffer = m_framebuffer,
                .renderArea = {
                    .offset = {
                        .x = 0,
                        .y = 0,
                    },
                    .extent = {
                        .width = m_attachment_width,
                        .height = m_attachment_height,
                    },
                },
                .clearValueCount = 1,
                .pClearValues = &clear_value,
            },
            vk::SubpassContents::eInline);

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

        command_buffer.draw(3, 1, 0, 0);
        command_buffer.endRenderPass();

        command_buffer.end();
    }

    std::once_flag m_create_buffer_flag;
    std::once_flag m_resources_creation_flag;

    avk::pipeline_layout m_layout;
    avk::command_pool m_command_pool;
    avk::command_buffer_list m_command_buffer;

    avk::framebuffer m_framebuffer;
    avk::render_pass m_pass;
    avk::vma_image m_attachment;
    avk::image_view m_attachment_view;

    avk::graphics_pipeline m_pipeline;

    avk::shader_module m_vertex_shader;
    avk::shader_module m_fragment_shader;

    avk::semaphore m_semaphore;

    size_t m_attachment_width;
    size_t m_attachment_height;
};

int main(int argv, const char** argc)
{
    auto dummy_listener = std::make_unique<hello_triangle_listener>();

    sandbox::hal::window window(800, 600, "sandbox window", std::move(dummy_listener));

    while (!window.closed()) {
        window.main_loop();
    }

    return 0;
}
