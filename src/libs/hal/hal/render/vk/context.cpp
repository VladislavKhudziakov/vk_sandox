

#include "context.hpp"

#include <hal/render/vk/raii.hpp>

#include <spdlog/spdlog.h>

#include <functional>
#include <vector>
#include <cstring>
#include <iostream>
#include <unordered_map>

using namespace sandbox;
using namespace sandbox::hal;
using namespace sandbox::hal::render;
using namespace sandbox::hal::render::avk;

namespace
{
#ifndef NDEBUG
    std::vector<const char*> implicit_required_instance_layers {
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> implicit_required_instance_extensions {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
#else
    std::vector<const char*> implicit_required_instance_layers {};

    std::vector<const char*> implicit_required_instance_extensions {};
#endif

    std::vector<const char*> implicit_required_device_extensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifndef NDEBUG
    std::vector<const char*> implicit_required_device_layers {
        "VK_LAYER_KHRONOS_validation"
    };
#else
    std::vector<const char*> implicit_required_device_layers {};
#endif


    std::vector<const char*> gather_extensions_names(
        const std::function<std::vector<vk::ExtensionProperties>()>& get_extensions_props,
        avk::context::extensions_info extensions_info,
        std::vector<const char*> default_names)
    {
        auto extensions_props = get_extensions_props();

        for (int i = 0; i < extensions_info.count; ++i) {
            auto find_default_name = [search_name = extensions_info.names[i]](const char* curr_name) {
                return strcmp(search_name, curr_name) == 0;
            };

            if (std::find_if(default_names.begin(), default_names.end(), find_default_name) == default_names.end()) {
                auto find_property = [search_name = extensions_info.names[i]](const vk::ExtensionProperties& props) {
                    return strcmp(search_name, props.extensionName) == 0;
                };

                if(std::find_if(extensions_props.begin(), extensions_props.end(), find_property) != extensions_props.end()) {
                    default_names.push_back(extensions_info.names[i]);
                } else {
                    throw std::runtime_error(std::string("Bad extension requested ") + extensions_info.names[i]);
                }
            }
        }

        return default_names;
    }


    std::vector<const char*> gather_layers_names(
        const std::function<std::vector<vk::LayerProperties>()>& get_layers_props,
        avk::context::layers_info layers_info,
        std::vector<const char*> default_names)
    {
        auto layers_props = get_layers_props();

        for (int i = 0; i < layers_info.count; ++i) {
            auto find_default_name = [search_name = layers_info.names[i]](const char* curr_name) {
                return strcmp(search_name, curr_name) == 0;
            };

            if (std::find_if(default_names.begin(), default_names.end(), find_default_name) == default_names.end()) {
                auto find_property = [search_name = layers_info.names[i]](const vk::LayerProperties& props) {
                    return strcmp(search_name, props.layerName) == 0;
                };

                if(std::find_if(layers_props.begin(), layers_props.end(), find_property) != layers_props.end()) {
                    default_names.push_back(layers_info.names[i]);
                } else {
                    throw std::runtime_error(std::string("Bad layer requested ") + layers_info.names[i]);
                }
            }
        }

        return default_names;
    }

    std::pair<int32_t, int32_t> find_best_queue_family(vk::PhysicalDevice gpu, vk::QueueFlagBits supported_operations)
    {
        int32_t queue_family_index = -1;
        int32_t max_queue_count = -1;

        uint32_t queue_families_props_size;
        gpu.getQueueFamilyProperties(&queue_families_props_size, nullptr);
        std::vector<vk::QueueFamilyProperties> props{queue_families_props_size};
        gpu.getQueueFamilyProperties(&queue_families_props_size, props.data());

        for (int i = 0; i < props.size(); ++i) {
            const auto& curr_props = props[i];
            if (curr_props.queueFlags & supported_operations) {
                if (max_queue_count < int32_t(curr_props.queueCount)) {
                    queue_family_index = i;
                    max_queue_count = int32_t(curr_props.queueCount);
                }
            }
        }

        return {queue_family_index, max_queue_count};
    }
}


std::unique_ptr<avk::context::impl> avk::context::m_impl{};
avk::context::init_status avk::context::m_init_status{avk::context::init_status::not_initialized};

class avk::context::impl
{
    friend hal::render::avk::context;
public:

    void init_instance(instance_data data)
    {
        #if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
            static vk::DynamicLoader  dl;
            PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
                    dl.getProcAddress<PFN_vkGetInstanceProcAddr>( "vkGetInstanceProcAddr" );
            VULKAN_HPP_DEFAULT_DISPATCHER.init( vkGetInstanceProcAddr );
        #endif

        auto layers = gather_layers_names([](){
                uint32_t layers_count{};
                vk::Result res = vk::enumerateInstanceLayerProperties(&layers_count, nullptr);
                std::vector<vk::LayerProperties> props{layers_count};
                res = vk::enumerateInstanceLayerProperties(&layers_count, props.data());
                return props;
            },
            data.layers,
            implicit_required_instance_layers);

        auto extensions = gather_extensions_names([](){
                uint32_t ext_count{};
                vk::Result res = vk::enumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
                std::vector<vk::ExtensionProperties> props{ext_count};
                res = vk::enumerateInstanceExtensionProperties(nullptr, &ext_count, props.data());
                return props;
            },
            data.extensions,
            implicit_required_instance_extensions);

        vk::ApplicationInfo app_info{
            "vk_sandbox",
            VK_MAKE_VERSION(0, 1, 0),
            "vk_sandbox",
            VK_MAKE_VERSION(0, 1, 0),
            VK_API_VERSION_1_1
        };

        vk::InstanceCreateInfo instance_info {
            {},
            &app_info,
            static_cast<uint32_t>(layers.size()),
            layers.data(),
            static_cast<uint32_t>(extensions.size()),
            extensions.data()
        };

        static auto debug_callback = [](
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData) -> VkBool32
        {
            switch (messageSeverity) {
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                    [[fallthrough]];
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                    spdlog::info(pCallbackData->pMessage);
                    break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                    spdlog::warn(pCallbackData->pMessage);
                    break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                    spdlog::error(pCallbackData->pMessage);
                    break;
                default:
                    throw std::runtime_error("Bad message severity.");
            }
            return VK_TRUE;
        };

        vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info(
                {},
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                debug_callback
            );

        instance_info.setPNext(&debug_messenger_info);

        m_instance = avk::create_instance(&instance_info, nullptr);
    }


    void init_device(device_data data)
    {
        select_gpu(data);
        create_device(data);
    }

    uint32_t queue_family(vk::QueueFlagBits supported_ops)
    {
        if (supported_ops & vk::QueueFlagBits::eGraphics) {
            return m_queue_families_data[vk::QueueFlagBits::eGraphics].first;
        } else if (supported_ops & vk::QueueFlagBits::eTransfer) {
            return m_queue_families_data[vk::QueueFlagBits::eTransfer].first;
        } else if (supported_ops & vk::QueueFlagBits::eCompute) {
            return m_queue_families_data[vk::QueueFlagBits::eCompute].first;
        } else {
            throw std::runtime_error("Unsupported queue family type requested.");
        }
    }

    uint32_t queues_count(vk::QueueFlagBits supported_ops)
    {
        if (supported_ops & vk::QueueFlagBits::eGraphics) {
            return m_queue_families_data[vk::QueueFlagBits::eGraphics].second;
        } else if (supported_ops & vk::QueueFlagBits::eTransfer) {
            return m_queue_families_data[vk::QueueFlagBits::eTransfer].second;
        } else if (supported_ops & vk::QueueFlagBits::eCompute) {
            return m_queue_families_data[vk::QueueFlagBits::eCompute].second;
        } else {
            throw std::runtime_error("Unsupported queue family type requested.");
        }
    }


    vk::Queue queue(vk::QueueFlagBits supported_ops, uint32_t index)
    {
        if (index >= queues_count(supported_ops)) {
            throw std::runtime_error("Queue index exceed queues count for current queue family.");
        }

        return m_device->getQueue(queue_family(supported_ops), index);
    }

private:
    void select_gpu(device_data data)
    {
        uint32_t gpus_count{0};
        vk::Result res = m_instance->enumeratePhysicalDeviceGroups(&gpus_count, nullptr);
        std::vector<vk::PhysicalDeviceGroupProperties> gpus_props{gpus_count};
        res = m_instance->enumeratePhysicalDeviceGroups(&gpus_count, gpus_props.data());

        auto check_queue_families_support = [](vk::PhysicalDevice gpu)
        {
            return find_best_queue_family(gpu, vk::QueueFlagBits::eGraphics).first >= 0;
        };

        auto check_surface_support = [&data](vk::PhysicalDevice gpu)
        {
            auto graphics_queue_family = find_best_queue_family(gpu, vk::QueueFlagBits::eGraphics).first;
            assert(graphics_queue_family >= 0);

            for (int i = 0; i < data.required_supported_surfaces_count; ++i) {
                VkBool32 surface_supported{VK_FALSE};
                vk::Result res = gpu.getSurfaceSupportKHR(graphics_queue_family, data.required_supported_surfaces[i], &surface_supported);
                if (res != vk::Result::eSuccess) {
                    throw std::runtime_error("Cannot detect surface support.");
                }
                if (!surface_supported) {
                    return false;
                }
            }
            return true;
        };

        auto check_extensions_support = [&data](vk::PhysicalDevice gpu)
        {
            try {
                gather_layers_names([&gpu]() {
                        uint32_t layers_count{0};
                        vk::Result res = gpu.enumerateDeviceLayerProperties(&layers_count, nullptr);
                        std::vector<vk::LayerProperties> props{layers_count};
                        res = gpu.enumerateDeviceLayerProperties(&layers_count, props.data());
                        return props;
                    },
                    data.layers,
        implicit_required_device_layers);

                gather_extensions_names([&gpu]() {
                        uint32_t ext_count{0};
                        vk::Result res = gpu.enumerateDeviceExtensionProperties(nullptr, &ext_count, nullptr);
                        std::vector<vk::ExtensionProperties> extensions{ext_count};
                        res = gpu.enumerateDeviceExtensionProperties(nullptr, &ext_count, extensions.data());
                        return extensions;
                    },
                    data.extensions,
                implicit_required_device_extensions);
            } catch (...) {
                return false;
            }
            return true;
        };

        auto try_find_gpu =
                [&gpus_props, &check_surface_support, &check_extensions_support, &check_queue_families_support]
                (vk::PhysicalDeviceType device_type) -> vk::PhysicalDevice
        {
            for(const auto& gpu : gpus_props) {
                for (const auto device : gpu.physicalDevices) {
                    if (!device) {
                        continue;
                    }

                    auto props = device.getProperties();

                    if (props.deviceType != device_type) {
                        continue;
                    }

                    if (!check_queue_families_support(device)) {
                        continue;
                    }

                    if (!check_surface_support(device)) {
                        continue;
                    }

                    if (!check_extensions_support(device)) {
                        continue;
                    }

                    return device;
                }
            }

            return {};
        };

        m_gpu = try_find_gpu(vk::PhysicalDeviceType::eDiscreteGpu);
        if (!m_gpu) {
            m_gpu = try_find_gpu(vk::PhysicalDeviceType::eIntegratedGpu);
            if (!m_gpu) {
                m_gpu = try_find_gpu(vk::PhysicalDeviceType::eCpu);
                if (!m_gpu) {
                    throw std::runtime_error("Cannot find gpu.");
                }
            }
        }
    }


    void create_device(device_data data)
    {
        auto device_layers = gather_layers_names([this]() {
                uint32_t layers_count{0};
                vk::Result res = m_gpu.enumerateDeviceLayerProperties(&layers_count, nullptr);
                std::vector<vk::LayerProperties> props{layers_count};
                res = m_gpu.enumerateDeviceLayerProperties(&layers_count, props.data());
                return props;
            },
            data.layers,
                implicit_required_device_layers);

        auto device_extensions = gather_extensions_names([this]() {
                uint32_t ext_count{0};
                vk::Result res = m_gpu.enumerateDeviceExtensionProperties(nullptr, &ext_count, nullptr);
                std::vector<vk::ExtensionProperties> extensions{ext_count};
                res = m_gpu.enumerateDeviceExtensionProperties(nullptr, &ext_count, extensions.data());
                return extensions;
            },
            data.extensions,
            implicit_required_device_extensions);

        vk::QueueFamilyProperties props {};

        m_queue_families_data[vk::QueueFlagBits::eGraphics] = find_best_queue_family(m_gpu, vk::QueueFlagBits::eGraphics);
        m_queue_families_data[vk::QueueFlagBits::eCompute] = find_best_queue_family(m_gpu, vk::QueueFlagBits::eCompute);
        m_queue_families_data[vk::QueueFlagBits::eTransfer] = find_best_queue_family(m_gpu, vk::QueueFlagBits::eTransfer);

        std::vector<std::vector<float>> queues_priorities{};
        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos{};

        for (auto next = ++m_queue_families_data.begin(), curr = m_queue_families_data.begin(); next != m_queue_families_data.end(); ++next, ++curr) {
            if (curr->second.first == next->second.first && !queue_create_infos.empty()) {
                continue;
            }
            queues_priorities.emplace_back() = std::vector<float>(static_cast<size_t>(curr->second.second), 1.0f);
            queue_create_infos.emplace_back() = vk::DeviceQueueCreateInfo({}, curr->second.first, curr->second.second, queues_priorities.back().data());
        }

        vk::PhysicalDeviceFeatures features = m_gpu.getFeatures();

        vk::DeviceCreateInfo device_info(
            {},
            queue_create_infos.size(),
            queue_create_infos.data(),
            device_layers.size(),
            device_layers.data(),
            device_extensions.size(),
            device_extensions.data(),
            &features);

        m_device = avk::create_device(m_gpu, &device_info, nullptr);
    }

    avk::instance m_instance{};
    vk::PhysicalDevice m_gpu{};
    avk::device m_device{};
    std::unordered_map<vk::QueueFlagBits, std::pair<int32_t, int32_t>> m_queue_families_data{};
};


vk::PhysicalDevice *hal::render::avk::context::gpu()
{
    return &m_impl->m_gpu;
}


vk::Device *hal::render::avk::context::device()
{
    return m_impl->m_device;
}


vk::Instance* hal::render::avk::context::instance()
{
    return m_impl->m_instance;
}


void hal::render::avk::context::deinit()
{
    m_impl.reset();
}


void context::init_instance(context::instance_data instance_data)
{
    if (m_init_status >= init_status::instance_initialized_successfully) {
        return;
    }

    m_impl = std::make_unique<context::impl>();
    m_impl->init_instance(instance_data);
}


void context::init_device(context::device_data device_data)
{
    if (m_init_status >= init_status::device_initialized_successfully) {
        return;
    }

    m_impl->init_device(device_data);
}


uint32_t context::queue_family(vk::QueueFlagBits supported_ops)
{
    return m_impl->queue_family(supported_ops);
}


uint32_t context::queues_count(vk::QueueFlagBits supported_ops)
{
    return m_impl->queues_count(supported_ops);
}


vk::Queue context::queue(vk::QueueFlagBits supported_ops, uint32_t index)
{
    return m_impl->queue(supported_ops, index);
}
