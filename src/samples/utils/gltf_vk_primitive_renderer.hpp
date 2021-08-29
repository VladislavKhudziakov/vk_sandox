#pragma once

#include <gltf/gltf_vk.hpp>

namespace sandbox::samples
{
    class gltf_vk_primitive_renderer : public gltf::gltf_vk::gltf_renderer
    {
    public:
        ~gltf_vk_primitive_renderer() override = default;

        void draw_scene(
            const gltf::model& model,
            uint32_t scene,
            const vk::Buffer& vertex_buffer,
            const vk::Buffer& index_buffer) override;

    protected:
        virtual void create_pipelines(const gltf::model& model, uint32_t scene) = 0;
        virtual void write_command_buffers(
            const gltf::model& model,
            uint32_t scene,
            const vk::Buffer& vertex_buffer,
            const vk::Buffer& index_buffer) = 0;

        void toggle_command_buffers_rewrite();

        void for_each_primitive(
            const gltf::model& model,
            const std::function<void(const gltf::primitive&, const gltf::material&)>& callback);

        template<typename Callable, typename... Args>
        void for_each_scene_node(
            const gltf::model& model,
            const Callable& callback,
            Args&&... args)
        {
            const auto& scenes = model.get_scenes();
            const auto& all_nodes = model.get_nodes();

            const auto& scene_nodes = scenes[model.get_current_scene()]->get_nodes();

            for_each_child(
                scene_nodes,
                all_nodes,
                callback,
                std::forward<Args>(args)...);
        }

    private:
        template<typename Callable, typename... Args>
        void for_each_child(
            const std::vector<int32_t>& curr_nodes,
            const std::vector<std::unique_ptr<gltf::node>>& all_nodes,
            const Callable& callback,
            Args&&... args)
        {
            for (int32_t node : curr_nodes) {
                const auto& node_impl = all_nodes[node];
                if constexpr (std::is_same_v<decltype(callback(*all_nodes[node], std::forward<Args>(args)...)), void>) {
                    callback(*all_nodes[node], std::forward<Args>(args)...);
                    for_each_child(
                        node_impl->children,
                        all_nodes,
                        callback);
                } else {
                    for_each_child(
                        node_impl->children,
                        all_nodes,
                        callback,
                        callback(*all_nodes[node], std::forward<Args>(args)...));
                }
            }
        }

        const gltf::model* m_last_model{nullptr};
        int32_t m_last_scene{-1};
        bool m_need_rewrite_command_buffers{false};
    };
} // namespace sandbox::samples
