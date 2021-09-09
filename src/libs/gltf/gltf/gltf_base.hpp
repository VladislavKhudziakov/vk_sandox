#pragma once

#include <gltf/utils.hpp>

#include <utils/data.hpp>

#include <nlohmann/json.hpp>

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>
#include <variant>

#define _USE_MATH_DEFINES
#include <math.h>

namespace sandbox::gltf
{
    class assets_factory;


    class buffer
    {
    public:
        explicit buffer(utils::data);
        explicit buffer(const std::string&);
        virtual ~buffer() = default;

        const utils::data& get_data() const;

    private:
        mutable utils::data m_data;
    };


    class texture
    {
    public:
        texture(const nlohmann::json& gltf_json, const nlohmann::json& texture_json);
        virtual ~texture() = default;

        uint32_t get_sampler() const;
        uint32_t get_image() const;

    private:
        uint32_t m_sampler{0};
        uint32_t m_source{0};
    };


    class image
    {
    public:
        image(
            const std::vector<std::unique_ptr<buffer>>& buffers,
            const nlohmann::json& gltf_json,
            const nlohmann::json& image_json);

        virtual ~image() = default;

        size_t get_width() const;
        size_t get_height() const;
        size_t get_components_count() const;

        const uint8_t* get_pixels() const;

        image_mime_type get_mime() const;

    private:
        using image_handler = std::unique_ptr<uint8_t, std::function<void(const uint8_t*)>>;

        size_t m_width{0};
        size_t m_height{0};
        size_t m_components_count{0};

        image_handler m_data{nullptr, [](const uint8_t*){}};

        image_mime_type_value m_mime_type{};
    };


    class sampler
    {
    public:
        sampler(const nlohmann::json& gltf_json, const nlohmann::json& sampler_json);
        virtual ~sampler() = default;

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
        struct attribute_data
        {
            uint32_t buffer{0};
            uint64_t buffer_offset{0};
            uint64_t buffer_length{0};
            gltf::accessor_type accessor_type{};
            gltf::component_type component_type{};
        };

        struct indices_data
        {
            int32_t buffer{-1};
            uint64_t indices_count{0};
            uint64_t buffer_offset{0};
            uint64_t buffer_length{0};
            gltf::accessor_type accessor_type{};
            gltf::component_type component_type{};
        };

        primitive(const nlohmann::json& gltf_json, const nlohmann::json& primitive_json);
        virtual ~primitive() = default;

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
        mesh(
            assets_factory& assets_factory,
            const nlohmann::json& gltf_json,
            const nlohmann::json& mesh_json);
        virtual ~mesh() = default;

        const std::vector<std::unique_ptr<primitive>>& get_primitives() const;

    private:
        std::vector<std::unique_ptr<primitive>> m_primitives{};
    };


    class skin
    {
    public:
        skin(const std::vector<std::unique_ptr<buffer>>& buffers, const nlohmann::json& gltf_json, const nlohmann::json& skin_json);
        virtual ~skin() = default;

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
        virtual ~node() = default;

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
        material(const nlohmann::json& gltf_json, const nlohmann::json& material_json);

        virtual ~material() = default;

        const pbr_metallic_roughness& get_pbr_metallic_roughness() const;

        const texture_data& get_normal_texture() const;
        const texture_data& get_occlusion_texture() const;
        const texture_data& get_emissive_texture() const;

        glm::vec3 get_emissive_factor() const;

        alpha_mode get_alpha_mode() const;

        float get_alpha_cutoff() const;

        bool is_double_sided() const;

    private:
        pbr_metallic_roughness m_pbr_metallic_roughness_data{};
        texture_data m_normal_texture{};
        texture_data m_occlusion_texture{};
        texture_data m_emissive_texture{};

        glm::vec3 m_emissive_factor{0, 0, 0};

        alpha_mode_value m_alpha_mode{};

        float m_alpha_cutoff{0.5};

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
        camera(const nlohmann::json& gltf_json, const nlohmann::json& camera_json);
        virtual ~camera() = default;

        camera_type get_type() const;

        template<typename DataType>
        const DataType& get_data() const
        {
            return std::get<DataType>(m_data);
        }

    private:
        camera_type_value m_type{};
        std::variant<perspective, orthographic> m_data{perspective{}};
    };


    class scene
    {
    public:
        scene(const nlohmann::json& gltf_json, const nlohmann::json& scene_json);
        virtual ~scene() = default;

        const std::vector<int32_t>& get_nodes() const;

    private:
        std::vector<int32_t> m_nodes{};
    };


    class assets_factory
    {
    public:
        virtual ~assets_factory() = default;

        virtual std::unique_ptr<scene> create_scene(
            const nlohmann::json& gltf_json,
            const nlohmann::json& scene_json);

        virtual std::unique_ptr<material> create_material(
            const nlohmann::json& gltf_json,
            const nlohmann::json& material_json);

        virtual std::unique_ptr<node> create_node(
            const nlohmann::json& gltf_json,
            const nlohmann::json& node_json);

        virtual std::unique_ptr<skin> create_skin(
            const std::vector<std::unique_ptr<buffer>>& buffers,
            const nlohmann::json& gltf_json,
            const nlohmann::json& skin_json);

        virtual std::unique_ptr<mesh> create_mesh(
            const nlohmann::json& gltf_json,
            const nlohmann::json& mesh_json);

        virtual std::unique_ptr<buffer> create_buffer(utils::data);

        virtual std::unique_ptr<buffer> create_buffer(const std::string&);

        virtual std::unique_ptr<primitive> create_primitive(
            const nlohmann::json& gltf_json,
            const nlohmann::json& primitive_json);

        virtual std::unique_ptr<camera> create_camera(
            const nlohmann::json& gltf_json,
            const nlohmann::json& camera_json);

        virtual std::unique_ptr<texture> create_texture(
            const nlohmann::json& gltf_json,
            const nlohmann::json& texture_json);

        virtual std::unique_ptr<image> create_image(
            const std::vector<std::unique_ptr<buffer>>& buffers,
            const nlohmann::json& gltf_json,
            const nlohmann::json& image_json);

        virtual std::unique_ptr<sampler> create_sampler(
            const nlohmann::json& gltf_json,
            const nlohmann::json& sampler_json);
    };


    class model
    {
    public:
        model() = default;

        explicit model(const nlohmann::json& gltf_json, std::unique_ptr<assets_factory> assets_factory);
        model(const nlohmann::json& gltf_json, utils::data glb_data, std::unique_ptr<assets_factory> assets_factory);

        const std::vector<std::unique_ptr<scene>>& get_scenes() const;
        const std::vector<std::unique_ptr<material>>& get_materials() const;
        const std::vector<std::unique_ptr<node>>& get_nodes() const;
        const std::vector<std::unique_ptr<skin>>& get_skins() const;
        const std::vector<std::unique_ptr<mesh>>& get_meshes() const;
        const std::vector<std::unique_ptr<buffer>>& get_buffers() const;
        const std::vector<std::unique_ptr<camera>>& get_cameras() const;
        const std::vector<std::unique_ptr<image>>& get_images() const;
        const std::vector<std::unique_ptr<texture>>& get_textures() const;
        const std::vector<std::unique_ptr<sampler>>& get_samplers() const;

        uint32_t get_current_scene() const;

        assets_factory& get_assets_factory();

    private:
        void init_resources(const nlohmann::json& gltf_json);

        uint32_t m_current_scene{0};

        std::vector<std::unique_ptr<scene>> m_scenes{};
        std::vector<std::unique_ptr<material>> m_materials{};
        std::vector<std::unique_ptr<node>> m_nodes{};
        std::vector<std::unique_ptr<skin>> m_skins{};
        std::vector<std::unique_ptr<mesh>> m_meshes{};
        std::vector<std::unique_ptr<buffer>> m_buffers{};
        std::vector<std::unique_ptr<texture>> m_textures{};
        std::vector<std::unique_ptr<image>> m_images{};
        std::vector<std::unique_ptr<sampler>> m_samplers{};
        std::vector<std::unique_ptr<camera>> m_cameras{};

        std::unique_ptr<assets_factory> m_assets_factory{};
    };


    struct accessor_data
    {
        constexpr static auto BOUND_MIN_VALUE = std::numeric_limits<float>::min();
        constexpr static auto BOUND_MAX_VALUE = std::numeric_limits<float>::max();

        uint32_t buffer{0};
        uint64_t buffer_offset{0};

        glm::vec3 min_bound{BOUND_MAX_VALUE, BOUND_MAX_VALUE, BOUND_MAX_VALUE};
        glm::vec3 max_bound{BOUND_MIN_VALUE, BOUND_MIN_VALUE, BOUND_MIN_VALUE};

        component_type component_type;
        accessor_type accessor_type;

        size_t count;
    };


    accessor_data extract_accessor_data_from_buffer(
        const nlohmann::json& gltf,
        const nlohmann::json& accessor);


    std::pair<const void*, size_t> extract_buffer_view_data_from_buffer(
        const nlohmann::json& buffer_view,
        const std::vector<std::unique_ptr<buffer>>& buffers);
} // namespace sandbox::gltf
