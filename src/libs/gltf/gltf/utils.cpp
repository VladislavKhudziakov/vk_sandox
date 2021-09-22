
#include "utils.hpp"

#include <string>
#include <stdexcept>
#include <cstring>


namespace
{
    constexpr uint32_t magic_value = 0x46546C67; // "glTF"

    template<typename T>
    T read(const uint8_t** ptr)
    {
        T res{};
        std::memcpy(&res, *ptr, sizeof(T));
        *ptr += sizeof(T);
        return res;
    }
} // namespace


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


sandbox::gltf::attribute_path_value::attribute_path_value(const char* value)
{
    if (strcmp(ATTRIBUTE_PATH_POSITION, value) == 0) {
        type = attribute_path::position;
    } else if (strcmp(ATTRIBUTE_PATH_NORMAL, value) == 0) {
        type = attribute_path::normal;
    } else if (strcmp(ATTRIBUTE_PATH_TANGENT, value) == 0) {
        type = attribute_path::tangent;
    } else if (strcmp(ATTRIBUTE_PATH_TEXCOORD_0, value) == 0) {
        type = attribute_path::texcoord_0;
    } else if (strcmp(ATTRIBUTE_PATH_TEXCOORD_1, value) == 0) {
        type = attribute_path::texcoord_1;
    } else if (strcmp(ATTRIBUTE_PATH_COLOR_0, value) == 0) {
        type = attribute_path::color_0;
    } else if (strcmp(ATTRIBUTE_PATH_JOINTS_0, value) == 0) {
        type = attribute_path::joints_0;
    } else if (strcmp(ATTRIBUTE_PATH_WEIGHTS_0, value) == 0) {
        type = attribute_path::weights_0;
    } else {
        throw std::runtime_error("Bad accessor type value " + std::string(value));
    }
}


sandbox::gltf::attribute_path_value::attribute_path_value(const std::string& value)
    : attribute_path_value(value.c_str())
{
}


sandbox::gltf::attribute_path_value::operator sandbox::gltf::attribute_path() const
{
    return type;
}


sandbox::gltf::glb_file sandbox::gltf::parse_glb_file(const uint8_t* data_ptr, size_t data_size)
{
    const uint8_t* current = data_ptr;

    glb_file file{};
    file.header = read<glb_header>(&current);

    if (file.header.magic != magic_value) {
        const auto* file_magic = reinterpret_cast<const char*>(file.header.magic);
        std::string err_msg = "Bad glb magic: got " + std::string(file_magic, file_magic + sizeof(magic_value));
        throw std::runtime_error(err_msg);
    }

    file.json.size = read<uint32_t>(&current);
    file.json.type = read<glb_chunk_type>(&current);

    if (file.json.type != JSON) {
        throw std::runtime_error("Bad glb file. Invalid json chunk.");
    }
    file.json.data = current;
    current += file.json.size;

    while (*current == 0x20) {
        current++;
    }

    file.bin.size = read<uint32_t>(&current);
    file.bin.type = read<glb_chunk_type>(&current);
    if (file.bin.type != BIN) {
        throw std::runtime_error("Bad glb file. Invalid binary chunk.");
    }
    file.bin.data = current;

    return file;
}


sandbox::gltf::animation_path_value::animation_path_value(const char* value)
{
    if (strcmp(ANIMATION_PATH_ROTATION, value) == 0) {
        path = animation_path::rotation;
    } else if (strcmp(ANIMATION_PATH_SCALE, value) == 0) {
        path = animation_path::scale;
    } else if (strcmp(ANIMATION_PATH_TRANSLATION, value) == 0) {
        path = animation_path::translation;
    } else if (strcmp(ANIMATION_PATH_WEIGHTS, value) == 0) {
        path = animation_path::weights;
    } else {
        throw std::runtime_error("Bad accessor type value " + std::string(value));
    }
}


sandbox::gltf::animation_path_value::animation_path_value(const std::string& value)
    : animation_path_value(value.c_str())
{
}


sandbox::gltf::animation_path_value::operator sandbox::gltf::animation_path() const
{
    return path;
}


sandbox::gltf::animation_interpolation_value::animation_interpolation_value(const char* value)
{
    if (strcmp(ANIMATION_INTERPOLATION_LINEAR, value) == 0) {
        interpolation = animation_interpolation::linear;
    } else if (strcmp(ANIMATION_INTERPOLATION_STEP, value) == 0) {
        interpolation = animation_interpolation::step;
    } else if (strcmp(ANIMATION_INTERPOLATION_CUBIC_SPLINE, value) == 0) {
        interpolation = animation_interpolation::cubic_spline;
    } else {
        throw std::runtime_error("Bad accessor type value " + std::string(value));
    }
}


sandbox::gltf::animation_interpolation_value::animation_interpolation_value(const std::string& value)
    : animation_interpolation_value(value.c_str())
{
}


sandbox::gltf::animation_interpolation_value::operator sandbox::gltf::animation_interpolation() const
{
    return interpolation;
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
    } else if (strcmp("", value) != 0) {
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
}
