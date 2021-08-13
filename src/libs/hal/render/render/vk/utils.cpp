//
// Created by super on 04.08.2021.
//

#include "utils.hpp"

sandbox::hal::render::avk::vulkan_result_error::vulkan_result_error(vk::Result result)
    : exception()
    , m_result(result)
    , m_error_message("Vulkan::Error" + vk::to_string(result))
{
}


sandbox::hal::render::avk::vulkan_result_error::~vulkan_result_error() noexcept = default;


const char* sandbox::hal::render::avk::vulkan_result_error::what() const
{
    return m_error_message.c_str();
}


vk::Result sandbox::hal::render::avk::vulkan_result_error::result() const
{
    return m_result;
}
