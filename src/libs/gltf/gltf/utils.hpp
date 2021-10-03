#pragma once

#include <nlohmann/json.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>


namespace sandbox::gltf
{
    namespace detail
    {
        template<typename T1, typename T2, typename = void>
        struct can_get
        {
            constexpr static bool value = false;
        };

        template<typename T1, typename T2>
        struct can_get<T1, T2, std::void_t<decltype(std::declval<T1>().template get<T2>())>>
        {
            constexpr static bool value = true;
        };

        template<typename T1, typename T2>
        constexpr bool can_get_v = can_get<T1, T2>::value;
    } // namespace detail

    enum class attribute_path
    {
        position,
        normal,
        tangent,
        texcoord_0,
        texcoord_1,
        color_0,
        joints_0,
        weights_0
    };

    enum class component_type
    {
        signed_byte = 5120,
        unsigned_byte = 5121,
        signed_short = 5122,
        unsigned_short = 5123,
        unsigned_int = 5125,
        float32 = 5126
    };


    enum class sampler_filter_type
    {
        nearest = 9728,
        linear = 9729,
        near_mipmap_nearest = 9984,
        linear_mipmap_nearest = 9985,
        near_mipmap_linear = 9986,
        linear_mipmap_linear = 9987
    };


    enum class sampler_wrap_type
    {
        repeat = 10497,
        clamp_to_edge = 33071,
        mirrored_repeat = 33648
    };


    enum class alpha_mode
    {
        opaque,
        mask,
        blend
    };


    enum class accessor_type
    {
        scalar,
        vec2,
        vec3,
        vec4,
        mat2,
        mat3,
        mat4
    };

    enum class camera_type
    {
        perspective,
        orthographic
    };


    enum class image_mime_type
    {
        jpeg,
        png,
        undefined
    };

    enum class animation_path
    {
        rotation,
        translation,
        scale,
        weights
    };

    enum class animation_interpolation
    {
        linear,
        step,
        cubic_spline
    };

    enum glb_chunk_type : uint32_t
    {
        JSON = 0x4E4F534A, // "JSON"
        BIN = 0x004E4942   // "BIN"
    };

    struct glb_header
    {
        uint32_t magic;
        uint32_t version;
        uint32_t length;
    };

    struct glb_chunk
    {
        uint32_t size;
        glb_chunk_type type;
        const uint8_t* data;
    };

    struct glb_file
    {
        glb_header header;
        glb_chunk json;
        glb_chunk bin;
    };

    struct accessor_type_value
    {
        constexpr static auto ACCESSOR_TYPE_SCALAR = "SCALAR";
        constexpr static auto ACCESSOR_TYPE_VEC2 = "VEC2";
        constexpr static auto ACCESSOR_TYPE_VEC3 = "VEC3";
        constexpr static auto ACCESSOR_TYPE_VEC4 = "VEC4";
        constexpr static auto ACCESSOR_TYPE_MAT2 = "MAT2";
        constexpr static auto ACCESSOR_TYPE_MAT3 = "MAT3";
        constexpr static auto ACCESSOR_TYPE_MAT4 = "MAT4";

        accessor_type_value() = default;
        accessor_type_value(const char* value);
        accessor_type_value(const std::string& value);
        operator sandbox::gltf::accessor_type() const;

        accessor_type type{};
    };


    struct attribute_path_value
    {
        constexpr static auto ATTRIBUTE_PATH_POSITION = "POSITION";
        constexpr static auto ATTRIBUTE_PATH_NORMAL = "NORMAL";
        constexpr static auto ATTRIBUTE_PATH_TANGENT = "TANGENT";
        constexpr static auto ATTRIBUTE_PATH_TEXCOORD_0 = "TEXCOORD_0";
        constexpr static auto ATTRIBUTE_PATH_TEXCOORD_1 = "TEXCOORD_1";
        constexpr static auto ATTRIBUTE_PATH_COLOR_0 = "COLOR_0";
        constexpr static auto ATTRIBUTE_PATH_JOINTS_0 = "JOINTS_0";
        constexpr static auto ATTRIBUTE_PATH_WEIGHTS_0 = "WEIGHTS_0";

        attribute_path_value() = default;
        attribute_path_value(const char* value);
        attribute_path_value(const std::string& value);
        operator sandbox::gltf::attribute_path() const;

        attribute_path type{};
    };


    struct alpha_mode_value
    {
        constexpr static auto ALPHA_MODE_OPAQUE = "OPAQUE";
        constexpr static auto ALPHA_MODE_MASK = "MASK";
        constexpr static auto ALPHA_MODE_BLEND = "BLEND";

        alpha_mode_value() = default;
        alpha_mode_value(const char* value);
        alpha_mode_value(const std::string& value);

        operator sandbox::gltf::alpha_mode() const;

        alpha_mode mode{alpha_mode::opaque};
    };


    struct camera_type_value
    {
        constexpr static auto CAMERA_TYPE_PERSPECTIVE = "perspective";
        constexpr static auto CAMERA_TYPE_ORTHOGRAPHIC = "orthographic";

        camera_type_value() = default;
        camera_type_value(const char* value);
        camera_type_value(const std::string& value);

        operator sandbox::gltf::camera_type() const;

        camera_type type{camera_type::perspective};
    };


    struct image_mime_type_value
    {
        constexpr static auto IMAGE_MIME_TYPE_JPEG = "image/jpeg";
        constexpr static auto IMAGE_MIME_TYPE_PNG = "image/png";

        image_mime_type_value() = default;
        image_mime_type_value(const char* value);
        image_mime_type_value(const std::string& value);

        operator sandbox::gltf::image_mime_type() const;

        image_mime_type type{image_mime_type::undefined};
    };

    struct animation_path_value
    {
        constexpr static auto ANIMATION_PATH_ROTATION = "rotation";
        constexpr static auto ANIMATION_PATH_TRANSLATION = "translation";
        constexpr static auto ANIMATION_PATH_SCALE = "scale";
        constexpr static auto ANIMATION_PATH_WEIGHTS = "weights";

        animation_path_value() = default;
        animation_path_value(const char* value);
        animation_path_value(const std::string& value);

        operator sandbox::gltf::animation_path() const;

        animation_path path{animation_path::translation};
    };

    struct animation_interpolation_value
    {
        constexpr static auto ANIMATION_INTERPOLATION_LINEAR = "LINEAR";
        constexpr static auto ANIMATION_INTERPOLATION_STEP = "STEP";
        constexpr static auto ANIMATION_INTERPOLATION_CUBIC_SPLINE = "CUBIC_SPLINE";

        animation_interpolation_value() = default;
        animation_interpolation_value(const char* value);
        animation_interpolation_value(const std::string& value);

        operator sandbox::gltf::animation_interpolation() const;

        animation_interpolation interpolation{animation_interpolation::linear};
    };


    size_t get_component_type_size(component_type component_type);
    size_t accessor_components_count(sandbox::gltf::accessor_type accessor_type);
    size_t get_buffer_element_size(accessor_type accessor_type, component_type component_type);

    std::string to_string(accessor_type);
    std::string to_string(component_type);
    std::string to_string(alpha_mode);

    void do_if_found(
        const nlohmann::json& where,
        const std::string& what,
        const std::function<void(const nlohmann::json&)>& callback);

    template<typename T>
    T extract_glm_value(const nlohmann::json& value)
    {
        using GlmValueT = typename T::value_type;

        static_assert(std::is_arithmetic_v<GlmValueT>);

        auto vector_values = value.template get<std::vector<GlmValueT>>();

        T result{};
        size_t elements_count = sizeof(T) / sizeof(GlmValueT);
        std::copy(vector_values.begin(), vector_values.begin() + std::min(elements_count, vector_values.size()), glm::value_ptr(result));
        return result;
    }

    template<typename T, bool Required = true>
    auto extract_json_data(
        const nlohmann::json& where,
        const std::string& what,
        T default_value = T{},
        const std::function<T(const nlohmann::json&)> extractor = {})
    {
        auto get_value = [&extractor, &default_value](const nlohmann::json& value) {
            if constexpr (detail::can_get_v<nlohmann::json, T>) {
                if (extractor) {
                    return extractor(value);
                } else {
                    return value.template get<T>();
                }
            } else {
                if (extractor) {
                    return extractor(value);
                } else {
                    return default_value;
                }
            }
        };

        if constexpr (Required) {
            return get_value(where[what]);
        } else {
            do_if_found(where, what, [&default_value, &extractor, &get_value](const nlohmann::json& value) {
                default_value = get_value(value);
            });

            return default_value;
        }
    }

    glb_file parse_glb_file(const uint8_t* data_ptr, size_t data_size);
} // namespace sandbox::gltf
