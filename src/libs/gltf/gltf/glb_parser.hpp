#pragma once

#include <cinttypes>

#include <utils/data.hpp>

namespace sandbox::gltf
{
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

    glb_file parse_glb_file(const utils::data& data);
} // namespace sandbox::gltf
