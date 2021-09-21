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
    class assets_factory;
    class buffer;
    class buffer_view;
    class accessor;


    class buffer
    {
    public:
        explicit buffer(utils::data);
        //        explicit buffer(const std::string&);
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

        //        size_t get_width() const;
        //        size_t get_height() const;
        //        size_t get_components_count() const;

        //        const uint8_t* get_pixels() const;

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
        //        struct attribute_data
        //        {
        //            uint32_t buffer{0};
        //            uint64_t buffer_offset{0};
        //            uint64_t buffer_length{0};
        //            gltf::accessor_type accessor_type{};
        //            gltf::component_type component_type{};
        //        };

        //        struct indices_data
        //        {
        //            int32_t buffer{-1};
        //            uint64_t indices_count{0};
        //            uint64_t buffer_offset{0};
        //            uint64_t buffer_length{0};
        //            gltf::accessor_type accessor_type{};
        //            gltf::component_type component_type{};
        //        };

        explicit primitive(const nlohmann::json& primitive_json);

        //        const std::vector<attribute_data>& get_attributes_data() const;
        //        const indices_data& get_indices_data() const;
        //        uint64_t get_vertices_count() const;
        const std::vector<uint32_t>& get_attributes() const;
        const std::vector<attribute_path>& get_attributes_paths() const;

        int32_t get_material() const;
        int32_t get_indices() const;

        //        int32_t get_material() const;

    private:
        //        std::vector<attribute_data> m_attributes{};
        std::vector<uint32_t> m_attributes{};
        std::vector<attribute_path> m_attributes_paths{};

        int32_t m_material{-1};
        int32_t m_indices{-1};
        //        uint64_t m_vertices_count{0};
        //        indices_data m_indices_data{};
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
        explicit animation_channel(const nlohmann::json&);

        uint64_t get_sampler() const;
        uint64_t get_node() const;
        animation_path get_path() const;

    private:
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
        uint64_t get_output() const;

    private:
        animation_interpolation_value m_interpolation{};
        uint64_t m_input{0};
        uint64_t m_output{0};
    };


    class animation
    {
    public:
        explicit animation(const nlohmann::json& animation_json);

    private:
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

        explicit node(const nlohmann::json& node_json);

        int32_t get_mesh() const;
        int32_t get_skin() const;
        const std::vector<int32_t>& get_children() const;
        trs_transform get_transform() const;
        glm::mat4 get_matrix() const;

    private:
        //        trs_transform& get_transform();

        int32_t m_mesh = -1;
        int32_t m_skin = -1;
        std::vector<int32_t> m_children{};

        std::variant<trs_transform, glm::mat4> m_transform_data{trs_transform{}};
        //
        //        std::optional<trs_transform> m_transform{};
        //        std::optional<glm::mat4> m_matrix{};
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

    private:
        pbr_metallic_roughness m_pbr_metallic_roughness_data{};
        texture_data m_normal_texture{};
        texture_data m_occlusion_texture{};
        texture_data m_emissive_texture{};
        glm::vec3 m_emissive_factor{0, 0, 0};
        alpha_mode_value m_alpha_mode{};
        float m_alpha_cutoff{0.5};
        size_t m_textures_count{0};
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


    //    class assets_factory
    //    {
    //    public:
    //        using BufferT = buffer;
    //        using BufferViewT = buffer_view;
    //        using AccesssorT = accessor;
    //        using SceneT = scene;
    //        using NodeT = node;
    //        using MaterialT = material;
    //        using TextureT = texture;
    //        using ImageT = image;
    //        using SamplerT = sampler;
    //        using MeshT = mesh;
    //        using CameraT = camera;
    //        using AnimationT = animation;
    //        using SkinT = skin;
    //
    //        virtual ~assets_factory() = default;
    //
    //        buffer create_buffer(
    //            const nlohmann::json& buffer_json);
    //
    //        buffer_view create_buffer_view(
    //            const nlohmann::json& buffer_view_json);
    //
    //        accessor create_accessor(
    //            const nlohmann::json& accessor_json);
    //
    //        scene create_scene(
    //            const nlohmann::json& scene_json);
    //
    //        material create_material(
    //            const nlohmann::json& material_json);
    //
    //        node create_node(
    //            const nlohmann::json& node_json);
    //
    //        skin create_skin(
    //            const nlohmann::json& skin_json);
    //
    //        mesh create_mesh(
    //            const nlohmann::json& mesh_json);
    //
    ////        virtual std::unique_ptr<buffer> create_buffer(utils::data);
    ////
    ////        virtual std::unique_ptr<buffer> create_buffer(const std::string&);
    //
    ////        virtual std::unique_ptr<primitive> create_primitive(
    ////            const nlohmann::json& primitive_json);
    //
    //        camera create_camera(
    //            const nlohmann::json& camera_json);
    //
    //        texture create_texture(
    //            const nlohmann::json& texture_json);
    //
    //        image create_image(
    //            const nlohmann::json& image_json);
    //
    //        sampler create_sampler(
    //            const nlohmann::json& sampler_json);
    //
    //        animation create_animation(
    //            const nlohmann::json& animation_json);
    //    };


    //    template <typename AssetsFactoryT = assets_factory>
    class model
    {
    public:
        static model from_url(const std::string& url)
        {
            hal::filesystem::common_file file{};
            file.open(url);

            auto file_data = file.read_all_and_move();

            if (std::filesystem::path(url).extension() == ".gltf") {
                const auto file_data = file.read_all();
                const auto gltf_json = nlohmann::json::parse(
                    file_data.get_data(),
                    file_data.get_data() + file_data.get_size());
                return gltf::model(gltf_json);
            } else {
                auto glb_data = parse_glb_file(file_data.get_data(), file_data.get_size());

                const auto gltf_json = nlohmann::json::parse(glb_data.json.data, glb_data.json.data + glb_data.json.size);
                auto m = gltf::model(
                    gltf_json,
                    utils::data::create_non_owning(const_cast<uint8_t*>(glb_data.bin.data), glb_data.bin.size));
                m.glb_data_buffer = std::move(file_data);

                return m;
            }
        }
        //        using BufferT = typename AssetsFactoryT::BufferT;
        //        using BufferViewT = typename AssetsFactoryT::BufferViewT;
        //        using AccesssorT = typename AssetsFactoryT::AccesssorT;
        //        using SceneT = typename AssetsFactoryT::SceneT;
        //        using NodeT = typename AssetsFactoryT::NodeT;
        //        using MeshT = typename AssetsFactoryT::MeshT;
        //        using MaterialT = typename AssetsFactoryT::MaterialT;
        //        using TextureT = typename AssetsFactoryT::TextureT;
        //        using ImageT = typename AssetsFactoryT::ImageT;
        //        using SamplerT = typename AssetsFactoryT::SamplerT;
        //        using CameraT = typename AssetsFactoryT::CameraT;
        //        using SkinT = typename AssetsFactoryT::SkinT;
        //        using AnimationT = typename AssetsFactoryT::AnimationT;

        model() = default;

        explicit model(const nlohmann::json& gltf_json, std::optional<utils::data> glb_data = std::nullopt)
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

            do_if_found(gltf_json, "cameras", [&gltf_json, this](const json& cameras) {
                m_cameras.reserve(cameras.size());
                for (const auto& camera : cameras) {
                    m_cameras.emplace_back(camera);
                }
            });

            nlohmann::json jj{};

            m_cameras.emplace_back();

            //            if (gltf_json.template find("buffers") != gltf_json.end()) {
            if (glb_data.has_value()) {
                m_buffers.emplace_back(std::move(*glb_data));
            }

            m_buffers.reserve(gltf_json["buffers"].size() + m_buffers.size());

            for (auto& buffer : gltf_json["buffers"]) {
                m_buffers.emplace_back(buffer);
                //                if (buffer.find("uri") != buffer.end()) {
                //                    m_buffers.emplace_back(m_assets_factory->create_buffer(buffer["uri"].get<std::string>()));
                //
                //                    // clang-format off
                //                    CHECK_MSG(
                //                        m_buffers.back()->get_data().get_size() == buffer["byteLength"],
                //                        "Bad model buffer size. Expected: " +
                //                        std::to_string(buffer["byteLength"].get<int32_t>()) +
                //                        " Actual: " + std::to_string(m_buffers.back()->get_data().get_size()));
                //                    // clang-format on
                //                } else {
                //                    // assume that glb buffer was pushed previously. If not gltf model doesn't follow gltf 2.0 spec.
                //                    CHECK_MSG(m_buffers.size() == 1, "Not first glb buffers are not allowed.");
                //                }
            }
            //            }

            for (auto& buffer_view : gltf_json["bufferViews"]) {
                m_buffer_views.emplace_back(buffer_view);
            }

            for (auto& accessor_json : gltf_json["accessors"]) {
                auto new_accessor = accessor(accessor_json);
                m_accessors.emplace_back(std::move(new_accessor));
            }

            do_if_found(gltf_json, "images", [&gltf_json, this](const json& images) {
                m_images.reserve(images.size());
                for (const auto& image : images) {
                    m_images.emplace_back(image);
                }
            });

            do_if_found(gltf_json, "samplers", [&gltf_json, this](const json& samplers) {
                m_samplers.reserve(samplers.size());
                for (const auto& sampler : samplers) {
                    m_samplers.emplace_back(sampler);
                }
            });

            do_if_found(gltf_json, "textures", [&gltf_json, this](const json& textures) {
                m_textures.reserve(textures.size());
                for (const auto& texture : textures) {
                    m_textures.emplace_back(texture);
                }
            });

            do_if_found(gltf_json, "materials", [&gltf_json, this](const json& materials) {
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

            do_if_found(gltf_json, "animations", [&gltf_json, this](const json& animations) {
                m_animations.reserve(animations.size());
                for (const auto& animation : animations) {
                    m_samplers.emplace_back(animation);
                }
            });

            do_if_found(gltf_json, "skins", [&gltf_json, this](const json& skins) {
                m_skins.reserve(skins.size());
                for (const auto& skin : skins) {
                    m_skins.emplace_back(skin);
                }
            });
        }
        //
        //        model(const nlohmann::json& gltf_json, utils::data glb_data, std::unique_ptr<assets_factory> assets_factory)
        //        {
        //        }
        const std::vector<scene>& get_scenes() const
        {
            return m_scenes;
        }

        const std::vector<camera>& get_cameras() const
        {
            return m_cameras;
        }

        const std::vector<node>& get_nodes() const
        {
            return m_nodes;
        }

        const std::vector<buffer>& get_buffers() const
        {
            return m_buffers;
        }

        const std::vector<buffer_view>& get_buffer_views() const
        {
            return m_buffer_views;
        }

        const std::vector<accessor>& get_accessors() const
        {
            return m_accessors;
        }

        const std::vector<image>& get_images() const
        {
            return m_images;
        }

        const std::vector<sampler>& get_samplers() const
        {
            return m_samplers;
        }

        const std::vector<texture>& get_textures() const
        {
            return m_textures;
        }

        const std::vector<material>& get_materials() const
        {
            return m_materials;
        }

        const std::vector<mesh>& get_meshes() const
        {
            return m_meshes;
        }

        const std::vector<animation>& get_animations() const
        {
            return m_animations;
        }

        const std::vector<skin>& get_skins() const
        {
            return m_skins;
        }

        uint32_t get_current_scene() const
        {
            return m_current_scene;
        }
        //        uint32_t get_current_scene() const;
        //
        //        assets_factory& get_assets_factory();

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

        utils::data glb_data_buffer{};

        //        std::vector<std::unique_ptr<scene>> m_scenes{};
        //        std::vector<std::unique_ptr<material>> m_materials{};
        //        std::vector<std::unique_ptr<node>> m_nodes{};
        //        std::vector<std::unique_ptr<skin>> m_skins{};
        //        std::vector<std::unique_ptr<mesh>> m_meshes{};
        //        std::vector<std::unique_ptr<buffer>> m_buffers{};
        //        std::vector<std::unique_ptr<texture>> m_textures{};
        //        std::vector<std::unique_ptr<image>> m_images{};
        //        std::vector<std::unique_ptr<sampler>> m_samplers{};
        //        std::vector<std::unique_ptr<camera>> m_cameras{};

        //        std::unique_ptr<AssetsFactoryT> m_assets_factory{};
    };
} // namespace sandbox::gltf
