#pragma once

#include <render/vk/raii.hpp>

#include <functional>
#include <vector>

namespace sandbox::hal::render::avk
{
    namespace detail
    {
        template<typename T, typename = void>
        struct is_reservable
        {
            constexpr static bool value = false;
        };

        template<typename T>
        struct is_reservable<T, std::void_t<decltype(std::declval<T>().reserve(1))>>
        {
            constexpr static bool value = true;
        };

        template<typename T>
        constexpr bool is_reservable_v = is_reservable<T>::value;
    } // namespace detail

    template<
        typename CastType,
        typename IterType,
        template<typename T> typename AllocatorType = std::allocator,
        template<typename T, typename Allocator = AllocatorType<CastType>> typename Container = std::vector>
    Container<CastType, AllocatorType<CastType>> to_elements_list(IterType begin, IterType end)
    {
        using ConstainerT = Container<CastType, AllocatorType<CastType>>;
        ConstainerT res;

        if constexpr (detail::is_reservable_v<ConstainerT>) {
            res.reserve(std::distance(begin, end));
        }

        for (; begin != end; ++begin) {
            res.emplace_back(begin->template as<CastType>());
        }

        return res;
    }


    std::pair<avk::vma_image, avk::image_view> gen_attachment(
        uint32_t width,
        uint32_t height,
        vk::Format format,
        uint32_t queue_family,
        vk::ImageUsageFlags usage = {},
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);


    avk::vma_buffer gen_staging_buffer(
        uint32_t queue_family, size_t data_size, const std::function<void(uint8_t*)>& on_buffer_mapped = {});


    avk::framebuffer gen_framebuffer(
        uint32_t width, uint32_t height, const vk::RenderPass& pass, const vk::ImageView* attachments, uint32_t attachments_count);


    std::tuple<avk::framebuffer, std::vector<avk::vma_image>, std::vector<avk::image_view>>
    gen_framebuffer(
        uint32_t width,
        uint32_t height,
        uint32_t queue_family,
        const vk::RenderPassCreateInfo& pass_info,
        const vk::RenderPass& pass,
        const std::vector<float>& attachments_scales = {},
        const std::vector<vk::ImageUsageFlagBits>& attachments_usages = {});


    avk::descriptor_set_layout gen_descriptor_set_layout(
        uint32_t count, vk::DescriptorType type);

    std::pair<avk::descriptor_pool, avk::descriptor_set_list> gen_descriptor_sets(
        const std::vector<vk::DescriptorSetLayout>& layouts,
        const std::vector<std::pair<uint32_t, vk::DescriptorType>>& layouts_data);

    void write_texture_descriptors(
        vk::DescriptorSet dst_set,
        const std::vector<vk::ImageView>&,
        const std::vector<vk::Sampler>&);

    void write_buffer_descriptors(
        vk::DescriptorSet dst_set,
        const std::vector<vk::Buffer>&,
        const std::vector<std::pair<VkDeviceSize, VkDeviceSize>>& offsets_ranges);

    avk::pipeline_layout gen_pipeline_layout(
        const std::vector<vk::PushConstantRange>&,
        const std::vector<vk::DescriptorSetLayout>&);

    std::pair<avk::vma_image, avk::image_view> gen_texture(
        uint32_t queue_family,
        vk::ImageViewType type,
        vk::Format format,
        uint32_t width,
        uint32_t height,
        uint32_t depth = 1,
        uint32_t levels = 1,
        uint32_t layers = 1);

    avk::sampler gen_sampler(
        vk::Filter min_filter,
        vk::Filter mag_filter,
        vk::SamplerMipmapMode mip_filter,
        vk::SamplerAddressMode addr_mode_u,
        vk::SamplerAddressMode addr_mode_v,
        vk::SamplerAddressMode addr_mode_w,
        float max_lod,
        bool anisotropy = false);

    void copy_buffer_to_image(
        vk::CommandBuffer command_buffer,
        const vk::Buffer& staging_buffer,
        const vk::Image& dst_image,
        uint32_t image_width,
        uint32_t image_height,
        uint32_t image_level_count,
        uint32_t image_layers_count,
        size_t buffer_offset = 0);


    struct pass_create_info
    {
        std::vector<vk::AttachmentDescription> attachments{};
        std::vector<vk::AttachmentReference> attachments_refs{};
        std::vector<vk::SubpassDescription> subpasses{};
        std::vector<vk::SubpassDependency> subpass_dependencies{};
    };

    class render_pass_wrapper
    {
    public:
        render_pass_wrapper() = default;

        render_pass_wrapper(pass_create_info info);
        const vk::RenderPassCreateInfo& get_info() const;
        vk::RenderPass get_native_pass() const;

    private:
        pass_create_info m_info{};
        std::vector<vk::AttachmentReference> m_attachments_refs{};
        vk::RenderPassCreateInfo m_pass_info{};
        avk::render_pass m_pass_handler{};
    };
} // namespace sandbox::hal::render::avk
