#pragma once

#include <functional>

namespace sandbox::utils
{
    struct on_scope_exit
    {
        explicit on_scope_exit(const std::function<void()>& exit_callback)
            : m_exit_callback(exit_callback)
        {
        }

        on_scope_exit(const on_scope_exit&) = delete;
        on_scope_exit& operator=(const on_scope_exit&) = delete;
        on_scope_exit(on_scope_exit&&) noexcept = delete;
        on_scope_exit& operator=(on_scope_exit&&) noexcept = delete;

        ~on_scope_exit()
        {
            if (m_exit_callback) {
                m_exit_callback();
            }
        }

    private:
        std::function<void()> m_exit_callback{};
    };
} // namespace sandbox::utils
