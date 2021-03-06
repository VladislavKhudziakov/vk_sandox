
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


class test_sample_app : public sandbox::sample_app
{
public:
    explicit test_sample_app(const std::string& gltf_file)
        : m_model(gltf::model::from_url(gltf_file))
    {
    }


protected:
    void update(uint64_t dt) override
    {
        write_command_buffers(dt);
          
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
                     .begin_sub_pass()
                     .begin_attachment(vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal)
                     .set_attachmen_clear({0.2f, 0.3f, 0.6f, 1.0f})
                     .finish_attachment()
                     .begin_attachment(vk::Format::eD24UnormS8Uint, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eDepthStencilAttachmentOptimal)
                     .set_attachmen_clear(avk::render_pass_builder::depth_clear{1.0f, 0u})
                     .finish_attachment()
                     .finish_sub_pass()
                     .add_sub_pass_dependency({
                         .srcSubpass = 0,
                         .dstSubpass = VK_SUBPASS_EXTERNAL,
                         .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                         .dstStageMask = vk::PipelineStageFlagBits::eTransfer,
                         .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                         .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                         .dependencyFlags = {},
                     })
                     .create_pass(avk::context::queue_family(vk::QueueFlagBits::eGraphics));
    }

    void create_framebuffers(uint32_t width, uint32_t height) override
    {
        m_pass.resize(width, height);
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
                         .create(m_model, m_buffer_pool, m_image_pool);

        m_uniform_buffer =
            m_buffer_pool.get_builder()
                .set_size(sizeof(gltf::instance_transform_data))
                .set_usage(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst)
                .create();

        m_animation_controller = gltf::animation_controller(m_model, m_geometry);

        m_animation_controller.init_resources(m_buffer_pool, 1);
        m_anim_instance = m_animation_controller.instantiate_animation();
        m_anim_instance->play();

        const auto buffer_pool_future = m_buffer_pool.submit(vk::QueueFlagBits::eGraphics);
        const auto image_pool_future = m_image_pool.submit(vk::QueueFlagBits::eGraphics);

        init_pipelines();
    }

    vk::Image get_final_image() override
    {
        return m_pass.get_subpass().get_color_attachment(0, m_pass);
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
            create_shader(m_vertex_shader, WORK_DIR "/resources/test.vert.spv");
        }

        if (!m_fragment_shader) {
            create_shader(m_fragment_shader, WORK_DIR "/resources/test.frag.spv");
        }

        m_animation_controller.init_pipelines();

        for (const auto& mesh : m_geometry.get_meshes()) {
            const auto& primitives = mesh.get_primitives();
            auto curr_primitive = primitives.begin();

            int mesh_id = 0;
            for (const auto& primitive : mesh.get_primitives()) {
                avk::pipeline_builder builder{};

                const auto& mat = curr_primitive->get_material(m_geometry);

                builder.set_vertex_format(m_geometry.get_vertex_format())
                    .set_shader_stages({{m_vertex_shader, vk::ShaderStageFlagBits::eVertex}, {m_fragment_shader, vk::ShaderStageFlagBits::eFragment}})
                    .add_blend_state()
                    .add_push_constant(vk::ShaderStageFlagBits::eVertex, uint32_t(0))
                    .add_specialization_constant(uint32_t(1))                                    // use hierarchy
                    .add_specialization_constant(uint32_t(mesh.is_skinned()))                    // use skin
                    .add_specialization_constant(uint32_t(mesh.get_skin().get_hierarchy_size())) // hierarchy size
                    .add_specialization_constant(uint32_t(mesh.get_skin().get_joints_count()))   // skin size
                    .begin_descriptor_set()
                    .add_buffer(m_uniform_buffer, vk::DescriptorType::eUniformBuffer)
                    .add_buffer(m_animation_controller.get_hierarchies().front(), vk::DescriptorType::eStorageBuffer)
                    .add_buffer(mesh.get_skin().get_joints_buffer(), vk::DescriptorType::eUniformBuffer)
                    .finish_descriptor_set()
                    .begin_descriptor_set()
                    .add_texture(mat.get_base_color(m_geometry).get_image(), mat.get_base_color(m_geometry).get_sampler())
                    .add_texture(mat.get_normal(m_geometry).get_image(), mat.get_normal(m_geometry).get_sampler())
                    .add_texture(mat.get_metallic_roughness(m_geometry).get_image(), mat.get_metallic_roughness(m_geometry).get_sampler())
                    .add_texture(mat.get_occlusion(m_geometry).get_image(), mat.get_occlusion(m_geometry).get_sampler())
                    .add_texture(mat.get_emissive(m_geometry).get_image(), mat.get_emissive(m_geometry).get_sampler())
                    .finish_descriptor_set();
                m_models_primitives_pipelines[mesh_id].emplace_back(builder.create_graphics_pipeline(m_pass, 0));
                curr_primitive++;
            }
        }
    }


    void write_command_buffers(uint64_t dt)
    {
        glm::mat4 model_matrix{1};
        model_matrix = glm::translate(model_matrix, {1, 2, 3});
        gltf::instance_transform_data istance_transform{
            .model = model_matrix,
            .view = get_main_camera().get_view_matrix(),
            .proj = get_main_camera().get_proj_matrix(),
            .mvp = get_main_camera().get_proj_matrix() * get_main_camera().get_view_matrix()};

        vk::CommandBuffer& command_buffer = m_command_buffer->front();
        command_buffer.reset();

        command_buffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse});

        m_uniform_buffer.upload([istance_transform](uint8_t* dst) {
            std::memcpy(dst, &istance_transform, sizeof(istance_transform));
        });

        m_animation_controller.update(dt);

        m_buffer_pool.update();
        m_animation_controller.update(command_buffer);

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
    gltf::animation_controller m_animation_controller{};
    gltf::animation_instance* m_anim_instance{};

    avk::render_pass_instance m_pass{};

    std::vector<avk::semaphore> m_semaphores{};
    std::vector<vk::Semaphore> m_native_semaphores{};
    std::vector<avk::fence> m_fences{};
    std::vector<vk::Fence> m_native_fences{};

    avk::command_pool m_command_pool{};
    avk::command_buffer_list m_command_buffer{};

    avk::buffer_pool m_buffer_pool{};
    avk::image_pool m_image_pool{};

    avk::shader_module m_vertex_shader{};
    avk::shader_module m_fragment_shader{};
    avk::shader_module m_comp_shader{};
    std::unordered_map<int32_t, std::vector<avk::pipeline_instance>> m_models_primitives_pipelines{};

    avk::buffer_instance m_uniform_buffer{};

    bool m_reset_command_buffer = false;
};

int main(int argv, const char** argc)
{
    test_sample_app app{R"(D:\dev\glTF-Sample-Models\2.0\CesiumMan\glTF\CesiumMan.gltf)"};
    app.main_loop();

    return 0;
}
