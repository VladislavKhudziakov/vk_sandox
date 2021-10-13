#pragma once

#include <algorithm>
#include <functional>
#include <type_traits>

#include <render/vk/context.hpp>
#include <spdlog/spdlog.h>

namespace sandbox::hal::render::avk
{
    namespace detail
    {
        template<typename T1, typename T2, typename = void>
        struct is_explicitly_castable
        {
            constexpr static bool value = false;
        };


        template<typename T1, typename T2>
        struct is_explicitly_castable<T1, T2, std::void_t<decltype(static_cast<T2>(std::declval<T1>()))>>
        {
            constexpr static bool value = true;
        };


        template<typename T1, typename T2>
        constexpr bool is_explicitly_castable_v = is_explicitly_castable<T1, T2>::value;


        template<typename T1, typename T2, typename = void>
        struct is_as_castable
        {
            constexpr static bool value = false;
        };


        template<typename T1, typename T2>
        struct is_as_castable<T1, T2, std::void_t<decltype((std::declval<T1>().template as<T2>) )>>
        {
            constexpr static bool value = false;
        };


        template<typename T1, typename T2>
        constexpr bool is_as_castable_v = is_as_castable<T1, T2>::value;


        template<typename T1, typename T2>
        auto try_cast(const T1& value)
        {
            static_assert(std::is_same_v<T2, T1> || detail::is_explicitly_castable_v<T1, T2> || detail::is_as_castable_v<T1, T2>);

            if constexpr (std::is_same_v<T2, T1>) {
                return value;
            }

            if constexpr (detail::is_explicitly_castable_v<T1, T2>) {
                return static_cast<T2>(value);
            }

            if constexpr (detail::is_as_castable_v<T1, T2>) {
                return value.template as<T2>();
            }
        }


        template<typename T>
        class vma_resource
        {
        public:
            vma_resource() = default;

            vma_resource(const T& handler, VmaAllocation allocation, const VmaAllocationInfo& alloc_info)
                : m_handler(handler)
                , m_allocation(allocation)
                , m_allocation_info(alloc_info)
            {
            }

            operator T() const
            {
                return m_handler;
            }

            T* operator->()
            {
                return &m_handler;
            }

            const T* operator->() const
            {
                return &m_handler;
            }

            operator VmaAllocation() const
            {
                return m_allocation;
            }

            const VmaAllocationInfo& get_alloc_info() const
            {
                return m_allocation_info;
            }

            template<typename AsType>
            AsType as() const
            {
                return detail::try_cast<decltype(m_handler), AsType>(m_handler);
            }

        private:
            T m_handler{};
            VmaAllocationInfo m_allocation_info{};
            VmaAllocation m_allocation{};
        };

        using vma_image = vma_resource<vk::Image>;
        using vma_buffer = vma_resource<vk::Buffer>;
    } // namespace detail

    template<typename T>
    class raii_handler
    {
    public:
        using native_type = T;

        raii_handler()
        {
        }

        raii_handler(const std::function<T()>& create, std::function<void(const T&)> destroy)
            : m_handler{create()}
            , m_destroy{std::move(destroy)}
        {
        }

        raii_handler(T handler, std::function<void(const T&)> destroy)
            : m_handler{std::move(handler)}
            , m_destroy{std::move(destroy)}
        {
        }

        raii_handler(const raii_handler&) = delete;
        raii_handler& operator=(const raii_handler&) = delete;

        raii_handler(raii_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        raii_handler& operator=(raii_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }

            std::swap(m_handler, src.m_handler);
            std::swap(m_destroy, src.m_destroy);

            return *this;
        }

        ~raii_handler()
        {
            if constexpr (detail::is_explicitly_castable_v<T, bool>) {
                if (static_cast<bool>(m_handler)) {
                    assert(m_destroy);
                    m_destroy(m_handler);
                }
            } else {
                if (m_destroy) {
                    m_destroy(m_handler);
                }
            }
        }

        T* operator->()
        {
            return &m_handler;
        }

        const T* operator->() const
        {
            return &m_handler;
        }

        operator const T&() const
        {
            return m_handler;
        }

        T* handler_ptr()
        {
            return &m_handler;
        }

        const T* handler_ptr() const
        {
            return &m_handler;
        }

        operator bool() const
        {
            if constexpr (detail::is_explicitly_castable<T, bool>::value) {
                return static_cast<bool>(m_handler);
            } else {
                throw std::runtime_error("cannot convert handler to bool");
            }
        }

        template<typename AsType>
        AsType as() const
        {
            return detail::try_cast<decltype(m_handler), AsType>(m_handler);
        }

    private:
        T m_handler{};
        std::function<void(const T&)> m_destroy{};
    };

    using instance = raii_handler<vk::Instance>;
    using device = raii_handler<vk::Device>;
    using surface = raii_handler<vk::SurfaceKHR>;
    using allocator = raii_handler<VmaAllocator>;

    using swapchain = raii_handler<vk::SwapchainKHR>;
    using framebuffer = raii_handler<vk::Framebuffer>;

    using fence = raii_handler<vk::Fence>;
    using semaphore = raii_handler<vk::Semaphore>;

    using command_pool = raii_handler<vk::CommandPool>;
    using command_buffer_list = raii_handler<std::vector<vk::CommandBuffer>>;

    using vma_image = raii_handler<detail::vma_image>;
    using vma_buffer = raii_handler<detail::vma_buffer>;

    using shader_module = raii_handler<vk::ShaderModule>;
    using render_pass = raii_handler<vk::RenderPass>;

    using graphics_pipeline = raii_handler<vk::Pipeline>;
    using compute_pipeline = raii_handler<vk::Pipeline>;
    using pipeline = raii_handler<vk::Pipeline>;
    using pipeline_cache = raii_handler<vk::PipelineCache>;
    using pipeline_layout = raii_handler<vk::PipelineLayout>;

    using descriptor_pool = raii_handler<vk::DescriptorPool>;
    using descriptor_set_list = raii_handler<std::vector<vk::DescriptorSet>>;
    using descriptor_set_layout = raii_handler<vk::DescriptorSetLayout>;

    using image_view = raii_handler<vk::ImageView>;
    using sampler = raii_handler<vk::Sampler>;

    template<typename... Args>
    avk::instance create_instance(Args&&... args)
    {
        auto _create_instance = [&args...]() {
            vk::Instance instance{};
            vk::Result result = vk::createInstance(std::forward<Args>(args)..., &instance);
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Cannot init instance.");
            }
            return instance;
        };

        return avk::instance{_create_instance, [](const vk::Instance& instance) { instance.destroy(); }};
    }


    template<typename... Args>
    avk::device create_device(vk::PhysicalDevice gpu, Args&&... args)
    {
        auto _create_device = [&gpu, &args...]() {
            vk::Device device{};
            vk::Result result = gpu.createDevice(std::forward<Args>(args)..., &device);
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Cannot init device.");
            }
            return device;
        };

        return avk::device{_create_device, [](const vk::Device& device) { device.destroy(); }};
    }


    inline avk::surface create_surface(vk::Instance instance, const std::function<vk::SurfaceKHR()>& _create_surface = {})
    {
        return avk::surface{_create_surface, [instance](const vk::SurfaceKHR& surface) { instance.destroySurfaceKHR(surface); }};
    }


    inline avk::allocator create_allocator(const VmaAllocatorCreateInfo& alloc_info)
    {
        VmaAllocator allocator;
        vmaCreateAllocator(&alloc_info, &allocator);

        return avk::allocator{allocator, vmaDestroyAllocator};
    }


    template<typename T>
    raii_handler<T> create_resource(const T& value, const std::function<void(const T&)>& dtor)
    {
        return raii_handler<T>{value, dtor};
    }


#define VK_RESOURCE_INITIALIZER(Type, Dtor) \
    [](const Type& value) {                 \
        return create_resource<Type>(       \
            value,                          \
            [](const Type& resource) {      \
                Dtor(resource);             \
            });                             \
    }


    [[maybe_unused]] inline auto create_swapchain = VK_RESOURCE_INITIALIZER(vk::SwapchainKHR, avk::context::device()->destroySwapchainKHR);
    [[maybe_unused]] inline auto create_framebuffer = VK_RESOURCE_INITIALIZER(vk::Framebuffer, avk::context::device()->destroyFramebuffer);
    [[maybe_unused]] inline auto create_fence = VK_RESOURCE_INITIALIZER(vk::Fence, avk::context::device()->destroyFence);
    [[maybe_unused]] inline auto create_semaphore = VK_RESOURCE_INITIALIZER(vk::Semaphore, avk::context::device()->destroySemaphore);
    [[maybe_unused]] inline auto create_command_pool = VK_RESOURCE_INITIALIZER(vk::CommandPool, avk::context::device()->destroyCommandPool);
    [[maybe_unused]] inline auto create_shader_module = VK_RESOURCE_INITIALIZER(vk::ShaderModule, avk::context::device()->destroyShaderModule);
    [[maybe_unused]] inline auto create_render_pass = VK_RESOURCE_INITIALIZER(vk::RenderPass, avk::context::device()->destroyRenderPass);
    [[maybe_unused]] inline auto create_graphics_pipeline = VK_RESOURCE_INITIALIZER(vk::Pipeline, avk::context::device()->destroyPipeline);
    [[maybe_unused]] inline auto create_compute_pipeline = VK_RESOURCE_INITIALIZER(vk::Pipeline, avk::context::device()->destroyPipeline);
    [[maybe_unused]] inline auto create_pipeline_cache = VK_RESOURCE_INITIALIZER(vk::PipelineCache, avk::context::device()->destroyPipelineCache);
    [[maybe_unused]] inline auto create_pipeline_layout = VK_RESOURCE_INITIALIZER(vk::PipelineLayout, avk::context::device()->destroyPipelineLayout);
    [[maybe_unused]] inline auto create_descriptor_pool = VK_RESOURCE_INITIALIZER(vk::DescriptorPool, avk::context::device()->destroyDescriptorPool);
    [[maybe_unused]] inline auto create_descriptor_set_layout = VK_RESOURCE_INITIALIZER(vk::DescriptorSetLayout, avk::context::device()->destroyDescriptorSetLayout);
    [[maybe_unused]] inline auto create_image_view = VK_RESOURCE_INITIALIZER(vk::ImageView, avk::context::device()->destroyImageView);
    [[maybe_unused]] inline auto create_sampler = VK_RESOURCE_INITIALIZER(vk::Sampler, avk::context::device()->destroySampler);

    inline command_buffer_list allocate_command_buffers(const vk::CommandBufferAllocateInfo& allocate_info, bool free = true)
    {
        return {
            avk::context::device()->allocateCommandBuffers(allocate_info),
            [cmd_pool = allocate_info.commandPool, free](const std::vector<vk::CommandBuffer>& command_buffers) {
                if (free) {
                    avk::context::device()->freeCommandBuffers(cmd_pool, command_buffers);
                }
            }};
    }

    inline descriptor_set_list allocate_descriptor_sets(const vk::DescriptorSetAllocateInfo& allocate_info, bool free = true)
    {
        auto d_sets = avk::context::device()->allocateDescriptorSets(allocate_info);
        return {
            std::move(d_sets),
            [desc_pool = allocate_info.descriptorPool, free](const std::vector<vk::DescriptorSet>& desc_sets) {
                if (free) {
                    avk::context::device()->freeDescriptorSets(desc_pool, desc_sets);
                }
            }};
    }


    inline avk::vma_image create_vma_image(const vk::ImageCreateInfo& image_info, const VmaAllocationCreateInfo& alloc_info)
    {
        VkImageCreateInfo c_image_info = image_info;
        VmaAllocationInfo alloc_data{};
        VkImage image{};
        VmaAllocation allocation{};

        auto res = vmaCreateImage(avk::context::allocator(), &c_image_info, &alloc_info, &image, &allocation, &alloc_data);
        assert(res == VK_SUCCESS);
        return avk::vma_image{
            detail::vma_image{vk::Image{image}, allocation, alloc_data},
            [](const detail::vma_image& image) {
                vmaDestroyImage(
                    avk::context::allocator(),
                    static_cast<VkImage>(static_cast<vk::Image>(image)),
                    static_cast<VmaAllocation>(image));
            }};
    }


    inline vma_buffer create_vma_buffer(const vk::BufferCreateInfo& buffer_info, const VmaAllocationCreateInfo& alloc_info)
    {
        VkBufferCreateInfo c_buffer_info = buffer_info;
        VmaAllocationInfo alloc_data{};
        VkBuffer buffer{};
        VmaAllocation allocation{};

        vmaCreateBuffer(avk::context::allocator(), &c_buffer_info, &alloc_info, &buffer, &allocation, &alloc_data);

        return {
            detail::vma_buffer{vk::Buffer{buffer}, allocation, alloc_data},
            [](const detail::vma_buffer& buffer) {
                vmaDestroyBuffer(
                    avk::context::allocator(),
                    static_cast<VkBuffer>(static_cast<vk::Buffer>(buffer)),
                    static_cast<VmaAllocation>(buffer));
            }};
    }

#undef VK_RESOURCE_INITIALIZER
} // namespace sandbox::hal::render::avk
