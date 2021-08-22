

#include "glb_parser.hpp"

#include <stdexcept>

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


sandbox::gltf::glb_file sandbox::gltf::parse_glb_file(const sandbox::utils::data& data)
{
    const uint8_t* current = data.get_data();

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
