
#include <render/vk/raii.hpp>
#include <render/vk/utils.hpp>
#include <window/vk_main_loop_update_listener.hpp>

#include <stb/stb_image.h>
using namespace sandbox::hal::render;


class dummy_update_listener : public sandbox::hal::vk_main_loop_update_listener
{
public:
    explicit dummy_update_listener()
    {
    }

    void load_texture(const std::string& file_name)
    {
        std::unique_ptr<stbi_uc, void(*)(void*)> image{nullptr, [](void* ptr){if (ptr) stbi_image_free(ptr);}};
        image.reset(stbi_load(file_name.c_str(), &m_width, &m_height, &m_channels, 0));

        std::vector<uint8_t> image_pixels;

        uint8_t* curr_image_pixel_data = image.get();

        if (m_channels == STBI_rgb) {
            image_pixels.reserve(m_width * m_height * 4);

            for (int y = 0; y < m_height; ++y) {
                for (int x = 0; x < m_width; ++x) {
                    image_pixels.push_back(*(curr_image_pixel_data++));
                    image_pixels.push_back(*(curr_image_pixel_data++));
                    image_pixels.push_back(*(curr_image_pixel_data++));
                    image_pixels.push_back(255);
                }
            }

            curr_image_pixel_data = image_pixels.data();
        }

        assert(image);

        uint32_t queue_family = avk::context::queue_family(vk::QueueFlagBits::eGraphics);

        m_image = avk::create_vma_image(
            vk::ImageCreateInfo{
                .flags = {},
                .imageType = vk::ImageType::e2D,
                .format = vk::Format::eR8G8B8A8Srgb,
                .extent = {
                        .width = static_cast<uint32_t>(m_width),
                        .height = static_cast<uint32_t>(m_height),
                        .depth = 1,
                    },
                        .mipLevels = 1,
                        .arrayLayers = 1,
                        .samples = vk::SampleCountFlagBits::e1,
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
                        .sharingMode = vk::SharingMode::eExclusive,
                        .queueFamilyIndexCount = 1,
                        .pQueueFamilyIndices = &queue_family,
                        .initialLayout = vk::ImageLayout::eUndefined,
                    },

                    VmaAllocationCreateInfo{
                        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                });

        m_staging_buffer = avk::create_vma_buffer(
        vk::BufferCreateInfo{
                .flags = {},
                .size = static_cast<VkDeviceSize>(m_width * m_height) * 4,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &queue_family
            },
        VmaAllocationCreateInfo {
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

        void* mapped_data{};
        vmaMapMemory(avk::context::allocator(), static_cast<avk::detail::vma_buffer>(m_staging_buffer), &mapped_data);
        std::memcpy(mapped_data, curr_image_pixel_data, m_width * m_height * 4);

        vmaUnmapMemory(avk::context::allocator(), static_cast<avk::detail::vma_buffer>(m_staging_buffer));
    }

    void update(uint64_t dt) override
    {
        std::call_once(m_need_copy_buffer, [this] {
            auto command_pool = avk::create_command_pool(avk::context::device()->createCommandPool(vk::CommandPoolCreateInfo{
                .flags = {},
                .queueFamilyIndex = avk::context::queue_family(vk::QueueFlagBits::eGraphics),
            }));

            auto command_buffers = avk::allocate_command_buffers(vk::CommandBufferAllocateInfo{
                .commandPool = command_pool,
                .commandBufferCount = 1,
            });

            command_buffers->at(0).begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
            });

            command_buffers->at(0).pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer,
                {},
                {},
                {},
                {
                    vk::ImageMemoryBarrier {
                        .srcAccessMask = {},
                        .dstAccessMask = {},

                        .oldLayout = vk::ImageLayout::eUndefined,
                        .newLayout = vk::ImageLayout::eTransferDstOptimal,

                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                        .image = static_cast<avk::detail::vma_image>(m_image),

                        .subresourceRange = {
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                    }
                });

            command_buffers->at(0).copyBufferToImage(
                static_cast<avk::detail::vma_buffer>(m_staging_buffer),
                static_cast<avk::detail::vma_image>(m_image),
                vk::ImageLayout::eTransferDstOptimal,
                {vk::BufferImageCopy{
                    .bufferOffset = 0,
                    .bufferRowLength = static_cast<uint32_t>(m_width),
                    .bufferImageHeight = static_cast<uint32_t>(m_height),

                    .imageSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },

                    .imageOffset = vk::Offset3D {
                        .x = 0,
                        .y = 0,
                        .z = 0,
                    },
                    .imageExtent = vk::Extent3D {
                        .width = static_cast<uint32_t>(m_width),
                        .height = static_cast<uint32_t>(m_height),
                        .depth = 1,
                    },
                }});

            command_buffers->at(0).end();

            vk::Queue queue = avk::context::queue(vk::QueueFlagBits::eGraphics, 0);

            m_semaphore = avk::create_semaphore(avk::context::device()->createSemaphore({}));
            auto fence = avk::create_fence(avk::context::device()->createFence({}));

            queue.submit(vk::SubmitInfo{
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffers->at(0),
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = m_semaphore.handler_ptr(),
            },
            fence);

            VK_CALL(avk::context::device()->waitForFences({fence}, VK_TRUE, UINT64_MAX));

            m_image_copied = true;
        });
    }

    vk::Image get_present_image() const override
    {
        return static_cast<vk::Image>(static_cast<avk::detail::vma_image>(m_image));
    }

    vk::ImageLayout get_present_image_layout() const override
    {
        return vk::ImageLayout::eTransferDstOptimal;
    }

    vk::Extent2D get_present_image_size() const override
    {
        return vk::Extent2D {
            .width = static_cast<uint32_t>(m_width),
            .height = static_cast<uint32_t>(m_height)
        };
    }

    std::vector<vk::Semaphore> get_wait_semaphores() const override
    {
        if (m_image_copied) {
            m_image_copied = false;
            return {m_semaphore};
        }

        return {};
    }

private:
    int m_width{};
    int m_height{};
    int m_channels{};

    avk::vma_image m_image{};
    avk::vma_buffer m_staging_buffer{};
    std::once_flag m_need_copy_buffer{};
    avk::semaphore m_semaphore{};

    mutable bool m_image_copied{false};
};

int main() {
    auto dummy_listener = std::make_unique<dummy_update_listener>();
    auto* dummy_listener_impl = dummy_listener.get();

    sandbox::hal::window window(800, 600, "sandbox window", std::move(dummy_listener));
    dummy_listener_impl->load_texture(WORK_DIR"/resources/viking_room.png");

    while (!window.closed()) {
        window.main_loop();
    }

    return 0;
}
