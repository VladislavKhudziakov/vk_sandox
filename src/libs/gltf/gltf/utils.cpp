

#include "utils.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <stdexcept>
#include <cstring>


sandbox::gltf::accessor_type_value::accessor_type_value(const char* value)
{
    if (strcmp(ACCESSOR_TYPE_SCALAR, value) == 0) {
        type = accessor_type::scalar;
    } else if (strcmp(ACCESSOR_TYPE_VEC2, value) == 0) {
        type = accessor_type::vec2;
    } else if (strcmp(ACCESSOR_TYPE_VEC3, value) == 0) {
        type = accessor_type::vec3;
    } else if (strcmp(ACCESSOR_TYPE_VEC4, value) == 0) {
        type = accessor_type::vec4;
    } else {
        throw std::runtime_error("Bad accessor type value " + std::string(value));
    }
}


sandbox::gltf::accessor_type_value::accessor_type_value(const std::string& value)
    : accessor_type_value(value.c_str())
{
}


sandbox::gltf::accessor_type_value::operator sandbox::gltf::accessor_type() const
{
    return type;
}


sandbox::gltf::alpha_mode_value::alpha_mode_value(const char* value)
{
    if (strcmp(ALPHA_MODE_OPAQUE, value) == 0) {
        mode = alpha_mode::opaque;
    } else if (strcmp(ALPHA_MODE_MASK, value) == 0) {
        mode = alpha_mode::mask;
    } else if (strcmp(ALPHA_MODE_BLEND, value) == 0) {
        mode = alpha_mode::blend;
    } else {
        throw std::runtime_error("Bad alpha mode " + std::string(value));
    }
}


sandbox::gltf::alpha_mode_value::alpha_mode_value(const std::string& value)
    : alpha_mode_value(value.c_str())
{
}


sandbox::gltf::alpha_mode_value::operator sandbox::gltf::alpha_mode() const
{
    return mode;
}


sandbox::gltf::camera_type_value::camera_type_value(const char* value)
{
    if (strcmp(CAMERA_TYPE_PERSPECTIVE, value) == 0) {
        type = camera_type::perspective;
    } else if (strcmp(CAMERA_TYPE_ORTHOGRAPHIC, value) == 0) {
        type = camera_type::orthographic;
    } else {
        throw std::runtime_error("Bad camera type " + std::string(value));
    }
}


sandbox::gltf::camera_type_value::camera_type_value(const std::string& value)
    : camera_type_value(value.c_str())
{
}


sandbox::gltf::camera_type_value::operator sandbox::gltf::camera_type() const
{
    return type;
}


sandbox::gltf::image_mime_type_value::image_mime_type_value(const char* value)
{
    if (strcmp(IMAGE_MIME_TYPE_JPEG, value) == 0) {
        type = image_mime_type::jpeg;
    } else if (strcmp(IMAGE_MIME_TYPE_PNG, value) == 0) {
        type = image_mime_type::png;
    } else {
        throw std::runtime_error("Bad image mime type " + std::string(value));
    }
}


sandbox::gltf::image_mime_type_value::image_mime_type_value(const std::string& value)
    : image_mime_type_value(value.c_str())
{
}


sandbox::gltf::image_mime_type_value::operator sandbox::gltf::image_mime_type() const
{
    return type;
}


std::string sandbox::gltf::to_string(sandbox::gltf::accessor_type accessor_type)
{
    switch (accessor_type) {
        case accessor_type::scalar:
            return "SCALAR";
        case accessor_type::vec2:
            return "VEC2";
        case accessor_type::vec3:
            return "VEC3";
        case accessor_type::vec4:
            return "VEC4";
        case accessor_type::mat2:
            return "MAT2";
        case accessor_type::mat3:
            return "MAT3";
        case accessor_type::mat4:
            return "MAT4";
    }

    return std::to_string(static_cast<int>(accessor_type));
}


std::string sandbox::gltf::to_string(sandbox::gltf::component_type component_type)
{
    switch (component_type) {
        case component_type::unsigned_byte:
            return "UNSIGNED_BYTE";
        case component_type::unsigned_short:
            return "UNSIGNED_SHORT";
        case component_type::float32:
            return "FLOAT";
        case component_type::signed_byte:
            return "SIGNED_BYTE";
        case component_type::signed_short:
            return "SIGNED_SHORT";
        case component_type::unsigned_int:
            return "UNSIGNED_INT";
    }

    return std::to_string(static_cast<int>(component_type));
}


std::string sandbox::gltf::to_string(sandbox::gltf::alpha_mode alpha_mode)
{
    switch (alpha_mode) {
        case alpha_mode::opaque:
            return "OPAQUE";
        case alpha_mode::mask:
            return "MASK";
        case alpha_mode::blend:
            return "BLEND";
    }

    return std::to_string(static_cast<int>(alpha_mode));
}


size_t sandbox::gltf::get_buffer_element_size(
    sandbox::gltf::accessor_type accessor_type,
    sandbox::gltf::component_type component_type)
{
    switch (accessor_type) {
        case accessor_type::scalar:
            return get_component_type_size(component_type);
        case accessor_type::vec2:
            return 2 * get_component_type_size(component_type);
        case accessor_type::vec3:
            return 3 * get_component_type_size(component_type);
        case accessor_type::vec4:
            return 4 * get_component_type_size(component_type);
        case accessor_type::mat2:
            return 2 * 2 * get_component_type_size(component_type);
        case accessor_type::mat3:
            return 3 * 3 * get_component_type_size(component_type);
        case accessor_type::mat4:
            return 4 * 4 * get_component_type_size(component_type);
    }
    return 0;
}


size_t sandbox::gltf::get_component_type_size(sandbox::gltf::component_type component_type)
{
    switch (component_type) {
        case component_type::signed_byte:
            return 1;
        case component_type::unsigned_byte:
            return 1;
        case component_type::signed_short:
            return 2;
        case component_type::unsigned_short:
            return 2;
        case component_type::unsigned_int:
            return 4;
        case component_type::float32:
            return 4;
        default:
            throw std::runtime_error("Bad component type " + to_string(component_type));
    }
}


void sandbox::gltf::do_if_found(
    const nlohmann::json& where,
    const std::string& what,
    const std::function<void(const nlohmann::json&)>& callback)
{
    if (where.find(what) != where.end()) {
        callback(where[what]);
    }
};


sandbox::gltf::accessor_data sandbox::gltf::extract_accessor_data_from_buffer(
    const nlohmann::json& gltf,
    const nlohmann::json& accessor)
{
    const auto& buffer_view = gltf["bufferViews"][accessor["bufferView"].get<uint32_t>()];
    const auto [buffer, buffer_view_offset, buffer_view_length] = extract_buffer_view_data(buffer_view);

    gltf::accessor_data result{
        .buffer = buffer,
        .buffer_offset = buffer_view_offset + accessor["byteOffset"].get<uint64_t>(),
        .component_type = static_cast<gltf::component_type>(accessor["componentType"].get<uint32_t>()),
        .accessor_type = accessor_type_value(accessor["type"].get<std::string>()),
        .count = accessor["count"].get<size_t>(),
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


std::tuple<uint32_t, uint64_t, size_t> sandbox::gltf::extract_buffer_view_data(
    const nlohmann::json& buffer_view)
{
    const auto offset = buffer_view["byteOffset"].get<uint64_t>();
    const auto data_length = buffer_view["byteLength"].get<uint64_t>();
    const auto buffer = buffer_view["buffer"].get<uint32_t>();

    return {buffer, offset, data_length};
}