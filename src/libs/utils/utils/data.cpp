
#include "data.hpp"


sandbox::utils::data sandbox::utils::data::create_non_owning(uint8_t* ptr, size_t size)
{
    return {{ptr, [](uint8_t*) {}}, size};
}


sandbox::utils::data sandbox::utils::data::create_owning(
    uint8_t* ptr, const std::function<void(uint8_t*)>& deleter, size_t size)
{
    return {{ptr, deleter}, size};
}


sandbox::utils::data::data(std::unique_ptr<uint8_t, std::function<void(uint8_t*)>> data_handler, size_t size)
    : m_data(std::move(data_handler))
    , m_size(size)
{
}


uint8_t* sandbox::utils::data::get_data()
{
    return m_data.get();
}


const uint8_t* sandbox::utils::data::get_data() const
{
    return m_data.get();
}


size_t sandbox::utils::data::get_size() const
{
    return m_size;
}
