

#include "gltf_base.hpp"

#include <filesystem/common_file.hpp>

#include <utils/conditions_helpers.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <stb/stb_image.h>

using namespace sandbox;


namespace
{
    void do_if_found(const nlohmann::json& where, const std::string& what, const std::function<void(const nlohmann::json&)>& callback)
    {
        if (where.find(what) != where.end()) {
            callback(where[what]);
        }
    };
} // namespace

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

        const auto vert_buffer_accessor_data = extract_accessor_data_from_buffer(
            gltf_json, gltf_json["accessors"][attribute->get<int32_t>()]);

        const auto verts_count = vert_buffer_accessor_data.count;

        if (m_vertices_count != 0) {
            CHECK_MSG(m_vertices_count == vert_buffer_accessor_data.count, "Bad primitive vertices count.");
        } else {
            m_vertices_count = verts_count;
        }

        auto& attribute_data = m_attributes.emplace_back();
        attribute_data.buffer = vert_buffer_accessor_data.buffer;
        attribute_data.buffer_length = vert_buffer_accessor_data.count * get_buffer_element_size(
            vert_buffer_accessor_data.accessor_type,
            vert_buffer_accessor_data.component_type);
        attribute_data.buffer_offset = vert_buffer_accessor_data.buffer_offset;
        attribute_data.accessor_type = vert_buffer_accessor_data.accessor_type;
        attribute_data.component_type = vert_buffer_accessor_data.component_type;
    }

    if (auto indices = primitive_json.find("indices"); indices != primitive_json.end()) {
        const auto index_buffer_accessor_data = extract_accessor_data_from_buffer(
            gltf_json, gltf_json["accessors"][indices->get<int32_t>()]);
        m_indices_data.buffer = index_buffer_accessor_data.buffer;
        m_indices_data.buffer_offset = index_buffer_accessor_data.buffer_offset;
        m_indices_data.indices_count = index_buffer_accessor_data.count;
        m_indices_data.buffer_length = index_buffer_accessor_data.count * get_buffer_element_size(
            index_buffer_accessor_data.accessor_type,
            index_buffer_accessor_data.component_type);

        m_indices_data.accessor_type = index_buffer_accessor_data.accessor_type;
        m_indices_data.component_type = index_buffer_accessor_data.component_type;
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
    const nlohmann::json& gltf_json,
    const nlohmann::json& mesh_json)
{
    m_primitives.reserve(mesh_json["primitives"].size());
    for (const auto& primitive_json : mesh_json["primitives"]) {
        m_primitives.emplace_back(assets_factory.create_primitive(
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
    const auto accessor_data = extract_accessor_data_from_buffer(
        gltf_json,  gltf_json["accessors"][skin_json["inverseBindMatrices"].get<int32_t>()]);

    const auto& curr_buffer = buffers[accessor_data.buffer];
    const uint8_t* buffer_data = curr_buffer->get_data().get_data();

    const auto data_size = get_buffer_element_size(
        accessor_data.accessor_type,
        accessor_data.component_type) * accessor_data.count;

    std::memcpy(glm::value_ptr(inverse_bind_matrix), buffer_data + accessor_data.buffer_offset, data_size);

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
    using namespace nlohmann;

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
        m_meshes.emplace_back(m_assets_factory->create_mesh(gltf_json, mesh));
    }

    do_if_found(gltf_json, "materials", [&gltf_json, this](const json& materials) {
        m_materials.reserve(materials.size());
        for (const auto& material : materials) {
            m_materials.emplace_back(m_assets_factory->create_material(gltf_json, material));
        }
    });

    m_materials.emplace_back(m_assets_factory->create_material(gltf_json, {}));

    do_if_found(gltf_json, "skins", [&gltf_json, this](const json& skins) {
        m_skins.reserve(skins.size());
        for (const auto& skin : skins) {
            m_skins.emplace_back(m_assets_factory->create_skin(m_buffers, gltf_json, skin));
        }
    });

    do_if_found(gltf_json, "cameras", [&gltf_json, this](const json& cameras) {
        m_cameras.reserve(cameras.size());
        for (const auto& camera : cameras) {
            m_cameras.emplace_back(m_assets_factory->create_camera(gltf_json, camera));
        }
    });

    m_cameras.emplace_back(m_assets_factory->create_camera(gltf_json, {}));

    do_if_found(gltf_json, "textures", [&gltf_json, this](const json& textures) {
        m_textures.reserve(textures.size());
        for (const auto& texture : textures) {
            m_textures.emplace_back(m_assets_factory->create_texture(gltf_json, texture));
        }
    });

    do_if_found(gltf_json, "images", [&gltf_json, this](const json& images) {
        m_images.reserve(images.size());
        for (const auto& image : images) {
            m_images.emplace_back(m_assets_factory->create_image(m_buffers, gltf_json, image));
        }
    });

    do_if_found(gltf_json, "samplers", [&gltf_json, this](const json& samplers) {
        m_samplers.reserve(samplers.size());
        for (const auto& sampler : samplers) {
            m_samplers.emplace_back(m_assets_factory->create_sampler(gltf_json, sampler));
        }
    });
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


const std::vector<std::unique_ptr<gltf::image>>& gltf::model::get_images() const
{
    return m_images;
}


const std::vector<std::unique_ptr<gltf::texture>>& gltf::model::get_textures() const
{
    return m_textures;
}


const std::vector<std::unique_ptr<gltf::sampler>>& gltf::model::get_samplers() const
{
    return m_samplers;
}


const std::vector<std::unique_ptr<gltf::camera>>& gltf::model::get_cameras() const
{
    return m_cameras;
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
    const nlohmann::json& gltf_json,
    const nlohmann::json& mesh_json)
{
    return std::make_unique<gltf::mesh>(*this, gltf_json, mesh_json);
}


std::unique_ptr<gltf::primitive> gltf::assets_factory::create_primitive(
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


std::unique_ptr<gltf::camera> gltf::assets_factory::create_camera(
    const nlohmann::json& gltf_json, const nlohmann::json& camera_json)
{
    if (camera_json.empty()) {
        return std::make_unique<camera>();
    } else {
        return std::make_unique<camera>(gltf_json, camera_json);
    }
}


std::unique_ptr<gltf::texture> gltf::assets_factory::create_texture(const nlohmann::json& gltf_json, const nlohmann::json& texture_json)
{
    return std::make_unique<gltf::texture>(gltf_json, texture_json);
}


std::unique_ptr<gltf::image> gltf::assets_factory::create_image(
    const std::vector<std::unique_ptr<buffer>>& buffers,
    const nlohmann::json& gltf_json,
    const nlohmann::json& image_json)
{
    return std::make_unique<image>(buffers, gltf_json, image_json);
}


std::unique_ptr<gltf::sampler> gltf::assets_factory::create_sampler(const nlohmann::json& gltf_json, const nlohmann::json& sampler_json)
{
    return std::make_unique<sampler>(gltf_json, sampler_json);
}


gltf::material::material(const nlohmann::json& gltf_json, const nlohmann::json& material_json)
{
    using json = nlohmann::json;


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

    do_if_found(material_json, "pbrMetallicRoughness", [this, &set_texture_data, &set_vector_data](const json& pbr_metallic_roughness_data) {
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


gltf::camera::camera(const nlohmann::json& gltf_json, const nlohmann::json& camera_json)
    : m_type(camera_json["type"].get<std::string>())
{
    using json = nlohmann::json;

    switch (m_type) {
        case camera_type::perspective:
            do_if_found(camera_json, "perspective", [this](const json& perspective_json) {
                camera::perspective perspective{};

                perspective.yfov = perspective_json["yfov"];
                perspective.znear = perspective_json["znear"];

                do_if_found(perspective_json, "aspectRatio", [this, &perspective](const json& perspective_json) {
                    perspective.aspect_ratio = perspective_json["aspectRatio"].get<float>();
                });

                do_if_found(perspective_json, "zfar", [this, &perspective](const json& perspective_json) {
                    perspective.zfar = perspective_json["zfar"].get<float>();
                });

                m_data = perspective;
            });
            break;
        case camera_type::orthographic:
            do_if_found(camera_json, "orthographic", [this](const json& orthographic_json) {
                m_data = camera::orthographic{
                    .xmag = orthographic_json["xmag"],
                    .ymag = orthographic_json["ymag"],
                    .zfar = orthographic_json["zfar"],
                    .znear = orthographic_json["znear"]};
            });
            break;
    }
}


gltf::camera_type gltf::camera::get_type() const
{
    return m_type;
}


gltf::texture::texture(const nlohmann::json& gltf_json, const nlohmann::json& texture_json)
{
    m_sampler = texture_json["sampler"];
    m_source = texture_json["source"];
}


uint32_t gltf::texture::get_sampler() const
{
    return m_sampler;
}


uint32_t gltf::texture::get_image() const
{
    return m_source;
}


gltf::image::image(
    const std::vector<std::unique_ptr<buffer>>& buffers,
    const nlohmann::json& gltf_json,
    const nlohmann::json& image_json)
{
    using namespace nlohmann;

    do_if_found(image_json, "uri", [this](const json& value) {
        int w, h, c;
        auto image_ptr = stbi_load(value.get<std::string>().c_str(), &w, &h, &c, 0);

        assert(image_ptr);

        m_width = w;
        m_height = h;
        m_components_count = c;

        m_data = image_handler(image_ptr, [](const uint8_t* image) {stbi_image_free(const_cast<uint8_t*>(image));});
    });

    do_if_found(image_json, "bufferView", [this, &buffers](const json& buffer_view) {
        const auto [data, data_length] = extract_buffer_view_data_from_buffer(buffer_view, buffers);

        int w, h, c;
        auto image_ptr = stbi_load_from_memory(
            static_cast<const stbi_uc*>(data), data_length, &w, &h, &c, 0);

        assert(image_ptr);

        m_width = w;
        m_height = h;
        m_components_count = c;

        m_data = image_handler(image_ptr, [](const uint8_t* image) {stbi_image_free(const_cast<uint8_t*>(image));});
    });

    do_if_found(image_json, "mimeType", [this](const json& value) {
        m_mime_type = value.get<std::string>();
    });
}


gltf::image_mime_type gltf::image::get_mime() const
{
    return m_mime_type;
}


size_t gltf::image::get_width() const
{
    return m_width;
}


size_t gltf::image::get_height() const
{
    return m_height;
}


size_t gltf::image::get_components_count() const
{
    return m_components_count;
}


const uint8_t* gltf::image::get_pixels() const
{
    return m_data.get();
}


gltf::sampler::sampler(const nlohmann::json& gltf_json, const nlohmann::json& sampler_json)
{
    using namespace nlohmann;

    do_if_found(sampler_json, "magFilter", [this](const json& value) {
        m_mag_filter = static_cast<sampler_filter_type>(value.get<uint32_t>());
    });

    do_if_found(sampler_json, "minFilter", [this](const json& value) {
        m_min_filter = static_cast<sampler_filter_type>(value.get<uint32_t>());
    });

    do_if_found(sampler_json, "wrapS", [this](const json& value) {
        m_wrap_s = static_cast<sampler_wrap_type>(value.get<uint32_t>());
    });

    do_if_found(sampler_json, "wrapT", [this](const json& value) {
        m_wrap_t = static_cast<sampler_wrap_type>(value.get<uint32_t>());
    });
}


gltf::sampler_filter_type gltf::sampler::get_mag_filter() const
{
    return m_mag_filter;
}


gltf::sampler_filter_type gltf::sampler::get_min_filter() const
{
    return m_min_filter;
}


gltf::sampler_wrap_type gltf::sampler::get_wrap_s() const
{
    return m_wrap_s;
}


gltf::sampler_wrap_type gltf::sampler::get_wrap_t() const
{
    return m_wrap_t;
}


std::pair<const void*, size_t> gltf::extract_buffer_view_data_from_buffer(
    const nlohmann::json& buffer_view,
    const std::vector<std::unique_ptr<gltf::buffer>>& buffers)
{
    const auto offset = buffer_view["byteOffset"].get<uint64_t>();
    const auto data_length = buffer_view["length"].get<uint64_t>();
    const auto buffer = buffer_view["buffer"].get<uint32_t>();
    const auto* data = buffers[buffer]->get_data().get_data() + offset;

    return {static_cast<const void*>(data), data_length};
}


gltf::accessor_data gltf::extract_accessor_data_from_buffer(
    const nlohmann::json& gltf,
    const nlohmann::json& accessor)
{
    const auto& buffer_view = gltf["bufferViews"][accessor["bufferView"].get<uint32_t>()];
    gltf::accessor_data result {
        .buffer = buffer_view["buffer"].get<uint32_t>(),
        .buffer_offset = accessor["byteOffset"].get<uint64_t>() + buffer_view["byteOffset"].get<uint64_t>(),
        .component_type = static_cast<gltf::component_type>(accessor["componentType"].get<uint32_t>()),
        .accessor_type = accessor_type_value(accessor["type"].get<std::string>()),
        .count =  accessor["count"].get<size_t>(),
    };

    do_if_found(accessor, "min", [&result](const nlohmann::json& min) {
        const auto data = min.get<std::vector<float>>();

        if (data.size() != 3) {
            return;
        }

        std::memcpy(glm::value_ptr(result.min_bound), data.data(), sizeof(result.min_bound));
    });

    do_if_found(accessor, "max", [&result](const nlohmann::json& max) {
        const auto data = max.get<std::vector<float>>();

        if (data.size() != 3) {
            return;
        }

        std::memcpy(glm::value_ptr(result.max_bound), data.data(), sizeof(result.min_bound));
    });

    return result;
}
