#pragma once

#include <memory>
#include <vector>

struct GLFWwindow;

namespace sandbox::hal
{
    class window {
    public:
        window(size_t width, size_t height, const std::string& name);
        ~window();

        bool closed() const;
        void main_loop();

    private:
        using winow_hanlder = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
        winow_hanlder m_window{nullptr, nullptr};
    };
}

