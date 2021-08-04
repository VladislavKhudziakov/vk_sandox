#pragma once

#include <algorithm>
#include <functional>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.h>

namespace sandbox::hal::render::avk
{
    template<typename T>
    class raii_handler
    {
    public:
        raii_handler()
        {
        }

        explicit raii_handler(std::function<T()> create, std::function<void(const T&)> destroy)
            : m_handler{create()}
            , m_destroy{destroy}
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
            if (m_handler) {
                assert(m_destroy);
                m_destroy(m_handler);
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

        operator T*()
        {
            return &m_handler;
        }

        operator const T*() const
        {
            return &m_handler;
        }

        operator T() const
        {
            return m_handler;
        }

    private:
        T m_handler{};
        std::function<void(const T&)> m_destroy{};
    };

    using instance = raii_handler<vk::Instance>;
    using device = raii_handler<vk::Device>;
    using surface = raii_handler<vk::SurfaceKHR>;
    using allocator = raii_handler<VmaAllocator>;

    template <typename... Args>
    avk::instance create_instance(Args&& ...args)
    {
        auto _create_instance = [&args...]() {
            vk::Instance instance{};
            vk::Result result = vk::createInstance(std::forward<Args>(args)..., &instance);
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Cannot init instance.");
            }
            return instance;
        };

        return avk::instance{_create_instance, [](const vk::Instance& instance) {instance.destroy();}};
    }


    template <typename... Args>
    avk::device create_device(vk::PhysicalDevice gpu, Args&& ...args)
    {
        auto _create_device = [&gpu, &args...]() {
            vk::Device device{};
            vk::Result result = gpu.createDevice(std::forward<Args>(args)..., &device);
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Cannot init device.");
            }
            return device;
        };

        return avk::device{_create_device, [](const vk::Device& device) {device.destroy();}};
    }


    inline avk::surface create_surface(vk::Instance instance, const std::function<vk::SurfaceKHR()>& _create_surface = {})
    {
        return avk::surface{_create_surface, [instance](const vk::SurfaceKHR& surface) {instance.destroySurfaceKHR(surface);}};
    }


    inline avk::allocator create_allocator(const VmaAllocatorCreateInfo& alloc_info)
    {
        auto create_allocator = [&alloc_info]() {
            VmaAllocator allocator;
            vmaCreateAllocator(&alloc_info, &allocator);
            return allocator;
        };

        return avk::allocator{create_allocator, vmaDestroyAllocator};
    }
}

