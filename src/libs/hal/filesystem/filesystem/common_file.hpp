#pragma once

#include <filesystem/base_file.hpp>

#include <memory>
#include <cstdio>
#include <vector>

namespace sandbox::hal::filesystem
{
    class common_file : public base_file
    {
    public:
        explicit common_file(const std::string& cwd = "");
        common_file(const common_file&) = delete;
        common_file& operator=(const common_file&) = delete;
        common_file(common_file&&) noexcept = default;
        common_file& operator=(common_file&&) noexcept = default;
        ~common_file() override;

        void open(const std::string& url) override;
        void close() override;

        utils::data read_all() override;
        utils::data read_all_and_move() override;
        size_t get_size() override;

    private:
        std::unique_ptr<FILE, void (*)(FILE*)> m_file_handler{nullptr, [](FILE* f) {if (f != nullptr) fclose(f); }};
        size_t m_size{0};
        std::vector<uint8_t> m_data_buffer{};
        std::string m_cwd{};
        std::string m_prev_cwd{};
    };
} // namespace sandbox::hal::filesystem
