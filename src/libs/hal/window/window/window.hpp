#pragma once

#include <memory>
#include <vector>
#include <string>

namespace sandbox::hal
{
    class window
    {
    public:
        class main_loop_update_listener
        {
        public:
            virtual ~main_loop_update_listener() = default;
            virtual void update(uint64_t dt) = 0;
        };

        class impl
        {
        public:
            virtual ~impl() = default;

            std::unique_ptr<impl> static create(
                size_t width,
                size_t height,
                const std::string& name,
                std::unique_ptr<main_loop_update_listener>);

            virtual bool closed() = 0;
            virtual void main_loop() = 0;
        };

        window(
            size_t width,
            size_t height,
            const std::string& name,
            std::unique_ptr<main_loop_update_listener> update_listener);

        ~window() = default;

        bool closed() const;
        void main_loop();

    private:
        std::unique_ptr<impl> m_impl;
    };
} // namespace sandbox::hal
