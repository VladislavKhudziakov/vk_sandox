
#include "common_file.hpp"

void sandbox::hal::filesystem::common_file::open(const std::string& url)
{
    m_file_handler.reset(std::fopen(url.c_str(), "rb"));
    m_data_buffer.clear();
    m_size = 0;
}


void sandbox::hal::filesystem::common_file::close()
{
    m_file_handler.reset();
    m_data_buffer.clear();
    m_size = 0;
}


sandbox::utils::data sandbox::hal::filesystem::common_file::read_all()
{
    if (!m_data_buffer.empty()) {
        sandbox::utils::data::create_non_owning(m_data_buffer.data(), m_data_buffer.size());
    }

    if (const auto sz = get_size(); sz > 0) {
        m_data_buffer.resize(sz);
        std::fread(m_data_buffer.data(), 1, sz, m_file_handler.get());
        return sandbox::utils::data::create_non_owning(m_data_buffer.data(), sz);
    }

    return {};
}


size_t sandbox::hal::filesystem::common_file::get_size()
{
    if (m_file_handler == nullptr) {
        return 0;
    }

    if (m_size != 0) {
        return m_size;
    }

    std::fseek(m_file_handler.get(), 0, SEEK_END);
    m_size = std::ftell(m_file_handler.get());
    std::fseek(m_file_handler.get(), 0, SEEK_SET);

    return m_size;
}
