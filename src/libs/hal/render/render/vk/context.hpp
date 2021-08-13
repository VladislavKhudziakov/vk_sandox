#pragma once

#include <render/vk/vulkan_dependencies.hpp>

#include <memory>


namespace sandbox::hal::render::avk
{
    class context
    {
    public:
        struct extensions_info
        {
            const char** names;
            size_t count;
        };

        struct layers_info
        {
            const char** names;
            size_t count;
        };

        struct instance_data
        {
            extensions_info extensions;
            layers_info layers;
        };

        struct device_data
        {
            extensions_info extensions{};
            layers_info layers{};
            vk::SurfaceKHR* required_supported_surfaces = nullptr;
            size_t required_supported_surfaces_count{0};
        };

        static void init_instance(instance_data);
        static void init_device(device_data);

        static void deinit();

        static vk::Instance* instance();
        static vk::PhysicalDevice* gpu();
        static vk::Device* device();
        static VmaAllocator allocator();
        static uint32_t queue_family(vk::QueueFlagBits);
        static uint32_t queues_count(vk::QueueFlagBits);
        static vk::Queue queue(vk::QueueFlagBits, uint32_t index);

    private:
        enum class init_status
        {
            not_initialized,
            instance_initialized_successfully,
            device_initialized_successfully
        };

        class impl;

        static std::unique_ptr<impl> m_impl;
        static init_status m_init_status;
    };
} // namespace sandbox::hal::render::avk
