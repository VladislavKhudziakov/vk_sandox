

#include <filesystem/common_file.hpp>

#include <gltf/gltf_vk.hpp>

using namespace sandbox;
using namespace sandbox::hal;

int main(int argv, const char** argc)
{
    gltf::gltf_vk gltf = gltf::gltf_vk::from_url("./resources/Box.glb");

    return 0;
}
