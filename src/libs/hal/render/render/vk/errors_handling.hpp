#pragma once

#include <render/vk/vulkan_dependencies.hpp>

#include <stdexcept>
#include <functional>

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
    namespace sandbox::hal::render::avk::detail
    {
        template <typename T>
        struct vk_call_handler
        {
            static void handle_error(const std::function<T()>& callback)
            {
                callback();
            }
        };

        template<> struct vk_call_handler<vk::Result>
        {
            static void handle_error(const std::function<vk::Result()>& callback)
            {
                if (const auto result = callback(); result != vk::Result::eSuccess) {
                    throw sandbox::hal::render::avk::vulkan_result_error(result);
                }
            }
        };

        template<> struct vk_call_handler<VkResult>
        {
            static void handle_error(const std::function<VkResult()>& callback)
            {
                if (const auto result = callback(); result != VK_SUCCESS) {
                    throw sandbox::hal::render::avk::vulkan_result_error(vk::Result(result));
                }
            }
        };
    }

    #define VK_CALL(expr)                                                                                                           \
        do {                                                                                                                        \
            sandbox::hal::render::avk::detail::vk_call_handler<std::decay_t<decltype(expr)>>::handle_error([&](){return (expr);});  \
        } while (false)

#else
    #define VK_CALL(expr) (expr)
#endif