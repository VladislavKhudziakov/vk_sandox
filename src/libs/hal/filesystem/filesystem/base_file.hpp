#pragma once

#include <utils/data.hpp>

#include <cinttypes>
#include <string>

namespace sandbox::hal::filesystem
{
    class base_file
    {
    public:
        virtual ~base_file() = default;
        virtual void open(const std::string& url) = 0;
        virtual void close() = 0;
        virtual sandbox::utils::data read_all() = 0;
        virtual sandbox::utils::data read_all_and_move() = 0;

        virtual size_t get_size() = 0;
    };
} // namespace sandbox::hal::filesystem
