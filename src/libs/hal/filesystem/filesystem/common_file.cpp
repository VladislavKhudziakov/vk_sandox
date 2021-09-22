
#include "common_file.hpp"

#include <filesystem>

sandbox::hal::filesystem::common_file::common_file(const std::string& mount_dir)
    : m_cwd(mount_dir)
{
    if (!m_cwd.empty()) {
        m_prev_cwd = std::filesystem::current_path().string();
        std::filesystem::current_path(m_cwd);
    }
}


sandbox::hal::filesystem::common_file::~common_file()
{
    if (!m_prev_cwd.empty()) {
        std::filesystem::current_path(m_prev_cwd);
    }
}


void sandbox::hal::filesystem::common_file::open(const std::string& url)
{
    std::string full_path{};
    if (!m_cwd.empty()) {
        full_path = (std::filesystem::path(m_cwd) / std::filesystem::path(url)).string();
    }

    m_file_handler.reset(std::fopen(full_path.empty() ? url.c_str() : full_path.c_str(), "rb"));

    if (!m_file_handler) {
        throw filesystem::cannot_open_file_error("cannot open file " + url + (m_cwd.empty() ? "" : " in directory " + m_cwd) + ".");
    }

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


sandbox::utils::data sandbox::hal::filesystem::common_file::read_all_and_move()
{
    auto move_data = [this]() {
        const auto sz = m_data_buffer.size();
        auto data_buffer = new uint8_t[sz];
        std::memcpy(data_buffer, m_data_buffer.data(), m_data_buffer.size());
        return sandbox::utils::data::create_owning(
            data_buffer, [](uint8_t* data) { delete[] data; }, sz);
    };

    if (!m_data_buffer.empty()) {
        return move_data();
    }

    if (const auto sz = get_size(); sz > 0) {
        m_data_buffer.resize(sz);
        std::fread(m_data_buffer.data(), 1, sz, m_file_handler.get());
        return move_data();
    }

    return {};
}
