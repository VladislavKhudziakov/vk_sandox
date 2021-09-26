
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <filesystem/common_file.hpp>
#include <sample_app.hpp>
#include <render/vk/errors_handling.hpp>
#include <render/vk/utils.hpp>
#include <gltf/vk_utils.hpp>

#include <renderdoc/renderdoc.hpp>

using namespace sandbox;
using namespace sandbox::hal;
using namespace sandbox::hal::render;


class test_sample_app : public sandbox::sample_app
{
public:
    explicit test_sample_app(const std::string& gltf_file)
        : m_model(gltf::model::from_url(gltf_file))
    {
        if (!m_model.get_animations().empty()) {
            m_anim_controller.emplace(m_model.get_animations().front());
            m_anim_controller->play();
        }
    }


protected:
    void update(uint64_t dt) override
    {
        if (m_anim_controller) {
            m_anim_controller->update(m_model, dt);
        }

        write_command_buffers();

        vk::Queue queue = avk::context::queue(vk::QueueFlagBits::eGraphics, 0);

        queue.submit(vk::SubmitInfo{
                         .commandBufferCount = static_cast<uint32_t>(m_command_buffer->size()),
                         .pCommandBuffers = m_command_buffer->data(),
                         .signalSemaphoreCount = static_cast<uint32_t>(m_native_semaphores.size()),
                         .pSignalSemaphores = m_native_semaphores.data(),
                     },
                     m_fences.back().as<vk::Fence>());

        sample_app::update(dt);
    }

    void create_render_passes() override
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

    void create_framebuffers(uint32_t width, uint32_t height) override
    {
        uint32_t queue_family = avk::context::queue_family(vk::QueueFlagBits::eGraphics);

        auto [framebuffer, attachments, attachments_views] = avk::gen_framebuffer(
            width, height, queue_family, m_pass.get_info(), m_pass.get_native_pass(), {}, {vk::ImageUsageFlagBits::eTransferSrc});

        m_framebuffer = std::move(framebuffer);
        m_attachments = std::move(attachments);
        m_attachments_views = std::move(attachments_views);

        m_reset_command_buffer = true;
    }

    void create_sync_primitives() override
    {
        m_semaphores.emplace_back(avk::create_semaphore(avk::context::device()->createSemaphore({})));
        m_fences.emplace_back(avk::create_fence(avk::context::device()->createFence({})));
        m_native_semaphores = avk::to_elements_list<vk::Semaphore>(m_semaphores.begin(), m_semaphores.end());
        m_native_fences = avk::to_elements_list<vk::Fence>(m_fences.begin(), m_fences.end());
    }

    void init_assets() override
    {
        m_command_pool = avk::create_command_pool(
            avk::context::device()->createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = avk::context::queue_family(vk::QueueFlagBits::eGraphics),
            }));

        m_command_buffer = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
            .commandPool = m_command_pool,
            .commandBufferCount = 1,
        });

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

        m_geometry = gltf::vk_geometry::from_gltf_model(m_model, curr_buffer, avk::context::queue_family(vk::QueueFlagBits::eGraphics));
        m_texture_atlas = gltf::vk_texture_atlas::from_gltf_model(m_model, curr_buffer, avk::context::queue_family(vk::QueueFlagBits::eGraphics));
        m_skins = gltf::vk_geometry_skins::from_gltf_model(m_model, curr_buffer, avk::context::queue_family(vk::QueueFlagBits::eGraphics));

        auto transforms_buffer_size = m_model.get_meshes().size() * sizeof(gltf::instance_transform_data);
        auto [uniform_staging_buffer, uniform_buffer] = avk::gen_buffer(curr_buffer, avk::context::queue_family(vk::QueueFlagBits::eGraphics), transforms_buffer_size, vk::BufferUsageFlagBits::eUniformBuffer);

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

        m_uniform_staging_buffer = std::move(uniform_staging_buffer);
        m_uniform_buffer = std::move(uniform_buffer);

        init_pipelines();

        VK_CALL(avk::context::device()->waitForFences({fence}, VK_TRUE, UINT64_MAX));
    }

    vk::Image get_final_image() override
    {
        return m_attachments.front().as<vk::Image>();
    }

    const std::vector<vk::Semaphore>& get_wait_semaphores() override
    {
        return m_native_semaphores;
    }

    const std::vector<vk::Fence>& get_wait_fences() override
    {
        return m_native_fences;
    }

private:
    struct pipeline_data
    {
        avk::pipeline_layout layout{};
        avk::descriptor_pool pool{};
        std::vector<avk::descriptor_set_layout> descriptor_layouts{};
        avk::descriptor_set_list descriptors{};

        avk::graphics_pipeline pipeline{};
    };

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

    void init_pipelines()
    {
        if (!m_vertex_shader) {
            create_shader(m_vertex_shader, "./resources/test.vert.spv");
        }

        if (!m_fragment_shader) {
            create_shader(m_fragment_shader, "./resources/test.frag.spv");
        }

        sandbox::gltf::for_each_scene_node(m_model, [this](const gltf::node& node, int32_t node_index) {
            if (node.get_mesh() < 0) {
                return;
            }

            const auto& mesh = m_model.get_meshes()[node.get_mesh()];
            const auto& materials = m_model.get_materials();

            const auto& vk_primitives = m_geometry.get_primitives(node.get_mesh());
            auto curr_vk_primitive = vk_primitives.begin();
            assert(vk_primitives.size() == mesh.get_primitives().size());

            for (const auto& primitive : mesh.get_primitives()) {
                pipeline_data curr_pipeline_data{};
                const gltf::material& curr_material = primitive.get_material() < 0 ? materials.back() : materials[primitive.get_material()];
                const gltf::vk_skin& curr_skin = node.get_skin() < 0 ? m_skins.get_skins().back() : m_skins.get_skins()[node.get_skin()];

                avk::graphics_pipeline_builder builder(m_pass.get_native_pass(), 0, 1);

                builder.set_vertex_format(curr_vk_primitive->attributes, curr_vk_primitive->bindings)
                .set_shader_stages({{m_vertex_shader, vk::ShaderStageFlagBits::eVertex}, {m_fragment_shader, vk::ShaderStageFlagBits::eFragment}})
                .add_push_constant(vk::ShaderStageFlagBits::eVertex, uint32_t(node_index))
                .add_specialization_constant(uint32_t(1)) // use hierarchy
                .add_specialization_constant(uint32_t(0)) // use skin
                .add_specialization_constant(uint32_t(m_skins.get_hierarchy_transforms().size())) // hierarchy size
                .add_specialization_constant(1) // skin size
                .add_buffer(m_uniform_buffer.as<vk::Buffer>(), node.get_mesh() * sizeof(gltf::instance_transform_data), sizeof(gltf::instance_transform_data), vk::DescriptorType::eUniformBuffer)
                .add_buffer(m_skins.get_hierarchy_buffer().as<vk::Buffer>(), 0, m_skins.get_hierarchy_transforms().size() * sizeof(glm::mat4), vk::DescriptorType::eUniformBuffer)
                .add_buffer(m_skins.get_skin_buffer().as<vk::Buffer>(), curr_skin.offset, curr_skin.size, vk::DescriptorType::eUniformBuffer);

                curr_material.for_each_texture([this, &builder](const gltf::material::texture_data& tex_data) {
                    const auto& vk_tex = m_texture_atlas.get_texture(tex_data.index);
                    builder.add_texture(vk_tex.image_view, vk_tex.sampler);
                });

                m_models_primitives_pipelines[node.get_mesh()].emplace_back(builder.create_pipeline());
                curr_vk_primitive++;
            }
        });
    }


    void write_command_buffers()
    {
        auto& curr_camera = m_model.get_cameras().back();

        std::vector<gltf::instance_transform_data> instance_transforms{};
        instance_transforms.reserve(m_model.get_meshes().size());

        auto view_matrix = glm::lookAt(glm::vec3{3, 3, 3}, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});

        auto [fb_width, fb_height] = get_window_framebuffer_size();

        auto proj_matrix = curr_camera.calculate_projection(float(fb_width) / float(fb_height));


        for (const auto& mesh : m_model.get_meshes()) {
            instance_transforms.emplace_back(gltf::instance_transform_data{
                .view = view_matrix,
                .proj = proj_matrix,
                .mvp = proj_matrix * view_matrix});
        }

        vk::CommandBuffer& command_buffer = m_command_buffer->front();
        command_buffer.reset();

        command_buffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse});

        avk::upload_buffer_data(
            command_buffer,
            m_uniform_staging_buffer,
            m_uniform_buffer,
            vk::PipelineStageFlagBits::eVertexShader,
            vk::AccessFlagBits::eUniformRead,
            instance_transforms.size() * sizeof(instance_transforms.front()),
            [&instance_transforms](const uint8_t* dst) {
                std::memcpy((void*) dst, instance_transforms.data(), instance_transforms.size() * sizeof(instance_transforms.front()));
            });

        if (m_anim_controller) {
            avk::upload_buffer_data(
                command_buffer,
                m_skins.get_hierarchy_staging_buffer(),
                m_skins.get_hierarchy_buffer(),
                vk::PipelineStageFlagBits::eVertexShader,
                vk::AccessFlagBits::eUniformRead,
                m_anim_controller->get_transformations().size() * sizeof(m_anim_controller->get_transformations().front()),
                [this](const uint8_t* dst) {
                    std::memcpy(
                        (void*) dst,
                        m_anim_controller->get_transformations().data(),
                        m_anim_controller->get_transformations().size() * sizeof(m_anim_controller->get_transformations().front()));
                });
        }

        command_buffer.setViewport(
            0,
            {vk::Viewport{
                .x = 0,
                .y = 0,
                .width = static_cast<float>(fb_width),
                .height = static_cast<float>(fb_height),
                .minDepth = 0,
                .maxDepth = 1}});

        command_buffer.setScissor(
            0, {vk::Rect2D{
                   .offset = {.x = 0, .y = 0},
                   .extent = {
                       .width = static_cast<uint32_t>(fb_width),
                       .height = static_cast<uint32_t>(fb_height),
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
                        .width = static_cast<uint32_t>(fb_width),
                        .height = static_cast<uint32_t>(fb_height),
                    },
                },
                .clearValueCount = std::size(clear),
                .pClearValues = clear,
            },
            vk::SubpassContents::eInline);

        for_each_scene_node(m_model, [this, &instance_transforms, &command_buffer](const gltf::node& node, int32_t node_index) {
            if (node.get_mesh() < 0) {
                return;
            }

            auto curr_pipeline = m_models_primitives_pipelines[node.get_mesh()].begin();

            for (const auto& primitive : m_geometry.get_primitives(node.get_mesh())) {
                curr_pipeline++->activate(command_buffer);
                gltf::draw_primitive(primitive, m_geometry.get_vertex_buffer(), m_geometry.get_index_buffer(), command_buffer);
            }
        });

        command_buffer.endRenderPass();
        command_buffer.end();
    }

    gltf::model m_model{};
    gltf::vk_geometry m_geometry{};
    gltf::vk_texture_atlas m_texture_atlas{};
    gltf::vk_geometry_skins m_skins{};

    avk::render_pass_wrapper m_pass{};

    avk::framebuffer m_framebuffer{};
    std::vector<avk::vma_image> m_attachments{};
    std::vector<avk::image_view> m_attachments_views{};

    std::vector<avk::semaphore> m_semaphores{};
    std::vector<vk::Semaphore> m_native_semaphores{};
    std::vector<avk::fence> m_fences{};
    std::vector<vk::Fence> m_native_fences{};

    avk::command_pool m_command_pool{};
    avk::command_buffer_list m_command_buffer{};

    avk::shader_module m_vertex_shader{};
    avk::shader_module m_fragment_shader{};

    std::unordered_map<int32_t, std::vector<avk::_graphics_pipeline>> m_models_primitives_pipelines{};

    std::optional<gltf::cpu_animation_controller> m_anim_controller{};

    avk::vma_buffer m_uniform_staging_buffer{};
    avk::vma_buffer m_uniform_buffer{};

    bool m_reset_command_buffer = false;
};


int main(int argv, const char** argc)
{
    test_sample_app app{"D:\\dev\\glTF-Sample-Models\\2.0\\AnimatedCube\\glTF\\AnimatedCube.gltf"};
    app.main_loop();

    return 0;
}
