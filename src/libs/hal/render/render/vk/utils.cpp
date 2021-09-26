#include "utils.hpp"

#include <render/vk/errors_handling.hpp>

#include <utils/scope_helpers.hpp>
#include <utils/conditions_helpers.hpp>

#include <map>

using namespace sandbox::hal::render;

namespace
{
    std::map<VkFormat, sandbox::hal::render::avk::vk_format_info> vk_format_table = {
        {VK_FORMAT_UNDEFINED, {0, 0}},
        {VK_FORMAT_R4G4_UNORM_PACK8, {1, 2}},
        {VK_FORMAT_R4G4B4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B4G4R4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R5G6B5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_B5G6R5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_R5G5B5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B5G5R5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_A1R5G5B5_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R8_UNORM, {1, 1}},
        {VK_FORMAT_R8_SNORM, {1, 1}},
        {VK_FORMAT_R8_USCALED, {1, 1}},
        {VK_FORMAT_R8_SSCALED, {1, 1}},
        {VK_FORMAT_R8_UINT, {1, 1}},
        {VK_FORMAT_R8_SINT, {1, 1}},
        {VK_FORMAT_R8_SRGB, {1, 1}},
        {VK_FORMAT_R8G8_UNORM, {2, 2}},
        {VK_FORMAT_R8G8_SNORM, {2, 2}},
        {VK_FORMAT_R8G8_USCALED, {2, 2}},
        {VK_FORMAT_R8G8_SSCALED, {2, 2}},
        {VK_FORMAT_R8G8_UINT, {2, 2}},
        {VK_FORMAT_R8G8_SINT, {2, 2}},
        {VK_FORMAT_R8G8_SRGB, {2, 2}},
        {VK_FORMAT_R8G8B8_UNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_SNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_USCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_SSCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_UINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SRGB, {3, 3}},
        {VK_FORMAT_B8G8R8_UNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_SNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_USCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_SSCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_UINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SRGB, {3, 3}},
        {VK_FORMAT_R8G8B8A8_UNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_USCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SSCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_UINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SRGB, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_USCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SSCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SRGB, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SRGB_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_R16_UNORM, {2, 1}},
        {VK_FORMAT_R16_SNORM, {2, 1}},
        {VK_FORMAT_R16_USCALED, {2, 1}},
        {VK_FORMAT_R16_SSCALED, {2, 1}},
        {VK_FORMAT_R16_UINT, {2, 1}},
        {VK_FORMAT_R16_SINT, {2, 1}},
        {VK_FORMAT_R16_SFLOAT, {2, 1}},
        {VK_FORMAT_R16G16_UNORM, {4, 2}},
        {VK_FORMAT_R16G16_SNORM, {4, 2}},
        {VK_FORMAT_R16G16_USCALED, {4, 2}},
        {VK_FORMAT_R16G16_SSCALED, {4, 2}},
        {VK_FORMAT_R16G16_UINT, {4, 2}},
        {VK_FORMAT_R16G16_SINT, {4, 2}},
        {VK_FORMAT_R16G16_SFLOAT, {4, 2}},
        {VK_FORMAT_R16G16B16_UNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_SNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_USCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_SSCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_UINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SFLOAT, {6, 3}},
        {VK_FORMAT_R16G16B16A16_UNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_USCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SSCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_UINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SFLOAT, {8, 4}},
        {VK_FORMAT_R32_UINT, {4, 1}},
        {VK_FORMAT_R32_SINT, {4, 1}},
        {VK_FORMAT_R32_SFLOAT, {4, 1}},
        {VK_FORMAT_R32G32_UINT, {8, 2}},
        {VK_FORMAT_R32G32_SINT, {8, 2}},
        {VK_FORMAT_R32G32_SFLOAT, {8, 2}},
        {VK_FORMAT_R32G32B32_UINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SFLOAT, {12, 3}},
        {VK_FORMAT_R32G32B32A32_UINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SFLOAT, {16, 4}},
        {VK_FORMAT_R64_UINT, {8, 1}},
        {VK_FORMAT_R64_SINT, {8, 1}},
        {VK_FORMAT_R64_SFLOAT, {8, 1}},
        {VK_FORMAT_R64G64_UINT, {16, 2}},
        {VK_FORMAT_R64G64_SINT, {16, 2}},
        {VK_FORMAT_R64G64_SFLOAT, {16, 2}},
        {VK_FORMAT_R64G64B64_UINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SFLOAT, {24, 3}},
        {VK_FORMAT_R64G64B64A64_UINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SFLOAT, {32, 4}},
        {VK_FORMAT_B10G11R11_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_D16_UNORM, {2, 1}},
        {VK_FORMAT_X8_D24_UNORM_PACK32, {4, 1}},
        {VK_FORMAT_D32_SFLOAT, {4, 1}},
        {VK_FORMAT_S8_UINT, {1, 1}},
        {VK_FORMAT_D16_UNORM_S8_UINT, {3, 2}},
        {VK_FORMAT_D24_UNORM_S8_UINT, {4, 2}},
        {VK_FORMAT_D32_SFLOAT_S8_UINT, {8, 2}},
        {VK_FORMAT_BC1_RGB_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGB_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC2_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC2_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC4_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC4_SNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC5_SNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_UFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_SFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_EAC_R11_UNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11_SNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11G11_UNORM_BLOCK, {16, 2}},
        {VK_FORMAT_EAC_R11G11_SNORM_BLOCK, {16, 2}},
        {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_4x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG, {8, 4}},
        // KHR_sampler_YCbCr_conversion extension - single-plane variants
        // 'PACK' formats are normal, uncompressed
        {VK_FORMAT_R10X6_UNORM_PACK16, {2, 1}},
        {VK_FORMAT_R10X6G10X6_UNORM_2PACK16, {4, 2}},
        {VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_R12X4_UNORM_PACK16, {2, 1}},
        {VK_FORMAT_R12X4G12X4_UNORM_2PACK16, {4, 2}},
        {VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16, {8, 4}},
        // _422 formats encode 2 texels per entry with B, R components shared - treated as compressed w/ 2x1 block size
        {VK_FORMAT_G8B8G8R8_422_UNORM, {4, 4}},
        {VK_FORMAT_B8G8R8G8_422_UNORM, {4, 4}},
        {VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16, {8, 4}},
        {VK_FORMAT_G16B16G16R16_422_UNORM, {8, 4}},
        {VK_FORMAT_B16G16R16G16_422_UNORM, {8, 4}},
        // KHR_sampler_YCbCr_conversion extension - multi-plane variants
        // Formats that 'share' components among texels (_420 and _422), size represents total bytes for the smallest possible texel block
        // _420 share B, R components within a 2x2 texel block
        {VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, {6, 3}},
        {VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, {6, 3}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16, {12, 3}},
        {VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM, {12, 3}},
        {VK_FORMAT_G16_B16R16_2PLANE_420_UNORM, {12, 3}},
        // _422 share B, R components within a 2x1 texel block
        {VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM, {4, 3}},
        {VK_FORMAT_G8_B8R8_2PLANE_422_UNORM, {4, 3}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16, {8, 3}},
        {VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM, {8, 3}},
        {VK_FORMAT_G16_B16R16_2PLANE_422_UNORM, {8, 3}},
        // _444 do not share
        {VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM, {3, 3}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, {6, 3}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16, {6, 3}},
        {VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM, {6, 3}}};
}

std::pair<avk::vma_image, avk::image_view> sandbox::hal::render::avk::gen_attachment(
    uint32_t width,
    uint32_t height,
    vk::Format format,
    uint32_t queue_family,
    vk::ImageUsageFlags usage,
    vk::SampleCountFlagBits samples)
{
    // clang-format off
    bool is_depth =
        format == vk::Format::eD24UnormS8Uint ||
        format == vk::Format::eD16Unorm ||
        format == vk::Format::eD16UnormS8Uint ||
        format == vk::Format::eD32Sfloat ||
        format == vk::Format::eD32SfloatS8Uint;
    // clang-format on

    if (is_depth) {
        usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    } else {
        usage |= vk::ImageUsageFlagBits::eColorAttachment;
    }

    auto image = avk::create_vma_image(
        vk::ImageCreateInfo{
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = samples,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family,
            .initialLayout = vk::ImageLayout::eUndefined,
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        });

    auto image_view = avk::create_image_view(
        avk::context::device()->createImageView(
            vk::ImageViewCreateInfo{
                .flags = {},
                .image = image.as<vk::Image>(),
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .components = {
                    .r = vk::ComponentSwizzle::eR,
                    .g = vk::ComponentSwizzle::eG,
                    .b = vk::ComponentSwizzle::eB,
                    .a = vk::ComponentSwizzle::eA,
                },
                .subresourceRange = {
                    .aspectMask = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            }));

    return {std::move(image), std::move(image_view)};
}


avk::vma_buffer avk::gen_staging_buffer(
    uint32_t queue_family, size_t data_size, const std::function<void(uint8_t*)>& on_buffer_mapped)
{
    auto buffer = avk::create_vma_buffer(
        vk::BufferCreateInfo{
            .size = data_size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family,
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    if (!on_buffer_mapped) {
        return buffer;
    }

    {
        utils::on_scope_exit exit([&buffer]() {
            auto allocation = buffer.as<VmaAllocation>();
            VkDeviceSize offset = 0;
            VkDeviceSize size = VK_WHOLE_SIZE;

            vmaUnmapMemory(avk::context::allocator(), buffer.as<VmaAllocation>());
            vmaFlushAllocations(avk::context::allocator(), 1, &allocation, &offset, &size);
        });

        void* mapped_data{nullptr};
        VK_C_CALL(vmaMapMemory(avk::context::allocator(), buffer.as<VmaAllocation>(), &mapped_data));
        assert(mapped_data != nullptr);
        on_buffer_mapped(reinterpret_cast<uint8_t*>(mapped_data));
    }

    return buffer;
}


avk::framebuffer avk::gen_framebuffer(
    uint32_t width, uint32_t height, const vk::RenderPass& pass, const vk::ImageView* attachments, uint32_t attachments_count)
{
    return avk::create_framebuffer(
        avk::context::device()->createFramebuffer(
            vk::FramebufferCreateInfo{
                .flags = {},
                .renderPass = pass,

                .attachmentCount = attachments_count,
                .pAttachments = attachments,

                .width = width,
                .height = height,
                .layers = 1,
            }));
}


std::tuple<avk::framebuffer, std::vector<avk::vma_image>, std::vector<avk::image_view>>
avk::gen_framebuffer(
    uint32_t width,
    uint32_t height,
    uint32_t queue_family,
    const vk::RenderPassCreateInfo& pass_info,
    const vk::RenderPass& pass,
    const std::vector<float>& attachments_scales,
    const std::vector<vk::ImageUsageFlagBits>& attachments_usages)
{
    std::vector<avk::vma_image> images{};
    std::vector<avk::image_view> images_views{};
    std::vector<vk::ImageView> image_views_native{};

    images.reserve(pass_info.attachmentCount);
    images_views.reserve(pass_info.attachmentCount);
    image_views_native.reserve(pass_info.attachmentCount);

    for (int i = 0; i < pass_info.attachmentCount; ++i) {
        const auto& attachment = pass_info.pAttachments[i];

        uint32_t attachment_width = width;
        uint32_t attachment_height = height;
        vk::ImageUsageFlagBits attachment_usage = {};

        if (i < attachments_scales.size() && attachments_scales[i] > 0) {
            attachment_width *= attachments_scales[i];
            attachment_height *= attachments_scales[i];
        }

        if (i < attachments_usages.size()) {
            attachment_usage = attachments_usages[i];
        }

        auto [image, view] = gen_attachment(
            attachment_width,
            attachment_height,
            attachment.format,
            queue_family,
            attachment_usage,
            attachment.samples);

        vk::ImageView native_view = view;

        images.emplace_back(std::move(image));
        images_views.emplace_back(std::move(view));
        image_views_native.emplace_back(native_view);
    }

    auto framebuffer = gen_framebuffer(width, height, pass, image_views_native.data(), image_views_native.size());

    return std::make_tuple(std::move(framebuffer), std::move(images), std::move(images_views));
}


void avk::copy_buffer_to_image(
    vk::CommandBuffer command_buffer,
    const vk::Buffer& staging_buffer,
    const vk::Image& dst_image,
    uint32_t image_width,
    uint32_t image_height,
    uint32_t image_level_count,
    uint32_t image_layers_count,
    size_t buffer_offset)
{
    // clang-format off
    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {}, {}, {},
        {vk::ImageMemoryBarrier{
            .srcAccessMask = {},
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = image_level_count,
                .baseArrayLayer = 0,
                .layerCount = image_layers_count
            },
        }});

    command_buffer.copyBufferToImage(
        staging_buffer,
        dst_image,
        vk::ImageLayout::eTransferDstOptimal,
        {vk::BufferImageCopy{
            .bufferOffset = static_cast<VkDeviceSize>(buffer_offset),
            .bufferRowLength = image_width,
            .bufferImageHeight = image_height,

            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },

            .imageOffset = {
                0, 0, 0
            },

            .imageExtent = {
                image_width, image_height, 1
            },
        }});

    vk::PipelineStageFlags all_shaders_flags =
        vk::PipelineStageFlagBits::eVertexShader |
        vk::PipelineStageFlagBits::eGeometryShader |
        vk::PipelineStageFlagBits::eTessellationControlShader |
        vk::PipelineStageFlagBits::eTessellationEvaluationShader |
        vk::PipelineStageFlagBits::eComputeShader |
        vk::PipelineStageFlagBits::eFragmentShader;


    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        all_shaders_flags,
        {}, {}, {},
        {vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = image_level_count,
                .baseArrayLayer = 0,
                .layerCount = image_layers_count
                },
            }});

    // clang-format on
}


avk::descriptor_set_layout avk::gen_descriptor_set_layout(uint32_t count, vk::DescriptorType type)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings{};
    bindings.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        bindings.emplace_back(vk::DescriptorSetLayoutBinding{
            .binding = i,
            .descriptorType = type,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll,
            .pImmutableSamplers = nullptr});
    }

    return avk::create_descriptor_set_layout(avk::context::device()->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()}));
}


std::pair<avk::descriptor_pool, avk::descriptor_set_list> avk::gen_descriptor_sets(
    const std::vector<vk::DescriptorSetLayout>& layouts,
    const std::vector<std::pair<uint32_t, vk::DescriptorType>>& layouts_data)
{
    CHECK(layouts.size() == layouts_data.size());

    std::vector<vk::DescriptorPoolSize> descriptor_pool_sizes{};
    descriptor_pool_sizes.reserve(layouts_data.size());

    for (const auto [desc_count, desc_type] : layouts_data) {
        descriptor_pool_sizes.emplace_back(vk::DescriptorPoolSize{
            .type = desc_type,
            .descriptorCount = desc_count,
        });
    }

    auto descriptor_pool = avk::create_descriptor_pool(avk::context::device()->createDescriptorPool(
        vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = static_cast<uint32_t>(layouts.size()),
            .poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size()),
            .pPoolSizes = descriptor_pool_sizes.data(),
        }));

    auto descriptor_sets = avk::allocate_descriptor_sets(vk::DescriptorSetAllocateInfo{
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()});

    return std::make_pair(std::move(descriptor_pool), std::move(descriptor_sets));
}


void avk::write_texture_descriptors(
    vk::DescriptorSet dst_set,
    const std::vector<vk::ImageView>& images_views,
    const std::vector<vk::Sampler>& samplers)
{
    if (images_views.empty()) {
        return;
    }
    CHECK(images_views.size() == samplers.size());

    std::vector<vk::DescriptorImageInfo> images_infos{};
    images_infos.reserve(images_views.size());

    for (int i = 0; i < images_views.size(); ++i) {
        images_infos.emplace_back(vk::DescriptorImageInfo{
            .sampler = samplers[i],
            .imageView = images_views[i],
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal});
    }

    // clang-format off
    avk::context::device()->updateDescriptorSets({
        vk::WriteDescriptorSet{
            .dstSet = dst_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(images_infos.size()),
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = images_infos.data(),
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }},
        {});
    // clang-format on
}


void avk::write_buffer_descriptors(
    vk::DescriptorSet dst_set,
    const std::vector<vk::Buffer>& buffers,
    const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>& sizes)
{
    CHECK(buffers.size() == sizes.size());

    std::vector<vk::DescriptorBufferInfo> buffers_infos{};

    for (int i = 0; i < buffers.size(); ++i) {
        buffers_infos.emplace_back(vk::DescriptorBufferInfo{
            .buffer = buffers[i],
            .offset = sizes[i].first,
            .range = sizes[i].second});
    }

    // clang-format off
    avk::context::device()->updateDescriptorSets({
        vk::WriteDescriptorSet{
            .dstSet = dst_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(buffers_infos.size()),
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = buffers_infos.data(),
            .pTexelBufferView = nullptr
        }}, {});
    // clang-format on
}


avk::pipeline_layout avk::gen_pipeline_layout(
    const std::vector<vk::PushConstantRange>& push_constants,
    const std::vector<vk::DescriptorSetLayout>& desc_set_layouts)
{
    return avk::create_pipeline_layout(avk::context::device()->createPipelineLayout(vk::PipelineLayoutCreateInfo{
        .setLayoutCount = static_cast<uint32_t>(desc_set_layouts.size()),
        .pSetLayouts = desc_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
        .pPushConstantRanges = push_constants.data()}));
}


std::pair<avk::vma_image, avk::image_view> avk::gen_texture(
    uint32_t queue_family,
    vk::ImageViewType type,
    vk::Format format,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    uint32_t levels,
    uint32_t layers)
{
    CHECK_MSG(width > 0, "width must be greater 0.");
    CHECK_MSG(height > 0, "height must be greater 0.");
    CHECK_MSG(depth > 0, "depth must be greater 0.");
    CHECK_MSG(levels > 0, "levels must be greater 0.");
    CHECK_MSG(layers > 0, "layers must be greater 0.");

    // clang-format off
    bool is_depth =
        format == vk::Format::eD24UnormS8Uint ||
        format == vk::Format::eD16Unorm ||
        format == vk::Format::eD16UnormS8Uint ||
        format == vk::Format::eD32Sfloat ||
        format == vk::Format::eD32SfloatS8Uint;
    // clang-format on

    vk::ImageType img_type{};

    vk::ImageCreateFlags image_create_flags{};

    switch (type) {
        case vk::ImageViewType::e1DArray:
            [[fallthrough]];
        case vk::ImageViewType::e1D:
            img_type = vk::ImageType::e1D;
            break;
        case vk::ImageViewType::eCube:
            image_create_flags |= vk::ImageCreateFlagBits::eCubeCompatible;
            [[fallthrough]];
        case vk::ImageViewType::eCubeArray:
            CHECK_MSG(layers % 6 == 0, "multiple 6 layers count in cubemaps required.");
            image_create_flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
            img_type = vk::ImageType::e2D;
            break;
        case vk::ImageViewType::e2DArray:
            image_create_flags = vk::ImageCreateFlagBits::e2DArrayCompatible;
            [[fallthrough]];
        case vk::ImageViewType::e2D:
            img_type = vk::ImageType::e2D;
            break;
        case vk::ImageViewType::e3D:
            img_type = vk::ImageType::e3D;
            break;
    }

    auto image = avk::create_vma_image(
        vk::ImageCreateInfo{
            .flags = image_create_flags,
            .imageType = img_type,
            .format = format,
            .extent = {
                .width = width,
                .height = height,
                .depth = depth,
            },
            .mipLevels = levels,
            .arrayLayers = layers,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family,
            .initialLayout = vk::ImageLayout::eUndefined,
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        });

    vk::ImageViewCreateFlags flags{};

    auto image_view = avk::create_image_view(
        avk::context::device()->createImageView(
            vk::ImageViewCreateInfo{
                .flags = {},
                .image = image.as<vk::Image>(),
                .viewType = type,
                .format = format,
                .components = {
                    .r = vk::ComponentSwizzle::eR,
                    .g = vk::ComponentSwizzle::eG,
                    .b = vk::ComponentSwizzle::eB,
                    .a = vk::ComponentSwizzle::eA,
                },
                .subresourceRange = {
                    .aspectMask = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = levels,
                    .baseArrayLayer = 0,
                    .layerCount = layers,
                },
            }));

    return std::make_pair(std::move(image), std::move(image_view));
}
avk::sampler avk::gen_sampler(
    vk::Filter min_filter,
    vk::Filter mag_filter,
    vk::SamplerMipmapMode mip_filter,
    vk::SamplerAddressMode addr_mode_u,
    vk::SamplerAddressMode addr_mode_v,
    vk::SamplerAddressMode addr_mode_w,
    float max_lod,
    bool anisotropy)
{
    return avk::create_sampler(avk::context::device()->createSampler(
        vk::SamplerCreateInfo{
            .magFilter = min_filter,
            .minFilter = mag_filter,
            .mipmapMode = mip_filter,

            .addressModeU = addr_mode_u,
            .addressModeV = addr_mode_v,
            .addressModeW = addr_mode_w,
            .mipLodBias = 0,

            .anisotropyEnable = VK_FALSE,
            .compareEnable = VK_FALSE,

            .minLod = 0,
            .maxLod = max_lod,

            .borderColor = vk::BorderColor::eFloatOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE}));
}


std::pair<avk::vma_buffer, avk::vma_buffer> avk::gen_buffer(
    vk::CommandBuffer& command_buffer,
    uint32_t queue_family,
    uint32_t buffer_size,
    vk::BufferUsageFlagBits buffer_usage,
    vk::PipelineStageFlagBits wait_stage,
    vk::AccessFlagBits access_flags,
    const std::function<void(const uint8_t*)>& on_mapped_callback)
{
    auto staging_buffer = avk::gen_staging_buffer(
        queue_family,
        buffer_size,
        on_mapped_callback);

    auto result_buffer = avk::create_vma_buffer(
        vk::BufferCreateInfo{
            .flags = {},
            .size = buffer_size,
            .usage = vk::BufferUsageFlagBits::eTransferDst | buffer_usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family},
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY});

    if (on_mapped_callback) {
        command_buffer.copyBuffer(
            staging_buffer.as<vk::Buffer>(),
            result_buffer.as<vk::Buffer>(),
            {vk::BufferCopy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = buffer_size,
            }});

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            wait_stage,
            {},
            {vk::MemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = access_flags,
            }},
            {},
            {});
    }

    return std::make_pair(std::move(staging_buffer), std::move(result_buffer));
}


void avk::upload_buffer_data(
    vk::CommandBuffer& command_buffer,
    const avk::vma_buffer& staging_buffer,
    const avk::vma_buffer& dst_buffer,
    vk::PipelineStageFlagBits dst_stage,
    vk::AccessFlagBits dst_access,
    VkDeviceSize size,
    const std::function<void(const uint8_t*)>& upload)
{
    void* dst_ptr{nullptr};
    auto res = vmaMapMemory(avk::context::allocator(), staging_buffer.as<VmaAllocation>(), &dst_ptr);

    utils::on_scope_exit scope_guard{[&staging_buffer, res]() {
        if (res == VK_SUCCESS) {
            vmaUnmapMemory(avk::context::allocator(), staging_buffer.as<VmaAllocation>());
            VkDeviceSize offset = 0;
            VkDeviceSize size = staging_buffer->get_alloc_info().size;
            VmaAllocation allocation = staging_buffer.as<VmaAllocation>();
            vmaFlushAllocations(avk::context::allocator(), 1, &allocation, &offset, &size);
        }
    }};

    if (res != VK_SUCCESS) {
        throw avk::vulkan_result_error(vk::Result(res));
    }

    upload(reinterpret_cast<const uint8_t*>(dst_ptr));

    // clang-format off
    command_buffer.copyBuffer(
        staging_buffer.as<vk::Buffer>(),
        dst_buffer.as<vk::Buffer>(),
        {
            vk::BufferCopy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = size,
            }
        });

    command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        dst_stage,
        {},
        {},
        {
            vk::BufferMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = dst_access,

                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                .buffer = dst_buffer.as<vk::Buffer>(),
                .offset = 0,
                .size = size,
            }
        },
        {});
    // clang-format on
}


sandbox::hal::render::avk::vk_format_info sandbox::hal::render::avk::get_format_info(VkFormat format)
{
    return vk_format_table[format];
}


avk::render_pass_wrapper::render_pass_wrapper(avk::pass_create_info info)
    : m_info(std::move(info))
    , m_pass_info{
          .flags = {},
          .attachmentCount = static_cast<uint32_t>(m_info.attachments.size()),
          .pAttachments = m_info.attachments.data(),
          .subpassCount = static_cast<uint32_t>(m_info.subpasses.size()),
          .pSubpasses = m_info.subpasses.data(),
          .dependencyCount = static_cast<uint32_t>(m_info.subpass_dependencies.size()),
          .pDependencies = m_info.subpass_dependencies.data()}
    , m_pass_handler{avk::create_render_pass(avk::context::device()->createRenderPass(m_pass_info))}
{
}


const vk::RenderPassCreateInfo& avk::render_pass_wrapper::get_info() const
{
    return m_pass_info;
}


vk::RenderPass avk::render_pass_wrapper::get_native_pass() const
{
    return m_pass_handler.as<vk::RenderPass>();
}


avk::graphics_pipeline_builder::graphics_pipeline_builder(
    vk::RenderPass pass,
    uint32_t subpass,
    size_t attachments_count)
    : m_pass(pass)
    , m_subpass(subpass)
{
    m_attachments_blend_states.resize(
        attachments_count, vk::PipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA
       });

    m_color_blend_state.pAttachments = m_attachments_blend_states.data();
    m_color_blend_state.attachmentCount = m_attachments_blend_states.size();
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::set_vertex_format(
    const std::vector<vk::VertexInputAttributeDescription>& attrs, const std::vector<vk::VertexInputBindingDescription>& bingings)
{
    m_vertex_attributes = attrs;
    m_vertex_bingings = bingings;
    m_vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
        .flags = {},
        .vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertex_bingings.size()),
        .pVertexBindingDescriptions = m_vertex_bingings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertex_attributes.size()),
        .pVertexAttributeDescriptions = m_vertex_attributes.data()
    };

    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::set_shader_stages(
    const std::vector<std::pair<vk::ShaderModule, vk::ShaderStageFlagBits>>& stages_list)
{
    m_shader_stages.clear();
    m_shader_stages.reserve(stages_list.size());

    for (const auto& [module, stage] : stages_list){
        m_shader_stages.emplace_back(vk::PipelineShaderStageCreateInfo{
            .flags = {},
            .stage = stage,
            .module = module,
            .pName = "main",
            .pSpecializationInfo = &m_specialization_info,
        });
    }

    return *this;
}



avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::set_primitive_topology(vk::PrimitiveTopology topology)
{
    m_input_assembly_state.topology = topology;
    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::set_cull_mode(vk::CullModeFlags cull_mode)
{
    m_rasterization_state.cullMode = cull_mode;
    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::set_polygon_mode(vk::PolygonMode polygon_mode)
{
    m_rasterization_state.polygonMode = polygon_mode;
    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::enable_depth_test(bool enable)
{
    m_depth_stencil_state.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::enable_depth_write(bool enable)
{
    m_depth_stencil_state.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::set_depth_compare_op(vk::CompareOp compare_op)
{
    m_depth_stencil_state.depthCompareOp = compare_op;
    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::add_buffer(
    vk::Buffer buffer,
    VkDeviceSize offset,
    VkDeviceSize size,
    vk::DescriptorType type)
{
    m_descriptors_count[type]++;

    m_buffers_descriptor_bindings.emplace_back(vk::DescriptorSetLayoutBinding{
        .binding = static_cast<uint32_t>(m_descriptor_buffers_writes.size()),
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    });

    m_descriptor_buffers_writes.emplace_back(vk::DescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size,
    });

    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::add_buffers(
    const std::vector<vk::Buffer>& buffers,
    const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>& ranges,
    vk::DescriptorType type)
{
    assert(buffers.size() == ranges.size());
    m_buffers_descriptor_bindings.reserve(buffers.size());
    m_descriptor_buffers_writes.reserve(buffers.size());

    auto buffers_begin = buffers.begin();
    auto ranges_begin = ranges.begin();

    for (; buffers_begin != buffers.end(); ++buffers_begin, ++ranges_begin) {
        add_buffer(*buffers_begin, ranges_begin->first, ranges_begin->second, type);
    }

    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::add_texture(vk::ImageView image_view, vk::Sampler image_sampler)
{
    m_descriptors_count[vk::DescriptorType::eCombinedImageSampler]++;

    m_textures_descriptor_bindings.emplace_back(vk::DescriptorSetLayoutBinding{
        .binding = static_cast<uint32_t>(m_descriptor_images_writes.size()),
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    });

    m_descriptor_images_writes.emplace_back(vk::DescriptorImageInfo{
        .sampler = image_sampler,
        .imageView = image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    });

    return *this;
}


avk::graphics_pipeline_builder& avk::graphics_pipeline_builder::add_textures(
    const std::vector<vk::ImageView>& images,
    const std::vector<vk::Sampler>& samplers)
{
    assert(images.size() == samplers.size());
    m_textures_descriptor_bindings.reserve(m_descriptor_images_writes.size() + images.size());
    m_descriptor_images_writes.reserve(m_descriptor_images_writes.size() + images.size());

    auto images_begin = images.begin();
    auto samplers_begin = samplers.begin();

    for (; images_begin != images.end(); ++images_begin, ++samplers_begin) {
        add_texture(*images_begin, *samplers_begin);
    }

    return *this;
}


avk::_graphics_pipeline avk::graphics_pipeline_builder::create_pipeline()
{
    avk::_graphics_pipeline new_pipeline{};

    auto desc_set_layouts = create_descriptor_set_layouts();
    auto desc_set_layouts_native = avk::to_elements_list<vk::DescriptorSetLayout>(desc_set_layouts.begin(), desc_set_layouts.end());

    auto desc_pool = create_descriptor_pool(desc_set_layouts_native);
    auto desc_list = create_descriptor_sets(desc_pool, desc_set_layouts_native);
    auto pipeline_layout = create_pipeline_layout(desc_set_layouts_native);

    new_pipeline.m_buffers_descriptor_bindings = m_buffers_descriptor_bindings;
    new_pipeline.m_textures_descriptor_bindings = m_textures_descriptor_bindings;
    new_pipeline.m_push_constant_buffer = m_push_constant_buffer;
    new_pipeline.m_push_constant_ranges = m_push_constant_ranges;

    m_specialization_info.dataSize = m_spec_data.size();
    m_specialization_info.pData = m_spec_data.data();
    m_specialization_info.mapEntryCount = m_specializations_map.size();
    m_specialization_info.pMapEntries = m_specializations_map.data();

    auto pipeline = avk::create_graphics_pipeline(
        avk::context::device()->createGraphicsPipeline(
            {},
            vk::GraphicsPipelineCreateInfo{
                .stageCount = static_cast<uint32_t>(m_shader_stages.size()),
                .pStages = m_shader_stages.data(),
                .pVertexInputState = &m_vertex_input_state,
                .pInputAssemblyState = &m_input_assembly_state,
                .pTessellationState = nullptr,
                .pViewportState = &m_viewports_state,
                .pRasterizationState = &m_rasterization_state,
                .pMultisampleState = &m_multisample_state,
                .pDepthStencilState = &m_depth_stencil_state,
                .pColorBlendState = &m_color_blend_state,
                .pDynamicState = &m_dynamic_state,
                .layout = pipeline_layout,

                .renderPass = m_pass,
                .subpass = m_subpass,
                .basePipelineHandle = {},
                .basePipelineIndex = -1,
                }).value);

    new_pipeline.m_pipeline = std::move(pipeline);
    new_pipeline.m_pipeline_layout = std::move(pipeline_layout);
    new_pipeline.m_descriptor_pool = std::move(desc_pool);
    new_pipeline.m_descriptor_sets = std::move(desc_list);


    return new_pipeline;
}


avk::pipeline_layout avk::graphics_pipeline_builder::create_pipeline_layout(const std::vector<vk::DescriptorSetLayout>& layouts)
{
    return avk::gen_pipeline_layout(
        m_push_constant_ranges,
        layouts);
}


avk::descriptor_pool avk::graphics_pipeline_builder::create_descriptor_pool(
    const std::vector<vk::DescriptorSetLayout>& layouts)
{
    std::vector<vk::DescriptorPoolSize> descriptor_pool_sizes{};
    descriptor_pool_sizes.reserve(m_descriptors_count.size());

    for (const auto [type, count] : m_descriptors_count) {
        descriptor_pool_sizes.emplace_back(vk::DescriptorPoolSize{
            .type = type,
            .descriptorCount = count
        });
    }

    if (!descriptor_pool_sizes.empty()) {
        return avk::create_descriptor_pool(avk::context::device()->createDescriptorPool(
            vk::DescriptorPoolCreateInfo{
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = static_cast<uint32_t>(layouts.size()),
                .poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size()),
                .pPoolSizes = descriptor_pool_sizes.data(),
            }));
    }

    return {};
}


std::vector<avk::descriptor_set_layout> avk::graphics_pipeline_builder::create_descriptor_set_layouts()
{
    std::vector<avk::descriptor_set_layout> layouts_list{};

    if (!m_textures_descriptor_bindings.empty()) {
        layouts_list.emplace_back(avk::create_descriptor_set_layout(
            avk::context::device()->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<uint32_t>(m_textures_descriptor_bindings.size()),
                .pBindings = m_textures_descriptor_bindings.data()})));
    }

    if (!m_buffers_descriptor_bindings.empty()) {
        layouts_list.emplace_back(avk::create_descriptor_set_layout(
            avk::context::device()->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<uint32_t>(m_buffers_descriptor_bindings.size()),
                .pBindings = m_buffers_descriptor_bindings.data()})));
    }

    return layouts_list;
}


avk::descriptor_set_list avk::graphics_pipeline_builder::create_descriptor_sets(
    const avk::descriptor_pool& pool,
    const std::vector<vk::DescriptorSetLayout>& layouts)
{
    if (layouts.empty()) {
        return {};
    }

    auto new_sets = avk::allocate_descriptor_sets(vk::DescriptorSetAllocateInfo{
        .descriptorPool = pool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()});

    auto curr_set = new_sets->begin();

    if (!m_descriptor_images_writes.empty()) {
        avk::context::device()->updateDescriptorSets({
            vk::WriteDescriptorSet{
                .dstSet = *curr_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(m_descriptor_images_writes.size()),
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = m_descriptor_images_writes.data(),
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            }},
        {});
        ++curr_set;
    }

    if (!m_descriptor_buffers_writes.empty()) {
        auto curr_buffer_binding = m_buffers_descriptor_bindings.begin();

        for (const auto& buffer_write : m_descriptor_buffers_writes) {
            avk::context::device()->updateDescriptorSets({
                vk::WriteDescriptorSet{
                    .dstSet = *curr_set,
                    .dstBinding = curr_buffer_binding->binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = curr_buffer_binding->descriptorType,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &buffer_write,
                    .pTexelBufferView = nullptr
                }},
                {});
            ++curr_buffer_binding;
        }
        ++curr_set;
    }

    return new_sets;
}


void avk::_graphics_pipeline::activate(vk::CommandBuffer& cmd_buffer, const std::vector<uint32_t>& dyn_offsets)
{
    for (const auto& range : m_push_constant_ranges) {
        cmd_buffer.pushConstants(m_pipeline_layout, range.stageFlags, 0, range.size, m_push_constant_buffer.data());
    }

    if (!m_descriptor_sets->empty()) {
        cmd_buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            m_pipeline_layout,
            0, m_descriptor_sets->size(),
            m_descriptor_sets->data(),
            dyn_offsets.size(),
            dyn_offsets.data());
    }

    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
}
