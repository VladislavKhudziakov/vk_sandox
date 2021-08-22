

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


gltf::mesh::mesh(
    assets_factory& assets_factory,
    const std::vector<std::unique_ptr<gltf::buffer>>& buffers,
    const nlohmann::json& gltf_json,
    const nlohmann::json& mesh_json)
{
    m_primitives.reserve(mesh_json["primitives"].size());
    for (const auto& primitive_json : mesh_json["primitives"]) {
        m_primitives.emplace_back(assets_factory.create_primitive(
            buffers,
            gltf_json,
            primitive_json));
    }
}


const std::vector<std::unique_ptr<gltf::primitive>>& gltf::mesh::get_primitives() const
{
    return m_primitives;
}


gltf::skin::skin(const std::vector<std::unique_ptr<buffer>>& buffers, const nlohmann::json& gltf_json, const nlohmann::json& skin_json)
{
    const auto& accessor = gltf_json["accessors"][skin_json["inverseBindMatrices"].get<int32_t>()];
    const auto& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<int32_t>()];
    const uint64_t matrix_data_ptr_offset = accessor["byteOffset"].get<uint64_t>() + buffer_view["byteOffset"].get<uint64_t>();

    const auto buffer_index = gltf_json["bufferViews"]["buffer"].get<int32_t>();
    const auto& curr_buffer = buffers[buffer_index];
    const uint8_t* buffer_data = curr_buffer->get_data().get_data();

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


const std::vector<int32_t>& gltf::scene::get_nodes() const
{
    return m_nodes;
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


gltf::model::model(const nlohmann::json& gltf_json, std::unique_ptr<assets_factory> assets_factory)
    : m_assets_factory(std::move(assets_factory))
{
    init_resources(gltf_json);
    m_current_scene = gltf_json["scene"].get<uint32_t>();
}


gltf::model::model(const nlohmann::json& gltf_json, utils::data glb_data, std::unique_ptr<assets_factory> assets_factory)
    : m_assets_factory(std::move(assets_factory))
{
    m_buffers.emplace_back(m_assets_factory->create_buffer(std::move(glb_data)));
    init_resources(gltf_json);
    m_current_scene = gltf_json["scene"].get<uint32_t>();
}


void gltf::model::init_resources(const nlohmann::json& gltf_json)
{
    m_buffers.reserve(gltf_json["buffers"].size());

    for (auto& buffer : gltf_json["buffers"]) {
        if (buffer.find("uri") != buffer.end()) {
            m_buffers.emplace_back(m_assets_factory->create_buffer(buffer["uri"].get<std::string>()));

            // clang-format off
            CHECK_MSG(
                m_buffers.back()->get_data().get_size() == buffer["byteLength"],
                "Bad model buffer size. Expected: " +
                    std::to_string(buffer["byteLength"].get<int32_t>()) +
                    " Actual: " + std::to_string(m_buffers.back()->get_data().get_size()));
            // clang-format on
        } else {
            // assume that glb buffer was pushed previously. If not gltf model doesn't follow gltf 2.0 spec.
            CHECK_MSG(m_buffers.size() == 1, "Not first glb buffers are not allowed.");
        }
    }

    m_scenes.reserve(gltf_json["scenes"].size());
    for (const auto& scene : gltf_json["scenes"]) {
        m_scenes.emplace_back(m_assets_factory->create_scene(gltf_json, scene));
    }

    m_nodes.reserve(gltf_json["nodes"].size());
    for (const auto& node : gltf_json["nodes"]) {
        m_nodes.emplace_back(m_assets_factory->create_node(gltf_json, node));
    }

    m_meshes.reserve(gltf_json["meshes"].size());
    for (const auto& mesh : gltf_json["meshes"]) {
        m_meshes.emplace_back(m_assets_factory->create_mesh(m_buffers, gltf_json, mesh));
    }

    if (gltf_json.find("materials") != gltf_json.end()) {
        for (const auto& material : gltf_json["materials"]) {
            m_materials.emplace_back(m_assets_factory->create_material(gltf_json, material));
        }
    }

    m_materials.emplace_back(m_assets_factory->create_material(gltf_json, {}));

    if (gltf_json.find("skins") != gltf_json.end()) {
        m_skins.reserve(gltf_json["skins"]);
        for (const auto& skin : gltf_json["skins"]) {
            m_skins.emplace_back(m_assets_factory->create_skin(m_buffers, gltf_json, skin));
        }
    }
}


const std::vector<std::unique_ptr<gltf::scene>>& gltf::model::get_scenes() const
{
    return m_scenes;
}


const std::vector<std::unique_ptr<gltf::material>>& gltf::model::get_materials() const
{
    return m_materials;
}


const std::vector<std::unique_ptr<gltf::node>>& gltf::model::get_nodes() const
{
    return m_nodes;
}


const std::vector<std::unique_ptr<gltf::skin>>& gltf::model::get_skins() const
{
    return m_skins;
}


const std::vector<std::unique_ptr<gltf::mesh>>& gltf::model::get_meshes() const
{
    return m_meshes;
}


const std::vector<std::unique_ptr<gltf::buffer>>& gltf::model::get_buffers() const
{
    return m_buffers;
}


uint32_t gltf::model::get_current_scene() const
{
    return m_current_scene;
}


gltf::assets_factory& gltf::model::get_assets_factory()
{
    return *m_assets_factory;
}


std::unique_ptr<gltf::scene> gltf::assets_factory::create_scene(
    const nlohmann::json& gltf_json, const nlohmann::json& scene_json)
{
    return std::make_unique<scene>(gltf_json, scene_json);
}


std::unique_ptr<gltf::material> gltf::assets_factory::create_material(
    const nlohmann::json& gltf_json, const nlohmann::json& material_json)
{
    if (material_json.empty()) {
        return std::make_unique<gltf::material>();
    } else {
        return std::make_unique<gltf::material>(gltf_json, material_json);
    }
}


std::unique_ptr<gltf::node> gltf::assets_factory::create_node(
    const nlohmann::json& gltf_json, const nlohmann::json& node_json)
{
    return std::make_unique<gltf::node>(gltf_json, node_json);
}


std::unique_ptr<gltf::skin> gltf::assets_factory::create_skin(
    const std::vector<std::unique_ptr<buffer>>& buffers, const nlohmann::json& gltf_json, const nlohmann::json& skin_json)
{
    return std::make_unique<gltf::skin>(buffers, gltf_json, skin_json);
}


std::unique_ptr<gltf::mesh> gltf::assets_factory::create_mesh(
    const std::vector<std::unique_ptr<gltf::buffer>>& buffers,
    const nlohmann::json& gltf_json,
    const nlohmann::json& mesh_json)
{
    return std::make_unique<gltf::mesh>(*this, buffers, gltf_json, mesh_json);
}


std::unique_ptr<gltf::primitive> gltf::assets_factory::create_primitive(
    const std::vector<std::unique_ptr<gltf::buffer>>& buffers,
    const nlohmann::json& gltf_json,
    const nlohmann::json& primitive_json)
{
    return std::make_unique<primitive>(gltf_json, primitive_json);
}


std::unique_ptr<gltf::buffer> gltf::assets_factory::create_buffer(utils::data data)
{
    return std::make_unique<buffer>(std::move(data));
}


std::unique_ptr<gltf::buffer> gltf::assets_factory::create_buffer(const std::string& url)
{
    return std::make_unique<buffer>(url);
}


gltf::material::material(const nlohmann::json& gltf_json, const nlohmann::json& material_json)
{
    using json = nlohmann::json;

    auto do_if_found = [](const nlohmann::json& where, const std::string& what, const std::function<void(const nlohmann::json&)>& callback) {
        if (where.find(what) != where.end()) {
            callback(where[what]);
        }
    };


    auto set_texture_data = [](texture_data& data, const json& json_texture) {
        data.index = json_texture["index"].get<int32_t>();
        data.coord_set = static_cast<texture_data::coords_set>(json_texture["texCoord"].get<int32_t>());
    };


    auto set_vector_data = [&material_json](const json& json_data, auto& dst) {
        using dst_value_type = typename std::remove_reference_t<decltype(dst)>::value_type;
        constexpr auto el_count = sizeof(dst) / sizeof(dst_value_type);
        const auto src = json_data.get<std::vector<dst_value_type>>();

        // clang-format off
        CHECK_MSG(
            src.size() ==  el_count,
       "Bad vector elements count in material " +
            material_json["name"].get<std::string>() +
            ". Expect " + std::to_string(el_count) +
            "got " + std::to_string(src.size()));
        // clang-format on

        std::copy(src.begin(), src.end(), glm::value_ptr(dst));
    };

    do_if_found(material_json, "pbrMetallicRoughness", [this, &do_if_found, &set_texture_data, &set_vector_data](const json& pbr_metallic_roughness_data) {
        do_if_found(pbr_metallic_roughness_data, "baseColorFactor", [this, &set_vector_data](const json& color) {
            set_vector_data(color, m_pbr_metallic_roughness_data.base_color);
        });

        do_if_found(pbr_metallic_roughness_data, "baseColorTexture", [this, &set_texture_data](const json& texture) {
            set_texture_data(m_pbr_metallic_roughness_data.base_color_texture, texture);
        });

        do_if_found(pbr_metallic_roughness_data, "metallicFactor", [this](const json& metallic) {
            m_pbr_metallic_roughness_data.metallic_factor = metallic.get<float>();
        });

        do_if_found(pbr_metallic_roughness_data, "roughnessFactor", [this](const json& roughness) {
            m_pbr_metallic_roughness_data.roughness_factor = roughness.get<float>();
        });

        do_if_found(pbr_metallic_roughness_data, "metallicRoughnessTexture", [this, &set_texture_data](const json& texture) {
            set_texture_data(m_pbr_metallic_roughness_data.metallic_roughness_texture, texture);
        });
    });

    do_if_found(material_json, "normalTexture", [this, &set_texture_data](const json& texture) {
        set_texture_data(m_normal_texture, texture);
    });

    do_if_found(material_json, "occlusionTexture", [this, &set_texture_data](const json& texture) {
        set_texture_data(m_occlusion_texture, texture);
    });

    do_if_found(material_json, "emissiveTexture", [this, &set_texture_data](const json& texture) {
        set_texture_data(m_emissive_texture, texture);
    });

    do_if_found(material_json, "emissiveFactor", [this, &set_vector_data](const json& factor) {
        set_vector_data(factor, m_emissive_factor);
    });

    do_if_found(material_json, "alphaMode", [this](const json& mode) {
        m_alpha_mode = mode.get<std::string>();
    });

    do_if_found(material_json, "alphaCutoff", [this](const json& cut_off) {
        m_alpha_cutoff = cut_off.get<float>();
    });

    do_if_found(material_json, "doubleSided", [this](const json& is_double_sided) {
        m_double_sided = is_double_sided.get<bool>();
    });
}


const gltf::material::pbr_metallic_roughness& gltf::material::get_pbr_metallic_roughness() const
{
    return m_pbr_metallic_roughness_data;
}


const gltf::material::texture_data& gltf::material::get_normal_texture() const
{
    return m_normal_texture;
}


const gltf::material::texture_data& gltf::material::get_occlusion_texture() const
{
    return m_occlusion_texture;
}


const gltf::material::texture_data& gltf::material::get_emissive_texture() const
{
    return m_emissive_texture;
}


glm::vec3 gltf::material::get_emissive_factor() const
{
    return m_emissive_factor;
}


gltf::alpha_mode gltf::material::get_alpha_mode() const
{
    return m_alpha_mode;
}


float gltf::material::get_alpha_cutoff() const
{
    return m_alpha_cutoff;
}


bool gltf::material::is_double_sided() const
{
    return m_double_sided;
}
