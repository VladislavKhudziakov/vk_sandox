

#include "gltf_base.hpp"

#include <utils/conditions_helpers.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <filesystem>

using namespace sandbox;
using namespace sandbox::gltf;

primitive::primitive(const nlohmann::json& primitive_json)
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

        m_attributes.emplace_back(attribute->get<int32_t>());
        m_attributes_paths.emplace_back(attribute_path_value(attr));
    }

    m_indices = extract_json_data<int32_t, false>(primitive_json, "indices", -1);
    m_material = extract_json_data<int32_t, false>(primitive_json, "material", -1);
}


int32_t primitive::get_material() const
{
    return m_material;
}


const std::vector<uint32_t>& primitive::get_attributes() const
{
    return m_attributes;
}


const std::vector<attribute_path>& primitive::get_attributes_paths() const
{
    return m_attributes_paths;
}


int32_t primitive::get_indices() const
{
    return m_indices;
}


mesh::mesh(const nlohmann::json& mesh_json)
{
    m_primitives.reserve(mesh_json["primitives"].size());
    for (const auto& primitive_json : mesh_json["primitives"]) {
        m_primitives.emplace_back(primitive_json);
    }
}


const std::vector<primitive>& mesh::get_primitives() const
{
    return m_primitives;
}


skin::skin(const nlohmann::json& skin_json)
    : m_inv_bind_matrices(extract_json_data<uint32_t>(skin_json, "inverseBindMatrices"))
    , m_joints(extract_json_data<std::vector<int32_t>>(skin_json, "joints"))
{
}


uint32_t skin::get_inv_bind_matrices() const
{
    return m_inv_bind_matrices;
}


const std::vector<int32_t>& skin::get_joints() const
{
    return m_joints;
}


node::node(const nlohmann::json& node_json)
    : m_mesh(extract_json_data<int32_t, false>(node_json, "mesh", -1))
    , m_skin(extract_json_data<int32_t, false>(node_json, "skin", -1))
    , m_children(extract_json_data<std::vector<int32_t>, false>(node_json, "children"))
{
    if (auto matrix_it = node_json.find("matrix"); matrix_it != node_json.end()) {
        m_transform_data = extract_json_data<glm::mat4>(node_json, "matrix", glm::mat4{1}, extract_glm_value<glm::mat4>);
    } else {
        trs_transform trs{
            .rotation = extract_json_data<glm::quat, false>(node_json, "rotation", glm::identity<glm::quat>(), extract_glm_value<glm::quat>),
            .scale = extract_json_data<glm::vec3, false>(node_json, "scale", {1, 1, 1}, extract_glm_value<glm::vec3>),
            .translation = extract_json_data<glm::vec3, false>(node_json, "scale", {0, 0, 0}, extract_glm_value<glm::vec3>),
        };

        m_transform_data = trs;
    }
}


int32_t node::get_mesh() const
{
    return m_mesh;
}


int32_t node::get_skin() const
{
    return m_skin;
}


const std::vector<int32_t>& node::get_children() const
{
    return m_children;
}


node::trs_transform node::get_transform() const
{
    if (const auto* trs = std::get_if<trs_transform>(&m_transform_data); trs != nullptr) {
        return *trs;
    } else {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(std::get<glm::mat4>(m_transform_data), scale, rotation, translation, skew, perspective);
        return {
            .rotation = glm::conjugate(rotation),
            .scale = scale,
            .translation = translation};
    }
}


glm::mat4 node::get_matrix() const
{
    if (const auto* matrix = std::get_if<glm::mat4>(&m_transform_data); matrix != nullptr) {
        return *matrix;
    } else {
        auto trs = std::get<trs_transform>(m_transform_data);
        glm::mat4 result{1};
        result = glm::translate(result, trs.translation);
        result *= glm::mat4_cast(trs.rotation);
        result = glm::scale(result, trs.scale);
        return result;
    }
}


scene::scene(const nlohmann::json& scene_json)
    : m_nodes(extract_json_data<std::vector<int32_t>>(scene_json, "nodes"))
{
}


const std::vector<int32_t>& scene::get_nodes() const
{
    return m_nodes;
}


buffer::buffer(utils::data data)
    : m_data(std::move(data))
{
}


buffer::buffer(const nlohmann::json& buffer_json)
{
    auto uri = extract_json_data<std::string, false>(buffer_json, "uri");

    if (!uri.empty()) {
        hal::filesystem::common_file file;
        file.open(uri);
        m_data = file.read_all_and_move();
    }
}


const uint8_t* buffer::get_data() const
{
    return m_data.get_data();
}


material::material(const nlohmann::json& material_json)
{
    using json = nlohmann::json;

    auto extract_texture_data = [](const json& json_texture) {
        texture_data result{
            .index = extract_json_data<int32_t>(json_texture, "index"),
            .coord_set = static_cast<texture_data::coords_set>(extract_json_data<int32_t, false>(json_texture, "texCoord", 0))};

        return result;
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

    do_if_found(material_json, "pbrMetallicRoughness", [this, &extract_texture_data](const json& pbr_metallic_roughness_json) {
        auto& pbr = m_pbr_metallic_roughness_data;

        pbr.base_color = extract_json_data<glm::vec4, false>(
            pbr_metallic_roughness_json, "baseColorFactor", pbr.base_color, extract_glm_value<glm::vec4>);

        pbr.base_color_texture = extract_json_data<texture_data, false>(
            pbr_metallic_roughness_json, "baseColorTexture", pbr.base_color_texture, extract_texture_data);

        pbr.metallic_factor = extract_json_data<float, false>(
            pbr_metallic_roughness_json, "metallicFactor", pbr.metallic_factor);

        pbr.roughness_factor = extract_json_data<float, false>(
            pbr_metallic_roughness_json, "roughnessFactor", pbr.roughness_factor);

        pbr.metallic_roughness_texture = extract_json_data<texture_data, false>(
            pbr_metallic_roughness_json, "metallicRoughnessTexture", pbr.metallic_roughness_texture, extract_texture_data);
    });

    m_normal_texture = extract_json_data<texture_data, false>(
        material_json, "normalTexture", m_normal_texture, extract_texture_data);

    m_occlusion_texture = extract_json_data<texture_data, false>(
        material_json, "occlusionTexture", m_occlusion_texture, extract_texture_data);

    m_emissive_texture = extract_json_data<texture_data, false>(
        material_json, "emissiveTexture", m_emissive_texture, extract_texture_data);

    m_emissive_factor = extract_json_data<glm::vec3, false>(
        material_json, "emissiveFactor", m_emissive_factor, extract_glm_value<glm::vec3>);

    m_alpha_mode = extract_json_data<std::string, false>(material_json, "alphaMode", to_string(m_alpha_mode));

    m_alpha_cutoff = extract_json_data<float, false>(material_json, "alphaCutoff", m_alpha_cutoff);

    m_double_sided = extract_json_data<bool, false>(material_json, "doubleSided", m_double_sided);
}


const material::pbr_metallic_roughness& material::get_pbr_metallic_roughness() const
{
    return m_pbr_metallic_roughness_data;
}


const material::texture_data& material::get_normal_texture() const
{
    return m_normal_texture;
}


const material::texture_data& material::get_occlusion_texture() const
{
    return m_occlusion_texture;
}


const material::texture_data& material::get_emissive_texture() const
{
    return m_emissive_texture;
}


glm::vec3 material::get_emissive_factor() const
{
    return m_emissive_factor;
}


alpha_mode material::get_alpha_mode() const
{
    return m_alpha_mode;
}


float material::get_alpha_cutoff() const
{
    return m_alpha_cutoff;
}


bool material::is_double_sided() const
{
    return m_double_sided;
}


uint32_t material::get_textures_count() const
{
    auto check_tex = [this](const texture_data& data) {
        if (data.index >= 0) {
            m_textures_count++;
        }
    };

    if (m_textures_count < 0) {
        m_textures_count = 0;
        check_tex(m_pbr_metallic_roughness_data.base_color_texture);
        check_tex(m_pbr_metallic_roughness_data.metallic_roughness_texture);
        check_tex(m_normal_texture);
        check_tex(m_emissive_texture);
        check_tex(m_occlusion_texture);
    }

    return m_textures_count;
}


camera::camera(const nlohmann::json& camera_json)
    : m_type(extract_json_data<std::string, false>(camera_json, "type", ""))
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


camera_type camera::get_type() const
{
    return m_type;
}


glm::mat4 camera::calculate_projection(float screen_aspect) const
{
    switch (m_type) {
        case camera_type::perspective:
            if (const auto data = std::get<perspective>(m_data); data.zfar == 0) {
                return glm::infinitePerspective(
                    data.yfov,
                    data.aspect_ratio == 0.0f ? screen_aspect : data.aspect_ratio,
                    data.znear);
            } else {
                return glm::perspective(
                    data.yfov,
                    data.aspect_ratio == 0.0f ? screen_aspect : data.aspect_ratio,
                    data.znear,
                    data.zfar);
            }
        case camera_type::orthographic:
            if (const auto data = std::get<orthographic>(m_data); data.zfar == 0) {
                return glm::ortho(-data.xmag / 2, data.xmag / 2, -data.ymag / 2, data.ymag / 2);
            } else {
                return glm::ortho(-data.xmag / 2, data.xmag / 2, -data.ymag / 2, data.ymag / 2, data.znear, data.zfar);
            }
        default:
            throw std::runtime_error("Bad camera projecion type.");
    }
}


texture::texture(const nlohmann::json& texture_json)
    : m_sampler{texture_json["sampler"]}
    , m_source{texture_json["source"]}
{
}


uint32_t texture::get_sampler() const
{
    return m_sampler;
}


uint32_t texture::get_image() const
{
    return m_source;
}


image::image(
    const nlohmann::json& image_json)
    : m_uri{extract_json_data<std::string, false>(image_json, "uri")}
    , m_buffer_view{extract_json_data<int32_t, false>(image_json, "bufferView", -1)}
    , m_mime_type{extract_json_data<std::string, false>(image_json, "mimeType")}
{
}


image_mime_type image::get_mime() const
{
    return m_mime_type;
}


const std::string& image::get_uri() const
{
    return m_uri;
}


int32_t image::get_buffer_view() const
{
    return m_buffer_view;
}


sampler::sampler(const nlohmann::json& sampler_json)
    : m_mag_filter(static_cast<sampler_filter_type>(
        extract_json_data<uint32_t, false>(sampler_json, "magFilter", uint32_t(sampler_filter_type::linear))))
    , m_min_filter(static_cast<sampler_filter_type>(
          extract_json_data<uint32_t, false>(sampler_json, "minFilter", uint32_t(sampler_filter_type::linear))))
    , m_wrap_s(static_cast<sampler_wrap_type>(
          extract_json_data<uint32_t, false>(sampler_json, "wrapS", uint32_t(sampler_wrap_type::repeat))))
    , m_wrap_t(static_cast<sampler_wrap_type>(
          extract_json_data<uint32_t, false>(sampler_json, "wrapT", uint32_t(sampler_wrap_type::repeat))))
{
}


sampler_filter_type sampler::get_mag_filter() const
{
    return m_mag_filter;
}


sampler_filter_type sampler::get_min_filter() const
{
    return m_min_filter;
}


sampler_wrap_type sampler::get_wrap_s() const
{
    return m_wrap_s;
}


sampler_wrap_type sampler::get_wrap_t() const
{
    return m_wrap_t;
}


accessor::accessor(const nlohmann::json& accessor_json)
    : m_buffer_view(extract_json_data<uint64_t>(accessor_json, "bufferView"))
    , m_byte_offset(extract_json_data<size_t>(accessor_json, "byteOffset"))
    , m_component_type(static_cast<component_type>(extract_json_data<uint32_t>(accessor_json, "componentType")))
    , m_type(extract_json_data<std::string>(accessor_json, "type"))
    , m_count(extract_json_data<uint64_t>(accessor_json, "count"))
    , m_max(extract_json_data<glm::vec4, false>(accessor_json, "max", glm::vec4{}, extract_glm_value<glm::vec4>))
    , m_min(extract_json_data<glm::vec4, false>(accessor_json, "max", glm::vec4{}, extract_glm_value<glm::vec4>))
{
}


uint64_t accessor::get_buffer_view() const
{
    return m_buffer_view;
}


size_t accessor::get_byte_offset() const
{
    return m_byte_offset;
}


accessor_type accessor::get_type() const
{
    return m_type;
}


component_type accessor::get_component_type() const
{
    return m_component_type;
}


uint64_t accessor::get_count() const
{
    return m_count;
}


glm::vec4 accessor::get_min() const
{
    return m_min;
}


glm::vec4 accessor::get_max() const
{
    return m_max;
}


const uint8_t* accessor::get_data(
    const buffer* buffers,
    size_t buffers_size,
    const buffer_view* buffer_views,
    size_t buffer_views_size) const
{
    return buffer_views[m_buffer_view].get_data(buffers, buffers_size) + m_byte_offset;
}


size_t accessor::get_data_size() const
{
    return get_buffer_element_size(m_type, m_component_type) * m_count;
}


buffer_view::buffer_view(const nlohmann::json& buffer_view_json)
    : m_buffer(extract_json_data<uint64_t>(buffer_view_json, "buffer"))
    , m_byte_offset(extract_json_data<size_t>(buffer_view_json, "byteOffset"))
    , m_byte_length(extract_json_data<size_t>(buffer_view_json, "byteLength"))
    , m_byte_stride(extract_json_data<size_t, false>(buffer_view_json, "byteStride", 0))
{
}


uint64_t buffer_view::get_buffer() const
{
    return m_buffer;
}


size_t buffer_view::get_byte_offset() const
{
    return m_byte_offset;
}


size_t buffer_view::get_byte_length() const
{
    return m_byte_length;
}


size_t buffer_view::get_byte_stride() const
{
    return m_byte_stride;
}


const uint8_t* buffer_view::get_data(
    const buffer* buffers,
    size_t buffers_size) const
{
    return buffers[m_buffer].get_data() + m_byte_offset;
}


animation::animation(const nlohmann::json& animation_json)
{
    for (const auto& sampler : animation_json["samplers"]) {
        m_samplers.emplace_back(sampler);
    }

    for (const auto& channel : animation_json["channels"]) {
        m_channels.emplace_back(channel);
    }
}


animation_channel::animation_channel(const nlohmann::json& animation_channel_json)
    : m_sampler(extract_json_data<uint64_t>(animation_channel_json, "sampler"))
    , m_node(extract_json_data<uint64_t>(animation_channel_json, "node"))
    , m_path(extract_json_data<std::string>(animation_channel_json, "path"))
{
}


uint64_t animation_channel::get_sampler() const
{
    return m_sampler;
}


uint64_t animation_channel::get_node() const
{
    return m_node;
}


animation_path animation_channel::get_path() const
{
    return m_path;
}


animation_sampler::animation_sampler(const nlohmann::json& animation_sampler_json)
    : m_input(extract_json_data<uint64_t>(animation_sampler_json, "input"))
    , m_output(extract_json_data<uint64_t>(animation_sampler_json, "output"))
    , m_interpolation(extract_json_data<std::string>(animation_sampler_json, "interpolation"))
{
}


animation_interpolation animation_sampler::get_interpolation() const
{
    return m_interpolation;
}


uint64_t animation_sampler::get_input() const
{
    return m_input;
}


uint64_t animation_sampler::get_output() const
{
    return m_output;
}


model model::from_url(const std::string& url)
{
    auto path = std::filesystem::path(url);

    std::string cwd{};

    if (path.is_absolute()) {
        cwd = path.parent_path().string();
    } else {
        cwd = std::filesystem::current_path().string();
    }

    hal::filesystem::common_file file{cwd};
    file.open(url);

    auto file_data = file.read_all_and_move();

    if (std::filesystem::path(url).extension() == ".gltf") {
        const auto file_data = file.read_all();
        const auto gltf_json = nlohmann::json::parse(
            file_data.get_data(),
            file_data.get_data() + file_data.get_size());
        return model(gltf_json, cwd);
    } else {
        auto glb_data = parse_glb_file(file_data.get_data(), file_data.get_size());

        const auto gltf_json = nlohmann::json::parse(glb_data.json.data, glb_data.json.data + glb_data.json.size);
        auto m = model(
            gltf_json,
            cwd,
            utils::data::create_non_owning(const_cast<uint8_t*>(glb_data.bin.data), glb_data.bin.size));
        m.glb_data_buffer = std::move(file_data);

        return m;
    }
}


model::model(const nlohmann::json& gltf_json, const std::string& cwd, std::optional<utils::data> glb_data)
    : m_cwd(cwd)
{
    using namespace nlohmann;

    m_scenes.reserve(gltf_json["scenes"].size());
    for (const auto& scene : gltf_json["scenes"]) {
        m_scenes.emplace_back(scene);
    }

    m_nodes.reserve(gltf_json["nodes"].size());
    for (const auto& node : gltf_json["nodes"]) {
        m_nodes.emplace_back(node);
    }

    do_if_found(gltf_json, "cameras", [this](const json& cameras) {
        m_cameras.reserve(cameras.size());
        for (const auto& camera : cameras) {
            m_cameras.emplace_back(camera);
        }
    });

    m_cameras.emplace_back();

    if (glb_data.has_value()) {
        m_buffers.emplace_back(std::move(*glb_data));
    }

    m_buffers.reserve(gltf_json["buffers"].size() + m_buffers.size());

    for (auto& buffer : gltf_json["buffers"]) {
        m_buffers.emplace_back(buffer);
    }

    for (auto& buffer_view : gltf_json["bufferViews"]) {
        m_buffer_views.emplace_back(buffer_view);
    }

    for (auto& accessor_json : gltf_json["accessors"]) {
        auto new_accessor = accessor(accessor_json);
        m_accessors.emplace_back(std::move(new_accessor));
    }

    do_if_found(gltf_json, "images", [this](const json& images) {
        m_images.reserve(images.size());
        for (const auto& image : images) {
            m_images.emplace_back(image);
        }
    });

    do_if_found(gltf_json, "samplers", [this](const json& samplers) {
        m_samplers.reserve(samplers.size());
        for (const auto& sampler : samplers) {
            m_samplers.emplace_back(sampler);
        }
    });

    do_if_found(gltf_json, "textures", [this](const json& textures) {
        m_textures.reserve(textures.size());
        for (const auto& texture : textures) {
            m_textures.emplace_back(texture);
        }
    });

    do_if_found(gltf_json, "materials", [this](const json& materials) {
        m_materials.reserve(materials.size());
        for (const auto& material : materials) {
            m_materials.emplace_back(material);
        }
    });

    m_materials.emplace_back();

    m_meshes.reserve(gltf_json["meshes"].size());
    for (const auto& mesh : gltf_json["meshes"]) {
        m_meshes.emplace_back(mesh);
    }

    do_if_found(gltf_json, "animations", [this](const json& animations) {
        m_animations.reserve(animations.size());
        for (const auto& animation : animations) {
            m_samplers.emplace_back(animation);
        }
    });

    do_if_found(gltf_json, "skins", [this](const json& skins) {
        m_skins.reserve(skins.size());
        for (const auto& skin : skins) {
            m_skins.emplace_back(skin);
        }
    });
}


const std::vector<scene>& model::get_scenes() const
{
    return m_scenes;
}


const std::vector<camera>& model::get_cameras() const
{
    return m_cameras;
}


const std::vector<node>& model::get_nodes() const
{
    return m_nodes;
}


const std::vector<buffer>& model::get_buffers() const
{
    return m_buffers;
}


const std::vector<buffer_view>& model::get_buffer_views() const
{
    return m_buffer_views;
}


const std::vector<accessor>& model::get_accessors() const
{
    return m_accessors;
}


const std::vector<image>& model::get_images() const
{
    return m_images;
}


const std::vector<sampler>& model::get_samplers() const
{
    return m_samplers;
}


const std::vector<texture>& model::get_textures() const
{
    return m_textures;
}


const std::vector<material>& model::get_materials() const
{
    return m_materials;
}


const std::vector<mesh>& model::get_meshes() const
{
    return m_meshes;
}


const std::vector<animation>& model::get_animations() const
{
    return m_animations;
}


const std::vector<skin>& model::get_skins() const
{
    return m_skins;
}


uint32_t model::get_current_scene() const
{
    return m_current_scene;
}


const std::string& model::get_cwd() const
{
    return m_cwd;
}
