#pragma once

#include <functional>
#include <memory>

namespace sandbox::utils
{
    class data
    {
    public:
        static data create_owning(uint8_t* ptr, const std::function<void(uint8_t*)>& deleter, size_t size);
        static data create_non_owning(uint8_t*, size_t size);

        data() = default;
        ~data() = default;

        const uint8_t* get_data() const;
        uint8_t* get_data();

        size_t get_size() const;

    private:
        data(std::unique_ptr<uint8_t, std::function<void(uint8_t*)>>, size_t);
        std::unique_ptr<uint8_t, std::function<void(uint8_t*)>> m_data{};
        size_t m_size{0};
    };
} // namespace sandbox::utils
