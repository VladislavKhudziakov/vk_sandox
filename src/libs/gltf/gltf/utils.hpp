#pragma once

#include <string>

#include <render/vk/vulkan_dependencies.hpp>

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

    vk::Format to_vk_format(accessor_type_value accessor_type, component_type component_type);
    vk::IndexType to_vk_index_type(accessor_type_value accessor_type, component_type component_type);

    size_t get_component_type_size(component_type component_type);
    size_t get_buffer_element_size(accessor_type_value accessor_type, component_type component_type);

    std::string to_string(accessor_type);
    std::string to_string(component_type);
    std::string to_string(alpha_mode);
} // namespace sandbox::gltf