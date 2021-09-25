#pragma once

#include <render/vk/vulkan_dependencies.hpp>

#include <stdexcept>

namespace sandbox::hal::render::avk
{
    class vulkan_result_error : public std::exception
    {
    public:
        explicit vulkan_result_error(vk::Result result);
        ~vulkan_result_error() noexcept override;

        const char* what() const override;

        vk::Result result() const;

    private:
        vk::Result m_result;
        std::string m_error_message;
    };
} // namespace sandbox::hal::render::avk

#ifndef NDEBUG
    #define VK_CALL(expr)                                                  \
        do {                                                               \
            vk::Result res = static_cast<vk::Result>(expr);                \
            if (res != vk::Result::eSuccess) {                             \
                throw sandbox::hal::render::avk::vulkan_result_error(res); \
            }                                                              \
        } while (false)

    #define VK_C_CALL(expr)                                                            \
        do {                                                                           \
            VkResult res = (expr);                                                     \
            if (res != VK_SUCCESS) {                                                   \
                throw sandbox::hal::render::avk::vulkan_result_error(vk::Result(res)); \
            }                                                                          \
        } while (false)

#else
    #define VK_CALL(expr) (expr)
    #define VK_C_CALL(expr) (expr)
#endif