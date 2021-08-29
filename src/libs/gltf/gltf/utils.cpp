

#include "utils.hpp"

#include <utils/conditions_helpers.hpp>

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


vk::Format sandbox::gltf::to_vk_format(sandbox::gltf::accessor_type_value accessor_type, sandbox::gltf::component_type component_type)
{
    switch (accessor_type) {
        case accessor_type::scalar:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32Uint;
                case component_type::float32:
                    return vk::Format::eR32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        case accessor_type::vec2:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8G8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8G8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16G16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16G16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32G32Uint;
                case component_type::float32:
                    return vk::Format::eR32G32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        case accessor_type::vec3:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8G8B8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8G8B8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16G16B16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16G16B16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32G32B32Uint;
                case component_type::float32:
                    return vk::Format::eR32G32B32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        case accessor_type::vec4:
            switch (component_type) {
                case component_type::signed_byte:
                    return vk::Format::eR8G8B8A8Sint;
                case component_type::unsigned_byte:
                    return vk::Format::eR8G8B8A8Uint;
                case component_type::signed_short:
                    return vk::Format::eR16G16B16A16Sint;
                case component_type::unsigned_short:
                    return vk::Format::eR16G16B16A16Uint;
                case component_type::unsigned_int:
                    return vk::Format::eR32G32B32A32Uint;
                case component_type::float32:
                    return vk::Format::eR32G32B32A32Sfloat;
                default:
                    throw std::runtime_error("Bad component type " + to_string(component_type));
            }
        default:
            throw std::runtime_error("Bad accessor type " + to_string(accessor_type));
    }
}


vk::IndexType sandbox::gltf::to_vk_index_type(sandbox::gltf::accessor_type_value accessor_type, sandbox::gltf::component_type component_type)
{
    CHECK_MSG(accessor_type == accessor_type::scalar, "Cannot convert non scalar accessor type into vulkan index type.");

    switch (component_type) {
        case component_type::unsigned_byte:
            return vk::IndexType::eUint8EXT;
        case component_type::unsigned_short:
            return vk::IndexType::eUint16;
        case component_type::unsigned_int:
            return vk::IndexType::eUint32;
        default:
            throw std::runtime_error("Cannot convert " + to_string(component_type) + " component type into vulkan index type.");
    }
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


size_t sandbox::gltf::get_buffer_element_size(sandbox::gltf::accessor_type_value accessor_type, sandbox::gltf::component_type component_type)
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
