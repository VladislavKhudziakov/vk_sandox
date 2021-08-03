#pragma once

#include <memory>
#include <vector>
#include <string>

struct GLFWwindow;

namespace sandbox::hal
{
    class window {

    public:
        class impl
        {
        public:
            virtual ~impl() = default;

            std::unique_ptr<impl> static create(size_t width, size_t height, const std::string& name);
            virtual bool closed() = 0;
            virtual void main_loop() = 0;
        };

        window(size_t width, size_t height, const std::string& name);
        ~window() = default;

        bool closed() const;
        void main_loop();

    private:
        std::unique_ptr<impl> m_impl;

        using winow_hanlder = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
        winow_hanlder m_window{nullptr, nullptr};
    };
}

