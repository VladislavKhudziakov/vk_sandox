
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <filesystem/common_file.hpp>
#include <sample_app.hpp>
#include <render/vk/errors_handling.hpp>
#include <render/vk/utils.hpp>
#include <render/vk/resources.hpp>
#include <gltf/vk_utils.hpp>

#include <renderdoc/renderdoc.hpp>

using namespace sandbox;
using namespace sandbox::hal;
using namespace sandbox::hal::render;


#include <glm/gtx/string_cast.hpp>

class test_sample_app : public sandbox::sample_app
{
public:
    explicit test_sample_app(const std::string& gltf_file)
        : m_model(gltf::model::from_url(gltf_file))
    {
        m_anim_controller.emplace(m_model.get_animations().front());
        m_anim_controller->play();
    }


protected:
    void update(uint64_t dt) override
    {
        m_curr_progression.y += dt;
        //m_curr_progression.y = std::min(m_curr_progression.y, int32_t(m_model.get_animations().front().get_duration() * 1e6));
        m_curr_progression.y = m_curr_progression.y % int32_t(m_model.get_animations().front().get_duration() * 1e6);

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
        m_pass = avk::render_pass_builder()
                     .begin_sub_pass(vk::SampleCountFlagBits::e1)
                     .add_color_attachment(
                         {0, 0},
                         {1.0f, 1.0f},
                         vk::Format::eR8G8B8A8Srgb,
                         vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eColorAttachmentOptimal,
                         vk::ImageLayout::eTransferSrcOptimal,
                         std::array{0.2f, 0.3f, 0.6f, 1.0f})
                     .set_depth_stencil_attachment(
                         {0, 0},
                         {1.0f, 1.0f},
                         vk::Format::eD24UnormS8Uint,
                         vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eDepthStencilAttachmentOptimal,
                         vk::ImageLayout::eDepthStencilAttachmentOptimal,
                         1.0f)
                     .finish_sub_pass()
                     .add_sub_pass_dependency({
                         .srcSubpass = VK_SUBPASS_EXTERNAL,
                         .dstSubpass = 0,
                         .srcStageMask = vk::PipelineStageFlagBits::eTransfer,
                         .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                         .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                         .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead,
                         .dependencyFlags = {},
                     })
                     .create_pass();
    }

    void create_framebuffers(uint32_t width, uint32_t height) override
    {
        m_pass.resize(avk::context::queue_family(vk::QueueFlagBits::eGraphics), width, height);
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

        m_geometry = gltf::vk_model_builder()
                         .set_vertex_format(
                             {vk::Format::eR32G32B32Sfloat,
                              vk::Format::eR32G32B32Sfloat,
                              vk::Format::eR32G32B32A32Sfloat,
                              vk::Format::eR32G32Sfloat,
                              vk::Format::eR32G32Sfloat,
                              vk::Format::eR32G32B32Sfloat,
                              vk::Format::eR32G32B32A32Uint,
                              vk::Format::eR32G32B32A32Sfloat})
                         .create(m_model, m_vertex_buffer_pool);

        m_texture_atlas = gltf::vk_texture_atlas::from_gltf_model(m_model, curr_buffer, avk::context::queue_family(vk::QueueFlagBits::eGraphics));

        m_hierarchy_buffer = m_vertex_buffer_pool.get_builder()
                                 .set_size(m_model.get_nodes().size() * sizeof(glm::mat4))
                                 .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
                                 .create();

        m_anim_progression_buffer = m_vertex_buffer_pool.get_builder()
                                        .set_size(sizeof(glm::ivec4))
                                        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
                                        .create();

        m_uniform_buffer = m_vertex_buffer_pool.get_builder()
                               .set_size(sizeof(gltf::instance_transform_data))
                               .set_usage(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst)
                               .create();

        m_vertex_buffer_pool.flush(avk::context::queue_family(vk::QueueFlagBits::eGraphics), curr_buffer);

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

        init_pipelines();

        VK_CALL(avk::context::device()->waitForFences({fence}, VK_TRUE, UINT64_MAX));
    }

    vk::Image get_final_image() override
    {
        return m_pass.get_framebuffer_image(0);
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

        if (!m_comp_shader) {
            create_shader(m_comp_shader, "./resources/skinning.comp.spv");
        }

        auto builder = avk::pipeline_builder();
        auto as = m_model.get_animations().size();
        auto ns = m_model.get_nodes().size();
        builder.set_shader_stages({{m_comp_shader, vk::ShaderStageFlagBits::eCompute}})
            .add_specialization_constant<uint32_t>(m_model.get_animations().size())
            .add_specialization_constant<uint32_t>(m_model.get_nodes().size())
            .add_specialization_constant<uint32_t>(1)
            .begin_descriptor_set()
            .add_buffer(m_anim_progression_buffer, vk::DescriptorType::eStorageBuffer)
            .add_buffer(m_geometry.get_animations().front().get_meta_buffer(), vk::DescriptorType::eStorageBuffer)
            .add_buffer(m_geometry.get_animations().front().get_keys_buffer(), vk::DescriptorType::eStorageBuffer)
            .add_buffer(m_geometry.get_animations().front().get_exec_order_buffer(), vk::DescriptorType::eStorageBuffer)
            .add_buffer(m_geometry.get_animations().front().get_nodes_buffer(), vk::DescriptorType::eStorageBuffer)
            .add_buffer(m_hierarchy_buffer, vk::DescriptorType::eStorageBuffer);

        m_animation_pipeline = builder.create_compute_pipeline();

        for (const auto& vk_mesh : m_geometry.get_meshes()) {
            const auto& materials = m_model.get_materials();

            const auto& vk_primitives = vk_mesh.get_primitives();
            auto curr_vk_primitive = vk_primitives.begin();

            int mesh_id = 0;
            for (const auto& primitive : vk_mesh.get_primitives()) {
                avk::pipeline_builder builder{};

                builder.set_vertex_format(m_geometry.get_vertex_format())
                    .set_shader_stages({{m_vertex_shader, vk::ShaderStageFlagBits::eVertex}, {m_fragment_shader, vk::ShaderStageFlagBits::eFragment}})
                    .add_blend_state()
                    .add_push_constant(vk::ShaderStageFlagBits::eVertex, uint32_t(0))
                    .add_specialization_constant(uint32_t(1))                                       // use hierarchy
                    .add_specialization_constant(uint32_t(vk_mesh.is_skinned()))                    // use skin
                    .add_specialization_constant(uint32_t(vk_mesh.get_skin().get_hierarchy_size())) // hierarchy size
                    .add_specialization_constant(uint32_t(vk_mesh.get_skin().get_joints_count()))   // skin size
                    .begin_descriptor_set()
                    .add_buffer(m_uniform_buffer, vk::DescriptorType::eUniformBuffer)
                    .add_buffer(m_hierarchy_buffer, vk::DescriptorType::eStorageBuffer)
                    .add_buffer(vk_mesh.get_skin().get_joints_buffer(), vk::DescriptorType::eUniformBuffer);

                builder.finish_descriptor_set();

                m_models_primitives_pipelines[mesh_id].emplace_back(builder.create_graphics_pipeline(m_pass.get_native_pass(), 0));
                curr_vk_primitive++;
            }
        }
    }


    void write_command_buffers()
    {
        auto& curr_camera = m_model.get_cameras().back();

        auto view_matrix = glm::lookAt(glm::vec3{0.75, 0.5, 1}, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});

        auto [fb_width, fb_height] = get_window_framebuffer_size();

        auto proj_matrix = curr_camera.calculate_projection(float(fb_width) / float(fb_height));

        glm::mat4 model_matrix{1};
        model_matrix = glm::translate(model_matrix, {1, 2, 3});
        gltf::instance_transform_data istance_transform{
            .model = model_matrix,
            .view = view_matrix,
            .proj = proj_matrix,
            .mvp = proj_matrix * view_matrix};

        vk::CommandBuffer& command_buffer = m_command_buffer->front();
        command_buffer.reset();

        command_buffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse});

        m_animation_pipeline.activate(command_buffer);

        m_anim_progression_buffer.upload([this](uint8_t* dst) {
            std::memcpy(dst, glm::value_ptr(m_curr_progression), sizeof(m_curr_progression));
        });

        m_uniform_buffer.upload([istance_transform](uint8_t* dst) {
            std::memcpy(dst, &istance_transform, sizeof(istance_transform));
        });

        m_vertex_buffer_pool.update(command_buffer);

        command_buffer.dispatch(256, 1, 1);

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eVertexShader,
            {},
            {vk::MemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            }},
            {},
            {});

        m_pass.begin(command_buffer);

        int node_id = 0;
        for (const auto& mesh : m_geometry.get_meshes()) {
            auto curr_pipeline = m_models_primitives_pipelines[node_id].begin();
            for (const auto& primitive : m_geometry.get_meshes()[node_id++].get_primitives()) {
                curr_pipeline++->activate(command_buffer);
                gltf::draw_primitive(primitive, command_buffer);
            }
        }

        m_pass.finish(command_buffer);
        command_buffer.end();
    }

    gltf::model m_model{};
    gltf::vk_model m_geometry{};
    gltf::vk_texture_atlas m_texture_atlas{};

    std::optional<gltf::cpu_animation_controller> m_anim_controller{};

    avk::pipeline_instance m_animation_pipeline{};
    avk::render_pass_instance m_pass{};

    std::vector<avk::semaphore> m_semaphores{};
    std::vector<vk::Semaphore> m_native_semaphores{};
    std::vector<avk::fence> m_fences{};
    std::vector<vk::Fence> m_native_fences{};

    avk::command_pool m_command_pool{};
    avk::command_buffer_list m_command_buffer{};

    avk::buffer_pool m_vertex_buffer_pool{};

    avk::shader_module m_vertex_shader{};
    avk::shader_module m_fragment_shader{};
    avk::shader_module m_comp_shader{};
    std::unordered_map<int32_t, std::vector<avk::pipeline_instance>> m_models_primitives_pipelines{};

    avk::buffer_instance m_anim_progression_buffer{};
    avk::buffer_instance m_uniform_buffer{};
    avk::buffer_instance m_hierarchy_buffer{};
    glm::ivec4 m_curr_progression{0, 0, 0, 0};

    bool m_reset_command_buffer = false;
};

int main(int argv, const char** argc)
{
    test_sample_app app{R"(D:\dev\glTF-Sample-Models\2.0\CesiumMan\glTF\CesiumMan.gltf)"};
    app.main_loop();

    return 0;
}
