

#include "context.hpp"

#include <render/vk/raii.hpp>
#include <render/vk/errors_handling.hpp>

#include <spdlog/spdlog.h>

#include <functional>
#include <vector>
#include <cstring>
#include <unordered_map>

using namespace sandbox;
using namespace sandbox::hal;
using namespace sandbox::hal::render;
using namespace sandbox::hal::render::avk;

#undef NDEBUG
namespace
{
#ifndef NDEBUG
    std::vector<const char*> implicit_required_instance_layers{
        "VK_LAYER_KHRONOS_validation"};

    std::vector<const char*> implicit_required_instance_extensions{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
#else
    std::vector<const char*> implicit_required_instance_layers{};

    std::vector<const char*> implicit_required_instance_extensions{};
#endif

    std::vector<const char*> implicit_required_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifndef NDEBUG
    std::vector<const char*> implicit_required_device_layers{
        "VK_LAYER_KHRONOS_validation"};
#else
    std::vector<const char*> implicit_required_device_layers{};
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

                if (std::find_if(extensions_props.begin(), extensions_props.end(), find_property) != extensions_props.end()) {
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

                if (std::find_if(layers_props.begin(), layers_props.end(), find_property) != layers_props.end()) {
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
} // namespace


std::unique_ptr<avk::context::impl> avk::context::m_impl{};
avk::context::init_status avk::context::m_init_status{avk::context::init_status::not_initialized};

class avk::context::impl
{
    friend hal::render::avk::context;

public:
    void init_instance(instance_data data)
    {
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
        static vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
#endif

        auto layers = gather_layers_names([]() -> std::vector<vk::LayerProperties> {
            return vk::enumerateInstanceLayerProperties();
        },
                                          data.layers,
                                          implicit_required_instance_layers);

        auto extensions = gather_extensions_names([]() -> std::vector<vk::ExtensionProperties> {
            return vk::enumerateInstanceExtensionProperties();
        },
                                                  data.extensions,
                                                  implicit_required_instance_extensions);

        vk::ApplicationInfo app_info{
            .pApplicationName = "vk_sandbox",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName = "vk_sandbox",
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_2};

        vk::InstanceCreateInfo instance_info{
            .flags = {},
            .pApplicationInfo = &app_info,
            .enabledLayerCount = static_cast<uint32_t>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data()};

        static auto debug_callback = [](
                                         VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                         VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                         const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                         void* pUserData) -> VkBool32 {
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

        vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info{
            .flags = {},
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = debug_callback};

        instance_info.setPNext(&debug_messenger_info);

        m_instance = avk::create_instance(&instance_info, nullptr);
    }


    void init_device(device_data data)
    {
        select_gpu(data);
        create_device(data);
        create_allocator();
    }

    uint32_t queue_family(vk::QueueFlagBits supported_ops) const
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

    uint32_t queues_count(vk::QueueFlagBits supported_ops) const
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


    vk::Queue queue(vk::QueueFlagBits supported_ops, uint32_t index) const
    {
        if (index >= queues_count(supported_ops)) {
            throw std::runtime_error("Queue index exceed queues count for current queue family.");
        }

        return m_device->getQueue(queue_family(supported_ops), index);
    }


    VmaAllocator allocator() const
    {
        return m_allocator;
    }

private:
    void select_gpu(device_data data)
    {
        std::vector<vk::PhysicalDeviceGroupProperties> gpus_props = m_instance->enumeratePhysicalDeviceGroups();

        auto check_queue_families_support = [](vk::PhysicalDevice gpu) {
            return find_best_queue_family(gpu, vk::QueueFlagBits::eGraphics).first >= 0;
        };

        auto check_surface_support = [&data](vk::PhysicalDevice gpu) {
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

        auto check_extensions_support = [&data](vk::PhysicalDevice gpu) {
            try {
                gather_layers_names([&gpu]() -> std::vector<vk::LayerProperties> {
                    return gpu.enumerateDeviceLayerProperties();
                },
                                    data.layers,
                                    implicit_required_device_layers);

                gather_extensions_names([&gpu]() -> std::vector<vk::ExtensionProperties> {
                    return gpu.enumerateDeviceExtensionProperties();
                },
                                        data.extensions,
                                        implicit_required_device_extensions);
            } catch (...) {
                return false;
            }
            return true;
        };

        auto try_find_gpu =
            [&gpus_props, &check_surface_support, &check_extensions_support, &check_queue_families_support](vk::PhysicalDeviceType device_type) -> vk::PhysicalDevice {
            for (const auto& gpu : gpus_props) {
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
        auto device_layers = gather_layers_names([this]() -> std::vector<vk::LayerProperties> {
            return m_gpu.enumerateDeviceLayerProperties();
        },
                                                 data.layers,
                                                 implicit_required_device_layers);

        auto device_extensions = gather_extensions_names([this]() -> std::vector<vk::ExtensionProperties> {
            return m_gpu.enumerateDeviceExtensionProperties();
        },
                                                         data.extensions,
                                                         implicit_required_device_extensions);

        vk::QueueFamilyProperties props{};

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
            queue_create_infos.emplace_back() = vk::DeviceQueueCreateInfo{
                .flags = {},
                .queueFamilyIndex = static_cast<uint32_t>(curr->second.first),
                .queueCount = static_cast<uint32_t>(curr->second.second),
                .pQueuePriorities = queues_priorities.back().data()};
        }

        vk::PhysicalDeviceFeatures features = m_gpu.getFeatures();

        vk::DeviceCreateInfo device_info{
            .flags = {},
            .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
            .pQueueCreateInfos = queue_create_infos.data(),
            .enabledLayerCount = static_cast<uint32_t>(device_layers.size()),
            .ppEnabledLayerNames = device_layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
            .ppEnabledExtensionNames = device_extensions.data(),
            .pEnabledFeatures = &features};

        m_device = avk::create_device(m_gpu, &device_info, nullptr);
    }


    void create_allocator()
    {
        VmaAllocatorCreateInfo allocator_info{
            .physicalDevice = static_cast<VkPhysicalDevice>(m_gpu),
            .device = static_cast<VkDevice>(static_cast<vk::Device>(m_device)),
            .instance = static_cast<VkInstance>(static_cast<vk::Instance>(m_instance)),
            .vulkanApiVersion = VK_API_VERSION_1_2,
        };

        m_allocator = ::create_allocator(allocator_info);
    }

    avk::instance m_instance{};
    vk::PhysicalDevice m_gpu{};
    avk::device m_device{};
    avk::allocator m_allocator{};
    mutable std::unordered_map<vk::QueueFlagBits, std::pair<int32_t, int32_t>> m_queue_families_data{};
};


vk::PhysicalDevice* hal::render::avk::context::gpu()
{
    return &m_impl->m_gpu;
}


vk::Device* hal::render::avk::context::device()
{
    return m_impl->m_device.handler_ptr();
}


vk::Instance* hal::render::avk::context::instance()
{
    return m_impl->m_instance.handler_ptr();
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


VmaAllocator context::allocator()
{
    return m_impl->allocator();
}
