#pragma once

#include <gltf/utils.hpp>

#include <utils/data.hpp>

#include <nlohmann/json.hpp>

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>

namespace sandbox::gltf
{
    class buffer
    {
    public:
        explicit buffer(utils::data);
        explicit buffer(const std::string&);

        const utils::data& get_data() const;

    private:
        utils::data m_data;
    };

    class primitive
    {
    public:
        struct attribute_data
        {
            uint32_t buffer{0};
            uint64_t buffer_offset{0};
            uint64_t buffer_length{0};
            gltf::accessor_type_value accessor_type{};
            gltf::component_type component_type{};
        };

        struct indices_data
        {
            int32_t buffer{-1};
            uint64_t indices_count{0};
            uint64_t buffer_offset{0};
            uint64_t buffer_length{0};
            gltf::accessor_type_value accessor_type{};
            gltf::component_type component_type{};
        };

        primitive(const nlohmann::json& gltf_json, const nlohmann::json& primitive_json);
        ~primitive() = default;

        const std::vector<attribute_data>& get_attributes_data() const;
        const indices_data& get_indices_data() const;
        uint64_t get_vertices_count() const;
        int32_t get_material() const;

    private:
        std::vector<attribute_data> m_attributes{};
        int32_t m_material{-1};
        uint64_t m_vertices_count{0};
        indices_data m_indices_data{};
    };


    class mesh
    {
    public:
        mesh(const nlohmann::json& gltf_json, const nlohmann::json& mesh_json);
        const std::vector<primitive>& get_primitives() const;

    private:
        std::vector<primitive> m_primitives{};
    };


    class skin
    {
    public:
        skin(const std::vector<buffer>& buffers, const nlohmann::json& gltf_json, const nlohmann::json& skin_json);

        glm::mat4 inverse_bind_matrix{};
        std::vector<int32_t> joints{};
    };


    class node
    {
    public:
        struct trs_transform
        {
            glm::quat rotation{0, 0, 0, 1};
            glm::vec3 scale{1, 1, 1};
            glm::vec3 translation{0, 0, 0};
        };

        node(const nlohmann::json& gltf_json, const nlohmann::json& node_json);

        int32_t mesh = -1;
        int32_t skin = -1;
        std::vector<int32_t> children{};

        std::optional<trs_transform> transform{};
        std::optional<glm::mat4> matrix{};

    private:
        trs_transform& get_transform();
    };

    class material
    {
    };

    class scene
    {
    public:
        scene(const nlohmann::json& gltf_json, const nlohmann::json& scene_json);

    private:
        std::vector<int32_t> m_nodes{};
    };


    class model
    {
    public:
        model() = default;

        explicit model(const nlohmann::json& gltf_json);
        model(const nlohmann::json& gltf_json, utils::data glb_data);

        const std::vector<scene>& get_scenes() const;
        const std::vector<material>& get_materials() const;
        const std::vector<node>& get_nodes() const;
        const std::vector<skin>& get_skins() const;
        const std::vector<mesh>& get_meshes() const;
        const std::vector<buffer>& get_buffers() const;

    private:
        void init_resources(const nlohmann::json& gltf_json);

        std::vector<scene> m_scenes{};
        std::vector<material> m_materials{};
        std::vector<node> m_nodes{};
        std::vector<skin> m_skins{};
        std::vector<mesh> m_meshes{};
        std::vector<buffer> m_buffers{};
    };
} // namespace sandbox::gltf
