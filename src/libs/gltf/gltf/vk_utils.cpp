#include "vk_utils.hpp"
#include "gltf_vk.hpp"

#include <render/vk/utils.hpp>
#include <utils/conditions_helpers.hpp>

using namespace sandbox;
using namespace sandbox::hal::render;


namespace
{
    void for_each_material_texture(
        const gltf::material& material,
        const std::function<void(const gltf::material::texture_data& tex_data)> callback)
    {
        if (material.get_pbr_metallic_roughness().base_color_texture.index >= 0) {
            callback(material.get_pbr_metallic_roughness().base_color_texture);
        }

        if (material.get_pbr_metallic_roughness().metallic_roughness_texture.index >= 0) {
            callback(material.get_pbr_metallic_roughness().metallic_roughness_texture);
        }

        if (material.get_normal_texture().index >= 0) {
            callback(material.get_normal_texture());
        }

        if (material.get_occlusion_texture().index >= 0) {
            callback(material.get_occlusion_texture());
        }

        if (material.get_emissive_texture().index >= 0) {
            callback(material.get_emissive_texture());
        }
    }
} // namespace


vk::IndexType sandbox::gltf::to_vk_index_type(
    sandbox::gltf::accessor_type accessor_type,
    sandbox::gltf::component_type component_type)
{
    CHECK_MSG(
        accessor_type == accessor_type::scalar,
        "Cannot convert non scalar accessor type into vulkan index type.");

    switch (component_type) {
        case component_type::unsigned_byte:
            return vk::IndexType::eUint8EXT;
        case component_type::unsigned_short:
            return vk::IndexType::eUint16;
        case component_type::unsigned_int:
            return vk::IndexType::eUint32;
        default:
            throw std::runtime_error(
                "Cannot convert " + to_string(component_type) + " component type into vulkan index type.");
    }
}


vk::Format sandbox::gltf::to_vk_format(
    sandbox::gltf::accessor_type accessor_type,
    sandbox::gltf::component_type component_type)
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


void sandbox::gltf::draw_primitive(
    const sandbox::gltf::primitive& primitive,
    const vk::Buffer& vertex_buffer,
    const vk::Buffer& index_buffer,
    vk::CommandBuffer& command_buffer)
{
    const auto& primitive_impl = static_cast<const gltf::gltf_vk::primitive&>(primitive);
    const vk::DeviceSize vertex_buffer_offset = primitive_impl.get_vertex_buffer_offset();

    thread_local static std::vector<vk::Buffer> buffers{};
    thread_local static std::vector<vk::DeviceSize> offsets{};

    buffers.clear();
    offsets.clear();

    buffers.insert(buffers.begin(), primitive_impl.get_vertex_bindings().size(), vertex_buffer);
    offsets.insert(offsets.begin(), primitive_impl.get_vertex_bindings().size(), vertex_buffer_offset);

    command_buffer.bindVertexBuffers(0, buffers.size(), buffers.data(), offsets.data());

    if (primitive_impl.get_index_type() != vk::IndexType::eNoneKHR) {
        command_buffer.bindIndexBuffer(index_buffer, primitive_impl.get_index_buffer_offset(), primitive_impl.get_index_type());
        command_buffer.drawIndexed(primitive_impl.get_indices_data().indices_count, 1, 0, 0, 0);
    } else {
        command_buffer.draw(primitive_impl.get_vertices_count(), 1, 0, 0);
    }
}


vk::SamplerAddressMode sandbox::gltf::to_vk_sampler_wrap(sandbox::gltf::sampler_wrap_type wrap)
{
    switch (wrap) {
        case sampler_wrap_type::repeat:
            return vk::SamplerAddressMode::eRepeat;
        case sampler_wrap_type::clamp_to_edge:
            return vk::SamplerAddressMode::eClampToEdge;
        case sampler_wrap_type::mirrored_repeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        default:
            throw std::runtime_error("Bad sampler wrap type.");
    }
}


std::pair<vk::Filter, vk::SamplerMipmapMode> sandbox::gltf::to_vk_sampler_filter(sandbox::gltf::sampler_filter_type filter)
{
    switch (filter) {
        case sampler_filter_type::nearest:
            [[fallthrough]];
        case sampler_filter_type::near_mipmap_nearest:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest};
        case sampler_filter_type::linear:
            [[fallthrough]];
        case sampler_filter_type::linear_mipmap_nearest:
            return {vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest};
        case sampler_filter_type::near_mipmap_linear:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};
        case sampler_filter_type::linear_mipmap_linear:
            return {vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
        default:
            throw std::runtime_error("Bad sampler filter type.");
    }
}


bool sandbox::gltf::need_mips(sandbox::gltf::sampler_filter_type filter)
{
    // clang-format off
    return filter == sampler_filter_type::linear_mipmap_linear ||
           filter == sampler_filter_type::near_mipmap_nearest ||
           filter == sampler_filter_type::linear_mipmap_nearest ||
           filter == sampler_filter_type::near_mipmap_linear;
    // clang-format on
}


sandbox::hal::render::avk::graphics_pipeline sandbox::gltf::create_pipeline_from_primitive(
    const sandbox::gltf::primitive& primitive,
    const std::vector<std::pair<vk::ShaderModule, vk::ShaderStageFlagBits>>& stages,
    vk::PipelineLayout layout,
    vk::RenderPass pass,
    uint32_t subpass,
    sandbox::gltf::pipeline_primitive_topology topology,
    sandbox::gltf::pipeline_blend_mode blend,
    bool backfaces,
    bool zwrite,
    bool ztest,
    bool color_write)
{
    // clang-format off

    auto vk_bool = [](bool flag) {
    return flag ? VK_TRUE : VK_FALSE;
    };

    const auto& primitive_impl = static_cast<const gltf::gltf_vk::primitive&>(primitive);

    std::vector<vk::PipelineShaderStageCreateInfo> pipeline_stages{};
    pipeline_stages.reserve(stages.size());

    for (const auto [module, stage] : stages) {
        pipeline_stages.emplace_back(vk::PipelineShaderStageCreateInfo{
            .flags = {},
            .stage = stage,
            .module = module,
            .pName = "main",
        });
    }

    const auto& vert_attrs = primitive_impl.get_vertex_attributes();
    const auto& vert_bindings = primitive_impl.get_vertex_bindings();

    vk::PipelineVertexInputStateCreateInfo vertex_input{
        .flags = {},
        .vertexBindingDescriptionCount = static_cast<uint32_t>(vert_bindings.size()),
        .pVertexBindingDescriptions = vert_bindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vert_attrs.size()),
        .pVertexAttributeDescriptions = vert_attrs.data()
    };

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .flags = {},
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = VK_FALSE
    };

    vk::Bool32 blend_enabled{VK_FALSE};
    vk::ColorComponentFlags color_write_mask{0};

    switch (blend) {
        case pipeline_blend_mode::none:
            break;
    }

    if (color_write) {
        color_write_mask =
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA;
    }

    vk::PipelineColorBlendAttachmentState attachment_blend{
        .blendEnable = blend_enabled,
        .colorWriteMask = color_write_mask,
    };

    vk::PipelineColorBlendStateCreateInfo color_blend{
        .flags = {},
        .logicOpEnable = VK_FALSE,
        .logicOp = {},
        .attachmentCount = 1,
        .pAttachments = &attachment_blend
    };

    vk::PolygonMode polygon_mode{};

    vk::CullModeFlags cull_mode = backfaces ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack;

    switch (topology) {
        case pipeline_primitive_topology::triangles:
            polygon_mode = vk::PolygonMode::eFill;
            break;
        case pipeline_primitive_topology::lines:
            polygon_mode = vk::PolygonMode::eLine;
            cull_mode = vk::CullModeFlagBits::eNone;
            break;
        case pipeline_primitive_topology::points:
            polygon_mode = vk::PolygonMode::ePoint;
            cull_mode = vk::CullModeFlagBits::eNone;
            break;
    }

    vk::PipelineRasterizationStateCreateInfo rasterization{
        .flags = {},
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = polygon_mode,
        .cullMode = cull_mode,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = VK_FALSE,
    };

    vk::PipelineViewportStateCreateInfo viewport{
        .flags = {},
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        .flags = {},
        .depthTestEnable = vk_bool(ztest),
        .depthWriteEnable = vk_bool(zwrite),
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    vk::SampleMask sampleMask{};
    vk::PipelineMultisampleStateCreateInfo multisample{
        .flags = {},
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };


    vk::DynamicState dyn_states[]{
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state{
        .flags = {},
        .dynamicStateCount = std::size(dyn_states),
        .pDynamicStates = dyn_states,
    };

    return avk::create_graphics_pipeline(
        avk::context::device()->createGraphicsPipeline(
            {},
            vk::GraphicsPipelineCreateInfo{
                .stageCount = static_cast<uint32_t>(pipeline_stages.size()),
                .pStages = pipeline_stages.data(),
                .pVertexInputState = &vertex_input,
                .pInputAssemblyState = &input_assembly,
                .pTessellationState = nullptr,
                .pViewportState = &viewport,
                .pRasterizationState = &rasterization,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blend,
                .pDynamicState = &dynamic_state,
                .layout = layout,

                .renderPass = pass,
                .subpass = subpass,
                .basePipelineHandle = {},
                .basePipelineIndex = -1,
                }).value);

    // clang-format on
}


vk::Format sandbox::gltf::stb_channels_count_to_vk_format(int32_t count)
{
    switch (count) {
        case 1:
            return vk::Format::eR8Srgb;
        case 2:
            return vk::Format::eR8G8Srgb;
        case 3:
            return vk::Format::eR8G8B8Srgb;
        case 4:
            return vk::Format::eR8G8B8A8Srgb;
        default:
            throw std::runtime_error("Bad components count");
    }
}


sandbox::hal::render::avk::descriptor_set_layout sandbox::gltf::create_material_textures_layout(
    const sandbox::gltf::material& material)
{
    uint32_t textures_count = 0;

    for_each_material_texture(material, [&textures_count](const auto& tex_data) {
        textures_count++;
    });

    return avk::gen_descriptor_set_layout(textures_count, vk::DescriptorType::eCombinedImageSampler);
}


void sandbox::gltf::write_material_textures_descriptors(
    const sandbox::gltf::material& material,
    vk::DescriptorSet dst_set,
    const std::vector<std::unique_ptr<gltf::texture>>& textures,
    const std::vector<std::unique_ptr<gltf::image>>& images)
{
    std::vector<vk::ImageView> vk_images{};
    std::vector<vk::Sampler> vk_samplers{};

    for_each_material_texture(material, [&vk_images, &vk_samplers, &textures, &images](const gltf::material::texture_data& texture_data) {
        const auto& curr_tex = static_cast<const gltf_vk::texture&>(*textures[texture_data.index]);
        const auto& curr_image = static_cast<const gltf_vk::image&>(*images[curr_tex.get_image()]);
        vk_images.emplace_back(curr_image.get_vk_image_view());
        vk_samplers.emplace_back(curr_tex.get_vk_sampler());
    });

    return avk::write_texture_descriptors(dst_set, vk_images, vk_samplers);
}


sandbox::gltf::vk_material_info sandbox::gltf::vk_material_info::from_gltf_material(const sandbox::gltf::material& material)
{
    auto get_texture_cords_set = [](const gltf::material::texture_data& tex_data) {
        if (tex_data.index < 0) {
            return -1;
        } else {
            return static_cast<int32_t>(tex_data.coord_set);
        }
    };

    const auto& pbr_metallic_roughness = material.get_pbr_metallic_roughness();

    return {
        .base_color = pbr_metallic_roughness.base_color,
        .base_color_texture_set = get_texture_cords_set(pbr_metallic_roughness.base_color_texture),
        .metallic_factor = pbr_metallic_roughness.metallic_factor,
        .roughness_factor = pbr_metallic_roughness.roughness_factor,
        .metallic_roughness_texture_set = get_texture_cords_set(pbr_metallic_roughness.metallic_roughness_texture),
        .normal_texture_set = get_texture_cords_set(material.get_normal_texture()),
        .occlusion_texture_set = get_texture_cords_set(material.get_occlusion_texture()),
        .emissive_texture_set = get_texture_cords_set(material.get_emissive_texture()),
        .emissive_factor = material.get_emissive_factor(),
        .alpha_mode = static_cast<int32_t>(material.get_alpha_mode()),
        .alpha_cutoff = material.get_alpha_cutoff(),
        .double_sided = material.is_double_sided()};
}
