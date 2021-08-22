

#include "gltf_base.hpp"

#include <filesystem/common_file.hpp>

#include <utils/conditions_helpers.hpp>

#include <glm/gtc/type_ptr.hpp>

using namespace sandbox;

gltf::primitive::primitive(const nlohmann::json& gltf_json, const nlohmann::json& primitive_json)
{
    constexpr const char* attributes[] = {
        "POSITION",
        "NORMAL",
        "TANGENT",
        "TEXCOORD_0",
        "TEXCOORD_1",
        "COLOR_0",
        "JOINTS_0",
        "WEIGHTS_0"};

    for (auto attr : attributes) {
        const auto attribute = primitive_json["attributes"].find(attr);

        if (attribute == primitive_json["attributes"].end()) {
            continue;
        }

        const auto& accessor = gltf_json["accessors"][attribute->get<int32_t>()];
        const auto& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<int32_t>()];

        const auto verts_count = accessor["count"].get<uint64_t>();

        if (m_vertices_count != 0) {
            CHECK_MSG(m_vertices_count == verts_count, "Bad primitive vertices count.");
        } else {
            m_vertices_count = verts_count;
        }

        auto& attribute_data = m_attributes.emplace_back();
        attribute_data.buffer = buffer_view["buffer"].get<uint32_t>();
        attribute_data.buffer_length = get_buffer_element_size(
                                           accessor["type"].get<std::string>(),
                                           static_cast<gltf::component_type>(accessor["componentType"].get<int32_t>()))
                                       * accessor["count"].get<uint64_t>();
        attribute_data.buffer_offset = accessor["byteOffset"].get<uint64_t>() + buffer_view["byteOffset"].get<uint64_t>();

        attribute_data.accessor_type = accessor["type"].get<std::string>();
        attribute_data.component_type = static_cast<gltf::component_type>(accessor["componentType"].get<int32_t>());
    }

    if (auto indices = primitive_json.find("indices"); indices != primitive_json.end()) {
        const auto& accessor = gltf_json["accessors"][indices->get<uint32_t>()];
        const auto& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<int32_t>()];

        m_indices_data.accessor_type = accessor["type"].get<std::string>();
        m_indices_data.component_type = static_cast<gltf::component_type>(accessor["componentType"].get<int32_t>());

        m_indices_data.buffer = buffer_view["buffer"].get<uint32_t>();
        m_indices_data.indices_count = accessor["count"].get<uint64_t>();
        m_indices_data.buffer_offset = accessor["byteOffset"].get<uint64_t>() + buffer_view["byteOffset"].get<uint64_t>();
        m_indices_data.buffer_length = get_buffer_element_size(
                                           accessor["type"].get<std::string>(),
                                           static_cast<gltf::component_type>(accessor["componentType"].get<int32_t>()))
                                       * accessor["count"].get<uint64_t>();
    }

    if (auto material = primitive_json.find("material"); material != primitive_json.end()) {
        m_material = primitive_json["material"].get<int32_t>();
    }
}


const std::vector<gltf::primitive::attribute_data>& gltf::primitive::get_attributes_data() const
{
    return m_attributes;
}


const gltf::primitive::indices_data& gltf::primitive::get_indices_data() const
{
    return m_indices_data;
}


uint64_t gltf::primitive::get_vertices_count() const
{
    return m_vertices_count;
}


int32_t gltf::primitive::get_material() const
{
    return m_material;
}


gltf::mesh::mesh(const nlohmann::json& gltf_json, const nlohmann::json& mesh_json)
{
    m_primitives.reserve(mesh_json["primitives"].size());
    for (const auto& primitive_json : mesh_json["primitives"]) {
        m_primitives.emplace_back(gltf_json, primitive_json);
    }
}


const std::vector<gltf::primitive>& gltf::mesh::get_primitives() const
{
    return m_primitives;
}


gltf::skin::skin(const std::vector<buffer>& buffers, const nlohmann::json& gltf_json, const nlohmann::json& skin_json)
{
    const auto& accessor = gltf_json["accessors"][skin_json["inverseBindMatrices"].get<int32_t>()];
    const auto& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<int32_t>()];
    const uint64_t matrix_data_ptr_offset = accessor["byteOffset"].get<uint64_t>() + buffer_view["byteOffset"].get<uint64_t>();

    const auto buffer_index = gltf_json["bufferViews"]["buffer"].get<int32_t>();
    const auto& curr_buffer = buffers[buffer_index];
    const uint8_t* buffer_data = curr_buffer.get_data().get_data();

    auto data_size = get_buffer_element_size(
                         accessor["type"].get<std::string>(),
                         static_cast<sandbox::gltf::component_type>(accessor["componentType"].get<int32_t>()))
                     * accessor["count"].get<uint64_t>();

    std::memcpy(glm::value_ptr(inverse_bind_matrix), buffer_data + matrix_data_ptr_offset, data_size);

    joints = skin_json["joints"].get<std::vector<int32_t>>();
}


gltf::node::node(const nlohmann::json& gltf_json, const nlohmann::json& node_json)
{
    if (auto mesh_it = node_json.find("mesh"); mesh_it != node_json.end()) {
        mesh = mesh_it->get<int32_t>();
    }

    if (auto rotation_it = node_json.find("rotation"); rotation_it != node_json.end()) {
        const auto rotation_data = rotation_it->get<std::vector<float>>();
        std::copy(rotation_data.begin(), rotation_data.end(), glm::value_ptr(get_transform().rotation));
    }

    if (auto scale_it = node_json.find("scale"); scale_it != node_json.end()) {
        const auto scale_data = scale_it->get<std::vector<float>>();
        std::copy(scale_data.begin(), scale_data.end(), glm::value_ptr(get_transform().scale));
    }

    if (auto translation_it = node_json.find("translation"); translation_it != node_json.end()) {
        const auto translation_data = translation_it->get<std::vector<float>>();
        std::copy(translation_data.begin(), translation_data.end(), glm::value_ptr(get_transform().translation));
    }

    if (auto children_it = node_json.find("children"); children_it != node_json.end()) {
        children = children_it->get<std::vector<int32_t>>();
    }

    if (auto matrix_it = node_json.find("matrix"); matrix_it != node_json.end()) {
        matrix.emplace();
        const auto matrix_data = matrix_it->get<std::vector<float>>();
        std::copy(matrix_data.begin(), matrix_data.end(), glm::value_ptr(*matrix));
    }
}


gltf::node::trs_transform& gltf::node::get_transform()
{
    if (!transform) {
        transform.emplace();
    }

    return *transform;
}


gltf::scene::scene(const nlohmann::json& gltf_json, const nlohmann::json& scene_json)
{
    m_nodes = scene_json["nodes"].get<std::vector<int32_t>>();
}


gltf::buffer::buffer(utils::data buffer_data)
    : m_data(std::move(buffer_data))
{
}


gltf::buffer::buffer(const std::string& url)
{
    hal::filesystem::common_file file{};
    file.open(url);
    m_data = file.read_all_and_move();
}


const utils::data& gltf::buffer::get_data() const
{
    return m_data;
}


gltf::model::model(const nlohmann::json& gltf_json)
{
    init_resources(gltf_json);
}


gltf::model::model(const nlohmann::json& gltf_json, utils::data glb_data)
{
    m_buffers.emplace_back(std::move(glb_data));
    init_resources(gltf_json);
}


void gltf::model::init_resources(const nlohmann::json& gltf_json)
{
    m_buffers.reserve(gltf_json["buffers"].size());

    for (auto& buffer : gltf_json["buffers"]) {
        if (buffer.find("uri") != buffer.end()) {
            m_buffers.emplace_back(buffer["uri"]);

            CHECK_MSG(
                m_buffers.back().get_data().get_size() == buffer["byteLength"],
                "Bad model buffer size. Expected: " + std::to_string(buffer["byteLength"].get<int32_t>()) + " Actual: " + std::to_string(m_buffers.back().get_data().get_size()));
        } else {
            // assume that glb buffer was pushed previously. If not gltf model doesn't follow gltf 2.0 spec.
            CHECK_MSG(m_buffers.size() == 1, "Not first glb buffers are not allowed.");
        }
    }

    m_scenes.reserve(gltf_json["scenes"].size());
    for (const auto& scene : gltf_json["scenes"]) {
        m_scenes.emplace_back(gltf_json, scene);
    }

    m_nodes.reserve(gltf_json["nodes"].size());
    for (const auto& node : gltf_json["nodes"]) {
        m_nodes.emplace_back(gltf_json, node);
    }

    m_meshes.reserve(gltf_json["meshes"].size());
    for (const auto& mesh : gltf_json["meshes"]) {
        m_meshes.emplace_back(gltf_json, mesh);
    }

    if (gltf_json.find("skins") != gltf_json.end()) {
        m_skins.reserve(gltf_json["skins"]);
        for (const auto& skin : gltf_json["skins"]) {
            m_skins.emplace_back(m_buffers, gltf_json, skin);
        }
    }
}


const std::vector<gltf::scene>& gltf::model::get_scenes() const
{
    return m_scenes;
}


const std::vector<gltf::material>& gltf::model::get_materials() const
{
    return m_materials;
}


const std::vector<gltf::node>& gltf::model::get_nodes() const
{
    return m_nodes;
}


const std::vector<gltf::skin>& gltf::model::get_skins() const
{
    return m_skins;
}


const std::vector<gltf::mesh>& gltf::model::get_meshes() const
{
    return m_meshes;
}


const std::vector<gltf::buffer>& gltf::model::get_buffers() const
{
    return m_buffers;
}
