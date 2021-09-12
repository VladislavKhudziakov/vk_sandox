
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <filesystem/common_file.hpp>

#include <window/window.hpp>
#include <window/vk_main_loop_update_listener.hpp>

#include <render/vk/errors_handling.hpp>
#include <render/vk/utils.hpp>

#include "../utils/gltf_vk_primitive_renderer.hpp"

#include <gltf/vk_utils.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <renderdoc/renderdoc.hpp>

using namespace sandbox;
using namespace sandbox::hal;
using namespace sandbox::hal::render;

class test_renderer : public samples::gltf_vk_primitive_renderer
{
public:
    ~test_renderer() override = default;

    void create_framebuffer(uint32_t width, uint32_t height)
    {
        uint32_t queue_family = avk::context::queue_family(vk::QueueFlagBits::eGraphics);

        auto [framebuffer, attachments, attachments_views] = avk::gen_framebuffer(
            width, height, queue_family, m_pass.get_info(), m_pass.get_native_pass(), {}, {vk::ImageUsageFlagBits::eTransferSrc});

        m_framebuffer = std::move(framebuffer);
        m_attachments = std::move(attachments);
        m_attachments_views = std::move(attachments_views);

        m_framebuffer_width = width;
        m_framebuffer_height = height;

        toggle_command_buffers_rewrite();
    }

    void create_pass()
    {
        // clang-format off
        avk::pass_create_info info{
            .attachments = {
                {
                    .flags = {},
                    .format = vk::Format::eR8G8B8A8Srgb,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .initialLayout = vk::ImageLayout::eUndefined,
                    .finalLayout = vk::ImageLayout::eTransferSrcOptimal,
                },
                {
                    .flags = {},
                    .format = vk::Format::eD24UnormS8Uint,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eUndefined,
                    .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                }
            },
            .attachments_refs = {
                {
                     .attachment = 0,
                     .layout = vk::ImageLayout::eColorAttachmentOptimal,
                },
                {
                    .attachment = 1,
                    .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                }
            },
            .subpasses = {
                {
                    .flags = {},
                    .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &info.attachments_refs[0],
                    .pDepthStencilAttachment = &info.attachments_refs[1]
                }
            },
            .subpass_dependencies = {
                {
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = vk::PipelineStageFlagBits::eTransfer,
                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead,
                    .dependencyFlags = {},
                }
            }
        };
        // clang-format on
        m_pass = avk::render_pass_wrapper(std::move(info));
    }

    vk::Image get_draw_attachment() const
    {
        return m_attachments.front().as<vk::Image>();
    }

    std::pair<size_t, size_t> get_framebuffer_size() const
    {
        return {m_framebuffer_width, m_framebuffer_height};
    }

    const avk::command_buffer_list& get_command_buffers() const
    {
        return m_command_buffer;
    }

protected:
    void create_shader(avk::shader_module& module, const std::string& path)
    {
        if (module) {
            return;
        }

        sandbox::hal::filesystem::common_file file{};
        file.open(path);
        assert(file.get_size());

        module = avk::create_shader_module(avk::context::device()->createShaderModule(vk::ShaderModuleCreateInfo{
            .flags = {},
            .codeSize = file.get_size(),
            .pCode = reinterpret_cast<const uint32_t*>(file.read_all().get_data())}));
    }


    void create_pipelines(const gltf::model& model, uint32_t scene) override
    {
        if (!m_vertex_shader) {
            create_shader(m_vertex_shader, "./resources/test.vert.spv");
        }

        if (!m_fragment_shader) {
            create_shader(m_fragment_shader, "./resources/test.frag.spv");
        }

        vk::PushConstantRange push_constant_range{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(glm::mat4)};

        if (!m_pipeline_layout) {
            m_pipeline_layout = avk::create_pipeline_layout(avk::context::device()->createPipelineLayout(vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 0,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_constant_range}));
        }

        m_model_primitive_pipelines.clear();

        for_each_scene_node(model, [this, &model](const gltf::node& node) {
            if (node.mesh < 0) {
                return;
            }

            const auto& mesh = model.get_meshes()[node.mesh];
            const auto& materials = model.get_materials();

            for (const auto& primitive : mesh->get_primitives()) {
                const auto& primitive_impl = static_cast<const gltf::gltf_vk::primitive&>(*primitive);

                avk::descriptor_set_layout textures_layout;

                if (primitive_impl.get_material() < 0) {
                    textures_layout = gltf::create_material_textures_layout(*materials.back());
                } else {
                    textures_layout = gltf::create_material_textures_layout(*materials[primitive->get_material()]);
                }

                auto [pool, sets] = avk::gen_descriptor_sets({textures_layout}, {{1u, vk::DescriptorType::eSampledImage}});
                gltf::write_material_textures_descriptors(*materials[primitive->get_material()], sets->at(0), model.get_textures(), model.get_images());

                auto pipeline_layout = avk::gen_pipeline_layout({}, {textures_layout});

                m_model_primitive_pipelines[node.mesh].emplace_back(gltf::create_pipeline_from_primitive(
                    *primitive,
                    {
                        {m_vertex_shader, vk::ShaderStageFlagBits::eVertex},
                        {m_fragment_shader, vk::ShaderStageFlagBits::eFragment}
                    },
                    m_pipeline_layout,
                    m_pass.get_native_pass(),
                    0));
            }
        });
    }


    void write_command_buffers(
        const gltf::model& model,
        uint32_t scene,
        const vk::Buffer& vertex_buffer,
        const vk::Buffer& index_buffer) override
    {
        if (!m_command_pool) {
            m_command_pool = avk::create_command_pool(
                avk::context::device()->createCommandPool(vk::CommandPoolCreateInfo{
                    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                    .queueFamilyIndex = avk::context::queue_family(vk::QueueFlagBits::eGraphics),
                }));

            m_command_buffer = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
                .commandPool = m_command_pool,
                .commandBufferCount = 1,
            });
        }

        auto& curr_camera = *model.get_cameras().back();
        auto& camera_data = curr_camera.get_data<gltf::camera::perspective>();

        struct instance_transform_data
        {
            glm::mat4 model;
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 mvp;
        };

        std::unordered_map<int32_t, instance_transform_data> models_transforms;

        auto view_matrix = glm::lookAt(glm::vec3{3, 3, 3}, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});

        auto proj_matrix = glm::infinitePerspective(
            camera_data.yfov,
            camera_data.aspect_ratio == 0.0f ? float(m_framebuffer_width) / float(m_framebuffer_height) : camera_data.aspect_ratio,
            0.01f);

        for_each_scene_node(
            model,
            [&proj_matrix, &view_matrix, &models_transforms](const gltf::node& node, const glm::mat4& parent_transform) {
                auto local_transform = glm::mat4{1};

                if (node.matrix) {
                    local_transform = node.matrix.value();
                }

                if (node.transform) {
                    local_transform = glm::translate(local_transform, node.transform->translation);
                    local_transform *= glm::mat4_cast(node.transform->rotation);
                    local_transform = glm::scale(local_transform, node.transform->scale);
                }

                auto world_transform = parent_transform * local_transform;

                if (node.mesh >= 0) {
                    models_transforms[node.mesh] = {
                        .model = world_transform,
                        .view = view_matrix,
                        .proj = proj_matrix,
                        .mvp = proj_matrix * view_matrix * world_transform};
                }

                return world_transform;
            },
            glm::mat4{1});

        vk::CommandBuffer& command_buffer = m_command_buffer->front();
        command_buffer.reset();

        command_buffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse});

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
                       .width = static_cast<uint32_t>(m_framebuffer_width),
                       .height = static_cast<uint32_t>(m_framebuffer_height),
                   },
               }});

        vk::ClearValue clear[]{
            vk::ClearColorValue{std::array<float, 4>{0.1f, 0.2f, 0.6f, 1.0f}},
            vk::ClearColorValue{std::array<float, 4>{1.0f}}};

        command_buffer.beginRenderPass(
            vk::RenderPassBeginInfo{
                .renderPass = m_pass.get_native_pass(),
                .framebuffer = m_framebuffer,
                .renderArea = {
                    .offset = {
                        .x = 0,
                        .y = 0,
                    },
                    .extent = {
                        .width = static_cast<uint32_t>(m_framebuffer_width),
                        .height = static_cast<uint32_t>(m_framebuffer_height),
                    },
                },
                .clearValueCount = std::size(clear),
                .pClearValues = clear,
            },
            vk::SubpassContents::eInline);

        for_each_scene_node(model, [this, &models_transforms, &command_buffer, &model, vertex_buffer, index_buffer](const gltf::node& node) {
            if (node.mesh < 0) {
                return;
            }

            auto& mesh = model.get_meshes()[node.mesh];

            command_buffer.pushConstants(
                m_pipeline_layout,
                vk::ShaderStageFlagBits::eVertex,
                0,
                sizeof(glm::mat4),
                glm::value_ptr(models_transforms[node.mesh].mvp));

            auto curr_pipeline = m_model_primitive_pipelines[node.mesh].begin();

            for (const auto& primitive : mesh->get_primitives()) {
                command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *curr_pipeline++);
                gltf::draw_primitive(*primitive, vertex_buffer, index_buffer, command_buffer);
            }
        });

        command_buffer.endRenderPass();
        command_buffer.end();
    }

private:
    avk::render_pass_wrapper m_pass{};

    avk::framebuffer m_framebuffer{};
    std::vector<avk::vma_image> m_attachments{};
    std::vector<avk::image_view> m_attachments_views{};

    avk::command_pool m_command_pool{};
    avk::command_buffer_list m_command_buffer{};

    size_t m_framebuffer_width{0};
    size_t m_framebuffer_height{0};

    avk::shader_module m_vertex_shader{};
    avk::shader_module m_fragment_shader{};
    avk::pipeline_layout m_pipeline_layout{};

    std::unordered_map<int32_t, std::vector<avk::graphics_pipeline>> m_model_primitive_pipelines{};
};


class gltf_test_listener : public hal::vk_main_loop_update_listener
{
public:
    explicit gltf_test_listener(gltf::gltf_vk model)
        : m_model(std::move(model))
    {
    }

    void on_swapchain_reset(size_t width, size_t height) override
    {
        m_renderer.create_framebuffer(width, height);
    }

    vk::Image get_present_image() const override
    {
        return m_renderer.get_draw_attachment();
    }

    vk::ImageLayout get_present_image_layout() const override
    {
        return vk::ImageLayout::eTransferSrcOptimal;
    }

    vk::Extent2D get_present_image_size() const override
    {
        return {
            static_cast<uint32_t>(m_renderer.get_framebuffer_size().first),
            static_cast<uint32_t>(m_renderer.get_framebuffer_size().second)};
    }

    std::vector<vk::Semaphore> get_wait_semaphores() const override
    {
        return {m_semaphore};
    }

    void on_window_initialized() override
    {
        avk::command_pool pool = avk::create_command_pool(
            avk::context::device()->createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = avk::context::queue_family(vk::QueueFlagBits::eGraphics),
            }));

        auto resources_buffer = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
            .commandPool = pool,
            .commandBufferCount = 1,
        });

        auto curr_buffer = resources_buffer->front();

        curr_buffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        });

        m_model.create_resources(curr_buffer, avk::context::queue_family(vk::QueueFlagBits::eGraphics));

        curr_buffer.end();

        auto graphics_queue = avk::context::queue(vk::QueueFlagBits::eGraphics, 0);

        auto fence = avk::create_fence(avk::context::device()->createFence({}));

        graphics_queue.submit(
            vk::SubmitInfo{
                .waitSemaphoreCount = 0,
                .commandBufferCount = static_cast<uint32_t>(resources_buffer->size()),
                .pCommandBuffers = resources_buffer->data(),
            },
            fence);

        VK_CALL(avk::context::device()->waitForFences({fence}, VK_TRUE, UINT64_MAX));

        m_renderer.create_pass();
    }

    void update(uint64_t dt) override
    {
        if (!m_semaphore) {
            m_semaphore = avk::create_semaphore(avk::context::device()->createSemaphore({}));
        }

        m_model.draw(m_renderer);

        vk::Queue queue = avk::context::queue(vk::QueueFlagBits::eGraphics, 0);
        queue.submit(vk::SubmitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = m_renderer.get_command_buffers()->data(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = m_semaphore.handler_ptr(),
        });
    }

private:
    test_renderer m_renderer{};
    gltf::gltf_vk m_model{};

    avk::semaphore m_semaphore{};
};


int main(int argv, const char** argc)
{
    RDOC_INIT_SCOPE();

    gltf::gltf_vk gltf = gltf::gltf_vk::from_url("./resources/Box.glb");

    auto dummy_listener = std::make_unique<gltf_test_listener>(std::move(gltf));

    sandbox::hal::window window(800, 600, "sandbox window", std::move(dummy_listener));

    while (!window.closed()) {
        window.main_loop();
    }

    return 0;
}
