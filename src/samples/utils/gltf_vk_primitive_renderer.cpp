

#include "gltf_vk_primitive_renderer.hpp"

void sandbox::samples::gltf_vk_primitive_renderer::draw_scene(
    const sandbox::gltf::model& model,
    const vk::Buffer& vertex_buffer,
    const vk::Buffer& index_buffer)
{
    if (&model != m_last_model || model.get_current_scene() != m_last_scene) {
        create_pipelines(model, model.get_current_scene());
        write_command_buffers(model, model.get_current_scene(), vertex_buffer, index_buffer);
        m_need_rewrite_command_buffers = false;
    }

    if (m_need_rewrite_command_buffers) {
        write_command_buffers(model, model.get_current_scene(), vertex_buffer, index_buffer);
    }

    m_last_model = &model;
    m_last_scene = model.get_current_scene();
    m_need_rewrite_command_buffers = false;
}


void sandbox::samples::gltf_vk_primitive_renderer::for_each_primitive(
    const sandbox::gltf::model& model,
    const std::function<void(const gltf::primitive&, const gltf::material&)>& callback)
{
    const auto& all_meshes = model.get_meshes();
    const auto& all_materials = model.get_materials();

    for_each_scene_node(model, [&all_meshes, &all_materials, &callback](const gltf::node& node_impl) {
        if (node_impl.mesh < 0) {
            return;
        }

        const auto& mesh_impl = all_meshes[node_impl.mesh];

        for (const auto& primitive : mesh_impl->get_primitives()) {
            const auto material_index = primitive->get_material();
            const auto& material = material_index >= 0 ? all_materials[material_index] : all_materials.back();
            callback(*primitive, *material);
        }
    });
}


void sandbox::samples::gltf_vk_primitive_renderer::toggle_command_buffers_rewrite()
{
    m_need_rewrite_command_buffers = true;
}
