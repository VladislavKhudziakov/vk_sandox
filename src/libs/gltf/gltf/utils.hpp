#pragma once

#include <render/vk/vulkan_dependencies.hpp>

#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>

#include <string>


namespace sandbox::gltf
{
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


    size_t get_component_type_size(component_type component_type);
    size_t get_buffer_element_size(accessor_type accessor_type, component_type component_type);

    std::string to_string(accessor_type);
    std::string to_string(component_type);
    std::string to_string(alpha_mode);

    void do_if_found(
        const nlohmann::json& where,
        const std::string& what,
        const std::function<void(const nlohmann::json&)>& callback);

    accessor_data extract_accessor_data_from_buffer(
        const nlohmann::json& gltf,
        const nlohmann::json& accessor);

    std::tuple<uint32_t, uint64_t, size_t> extract_buffer_view_data(
        const nlohmann::json& buffer_view);
} // namespace sandbox::gltf
