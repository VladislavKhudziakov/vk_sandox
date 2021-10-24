#pragma once

#include <gltf/utils.hpp>

#include <filesystem/common_file.hpp>
#include <utils/data.hpp>

#include <nlohmann/json.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <glm/gtc/quaternion.hpp>

#include <optional>
#include <variant>

#define _USE_MATH_DEFINES
#include <math.h>
#include <filesystem>

namespace sandbox::gltf
{
    class model;
    class buffer;
    class buffer_view;
    class accessor;
    class animation;

    class buffer
    {
    public:
        explicit buffer(utils::data);
        explicit buffer(const nlohmann::json& buffer_json);

        const uint8_t* get_data() const;

    private:
        utils::data m_data;
    };


    class accessor
    {
    public:
        const uint8_t* get_data(
            const buffer* buffers,
            size_t buffers_size,
            const buffer_view* buffer_views,
            size_t buffer_views_size) const;

        const uint8_t* get_data(const model&) const;

        explicit accessor(const nlohmann::json& accessor_json);

        uint64_t get_buffer_view() const;
        size_t get_byte_offset() const;
        accessor_type get_type() const;
        component_type get_component_type() const;
        uint64_t get_count() const;

        glm::vec4 get_min() const;
        glm::vec4 get_max() const;

        size_t get_data_size() const;

    private:
        uint64_t m_buffer_view{};
        size_t m_byte_offset{};
        accessor_type_value m_type{};
        component_type m_component_type{};
        uint64_t m_count{};

        glm::vec4 m_min{};
        glm::vec4 m_max{};
    };


    class buffer_view
    {
    public:
        const uint8_t* get_data(
            const buffer* buffers,
            size_t buffers_size) const;

        buffer_view(const nlohmann::json& buffer_view_json);

        uint64_t get_buffer() const;
        size_t get_byte_offset() const;
        size_t get_byte_length() const;
        size_t get_byte_stride() const;

    private:
        uint64_t m_buffer{};
        size_t m_byte_offset{};
        size_t m_byte_length{};
        size_t m_byte_stride{};
    };


    class texture
    {
    public:
        explicit texture(const nlohmann::json& texture_json);

        uint32_t get_sampler() const;
        uint32_t get_image() const;

    private:
        uint32_t m_sampler{0};
        uint32_t m_source{0};
    };


    class image
    {
    public:
        explicit image(const nlohmann::json& image_json);

        const std::string& get_uri() const;
        int32_t get_buffer_view() const;

        image_mime_type get_mime() const;

    private:
        using image_handler = std::unique_ptr<uint8_t, std::function<void(const uint8_t*)>>;

        size_t m_width{0};
        size_t m_height{0};
        size_t m_components_count{0};

        std::string m_uri{};
        int32_t m_buffer_view = -1;

        image_handler m_data{nullptr, [](const uint8_t*) {}};

        image_mime_type_value m_mime_type{};
    };


    class sampler
    {
    public:
        explicit sampler(const nlohmann::json& sampler_json);

        sampler_filter_type get_mag_filter() const;
        sampler_filter_type get_min_filter() const;

        sampler_wrap_type get_wrap_s() const;
        sampler_wrap_type get_wrap_t() const;

    private:
        sampler_filter_type m_mag_filter{sampler_filter_type::linear_mipmap_linear};
        sampler_filter_type m_min_filter{sampler_filter_type::linear_mipmap_linear};

        sampler_wrap_type m_wrap_s{sampler_wrap_type::clamp_to_edge};
        sampler_wrap_type m_wrap_t{sampler_wrap_type::clamp_to_edge};
    };


    class primitive
    {
    public:
        struct vertex_attribute
        {
            int32_t accessor = -1;
            accessor_type accessor_type{};
            component_type component_type{};
            uint64_t elements_count = 0;
            const uint8_t* attribute_data = nullptr;

            template<typename T, typename CallbackT, typename ConverterT>
            void for_each_element(
                CallbackT&& callback,
                gltf::accessor_type desired_accessor_type,
                gltf::component_type desired_component_type,
                ConverterT&& converter) const
            {
                if (attribute_data == nullptr) {
                    return;
                }

                const bool is_format_matches = accessor_type == desired_accessor_type && desired_component_type == component_type;

                auto element_size = get_buffer_element_size(accessor_type, component_type);
                for (size_t i = 0; i < elements_count; ++i) {
                    if (is_format_matches) {
                        callback(*reinterpret_cast<const T*>(attribute_data + (i * element_size)), i);
                    } else {
                        callback(converter(attribute_data + (i * element_size), accessor_type, component_type, desired_accessor_type, desired_component_type), i);
                    }
                }
            }
        };

        explicit primitive(const nlohmann::json& primitive_json);

        const std::vector<uint32_t>& get_attributes() const;
        const std::vector<attribute_path>& get_attributes_paths() const;

        int32_t get_material() const;
        int32_t get_indices() const;

        int32_t attribute_at_path(attribute_path path) const;
        vertex_attribute attribute_at_path(const gltf::model& model, attribute_path path) const;

        uint64_t get_vertices_count(const gltf::model&) const;
        uint64_t get_indices_count(const gltf::model&) const;
        std::pair<const uint8_t*, gltf::component_type> get_indices_data(const gltf::model&) const;

    private:
        std::vector<uint32_t> m_attributes{};
        std::vector<attribute_path> m_attributes_paths{};

        int32_t m_material{-1};
        int32_t m_indices{-1};
    };


    class mesh
    {
    public:
        explicit mesh(const nlohmann::json& mesh_json);

        const std::vector<primitive>& get_primitives() const;

    private:
        std::vector<primitive> m_primitives{};
    };


    class skin
    {
    public:
        explicit skin(const nlohmann::json& skin_json);

        uint32_t get_inv_bind_matrices() const;
        const std::vector<int32_t>& get_joints() const;

    private:
        uint32_t m_inv_bind_matrices{};
        std::vector<int32_t> m_joints{};
    };


    class animation_channel
    {
    public:
        animation_channel(const nlohmann::json&, const gltf::animation&);

        uint64_t get_sampler() const;
        uint64_t get_node() const;
        animation_path get_path() const;

        std::pair<uint64_t, const float*> get_keys(const gltf::model& mdl) const;
        std::tuple<uint64_t, accessor_type, component_type, const uint8_t*> get_values(const gltf::model& mdl) const;

    private:
        const gltf::animation& m_animation;
        uint64_t m_sampler{0};
        uint64_t m_node{0};
        animation_path_value m_path{};
    };


    class animation_sampler
    {
    public:
        explicit animation_sampler(const nlohmann::json& animation_sampler_json);

        animation_interpolation get_interpolation() const;

        uint64_t get_input() const;
        const accessor& get_input(const gltf::model& model) const;
        uint64_t get_output() const;
        const accessor& get_output(const gltf::model& model) const;

    private:
        animation_interpolation_value m_interpolation{};
        uint64_t m_input{0};
        uint64_t m_output{0};
    };


    class animation
    {
    public:
        using channels_list = std::array<int32_t, 4>;

        animation(const nlohmann::json& animation_json, const gltf::model& model);

        const std::vector<animation_channel>& get_channels() const;
        const std::vector<animation_sampler>& get_samplers() const;
        channels_list channels_for_node(uint32_t node_index) const;
        float get_duration() const;

    private:
        float m_duration{0};
        std::unordered_map<uint32_t, channels_list> m_node_channels_cache{};
        animation_interpolation_value m_interpolation{};
        std::vector<animation_channel> m_channels{};
        std::vector<animation_sampler> m_samplers{};
    };


    class node
    {
    public:
        struct trs_transform
        {
            glm::quat rotation{1, 0, 0, 0};
            glm::vec3 scale{1, 1, 1};
            glm::vec3 translation{0, 0, 0};
        };

        static glm::mat4 gen_matrix(const trs_transform&);

        explicit node(const nlohmann::json& node_json);

        int32_t get_mesh() const;
        int32_t get_skin() const;
        const std::vector<int32_t>& get_children() const;
        trs_transform get_transform() const;
        glm::mat4 get_matrix() const;

    private:
        int32_t m_mesh = -1;
        int32_t m_skin = -1;
        std::vector<int32_t> m_children{};

        std::variant<trs_transform, glm::mat4> m_transform_data{trs_transform{}};
    };


    class material
    {
    public:
        struct texture_data
        {
            enum class coords_set : uint32_t
            {
                texcoord_0 = 0,
                texcoord_1 = 1
            };

            int32_t index = -1;
            coords_set coord_set = coords_set::texcoord_0;
        };

        struct pbr_metallic_roughness
        {
            glm::vec4 base_color{1, 1, 1, 1};
            texture_data base_color_texture{};

            float metallic_factor{1};
            float roughness_factor{1};
            texture_data metallic_roughness_texture{};
        };

        material() = default;
        explicit material(const nlohmann::json& material_json);

        const pbr_metallic_roughness& get_pbr_metallic_roughness() const;

        const texture_data& get_normal_texture() const;
        const texture_data& get_occlusion_texture() const;
        const texture_data& get_emissive_texture() const;
        glm::vec3 get_emissive_factor() const;
        alpha_mode get_alpha_mode() const;
        float get_alpha_cutoff() const;
        bool is_double_sided() const;
        uint32_t get_textures_count() const;

        void for_each_texture(const std::function<void(const texture_data&)>& callback) const;

    private:
        pbr_metallic_roughness m_pbr_metallic_roughness_data{};
        texture_data m_normal_texture{};
        texture_data m_occlusion_texture{};
        texture_data m_emissive_texture{};
        glm::vec3 m_emissive_factor{0, 0, 0};
        alpha_mode_value m_alpha_mode{};
        float m_alpha_cutoff{0.5};
        mutable int32_t m_textures_count{-1};
        bool m_double_sided{false};
    };


    class camera
    {
    public:
        struct perspective
        {
            float aspect_ratio{0};
            float yfov{90.0 * M_PI / 180.0};
            float zfar{0};
            float znear{0.01};
        };

        struct orthographic
        {
            float xmag{0};
            float ymag{};
            float zfar{};
            float znear{0.01};
        };

        camera() = default;
        explicit camera(const nlohmann::json& camera_json);

        camera_type get_type() const;

        template<typename DataType>
        const DataType& get_data() const
        {
            return std::get<DataType>(m_data);
        }

        glm::mat4 calculate_projection(float screen_aspect) const;

    private:
        camera_type_value m_type{};
        std::variant<perspective, orthographic> m_data{perspective{}};
    };


    class scene
    {
    public:
        explicit scene(const nlohmann::json& scene_json);

        const std::vector<int32_t>& get_nodes() const;

    private:
        std::vector<int32_t> m_nodes{};
    };


    class model
    {
    public:
        static model from_url(const std::string& url);

        model() = default;

        explicit model(
            const nlohmann::json& gltf_json,
            const std::string& cwd,
            std::optional<utils::data> glb_data = std::nullopt);

        const std::vector<scene>& get_scenes() const;
        const std::vector<camera>& get_cameras() const;
        const std::vector<node>& get_nodes() const;
        const std::vector<buffer>& get_buffers() const;
        const std::vector<buffer_view>& get_buffer_views() const;
        const std::vector<accessor>& get_accessors() const;
        const std::vector<image>& get_images() const;
        const std::vector<sampler>& get_samplers() const;
        const std::vector<texture>& get_textures() const;
        const std::vector<material>& get_materials() const;
        const std::vector<mesh>& get_meshes() const;
        const std::vector<animation>& get_animations() const;
        const std::vector<skin>& get_skins() const;
        uint32_t get_current_scene() const;

        const std::string& get_cwd() const;

    private:
        uint32_t m_current_scene{0};

        std::vector<scene> m_scenes{};
        std::vector<node> m_nodes{};
        std::vector<camera> m_cameras{};

        std::vector<buffer> m_buffers{};
        std::vector<buffer_view> m_buffer_views{};
        std::vector<accessor> m_accessors{};

        std::vector<image> m_images{};
        std::vector<sampler> m_samplers{};
        std::vector<texture> m_textures{};

        std::vector<material> m_materials{};

        std::vector<mesh> m_meshes{};

        std::vector<animation> m_animations{};
        std::vector<skin> m_skins{};

        std::string m_cwd{};
        utils::data glb_data_buffer{};
    };

    template<typename Callable, typename... Args>
    void for_each_node_child(
        const std::vector<int32_t>& curr_nodes,
        const std::vector<gltf::node>& all_nodes,
        const Callable& callback,
        Args&&... args)
    {
        for (int32_t node : curr_nodes) {
            const auto& node_impl = all_nodes[node];
            if constexpr (std::is_same_v<decltype(callback(all_nodes[node], node, std::forward<Args>(args)...)), void>) {
                callback(all_nodes[node], node, std::forward<Args>(args)...);
                for_each_node_child(
                    node_impl.get_children(),
                    all_nodes,
                    callback);
            } else {
                for_each_node_child(
                    node_impl.get_children(),
                    all_nodes,
                    callback,
                    callback(all_nodes[node], node, std::forward<Args>(args)...));
            }
        }
    }

    template<typename Callable, typename... Args>
    void for_each_scene_node(
        const gltf::model& model,
        const Callable& callback,
        Args&&... args)
    {
        const auto& scenes = model.get_scenes();
        const auto& all_nodes = model.get_nodes();

        const auto& scene_nodes = scenes[model.get_current_scene()].get_nodes();

        for_each_node_child(
            scene_nodes,
            all_nodes,
            callback,
            std::forward<Args>(args)...);
    }
} // namespace sandbox::gltf
