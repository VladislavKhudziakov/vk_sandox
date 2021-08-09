
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

        const char *what() const override;

        vk::Result result() const;

    private:
        vk::Result m_result;
        std::string m_error_message;
    };
}

#ifndef NDEBUG

#define VK_CALL(expr)                                               \
{                                                                   \
    vk::Result res = static_cast<vk::Result>(expr);                 \
    if (res != vk::Result::eSuccess)  {                             \
         throw sandbox::hal::render::avk::vulkan_result_error(res); \
    }                                                               \
}

#else
    #define VK_CALL(expr) (expr);
#endif