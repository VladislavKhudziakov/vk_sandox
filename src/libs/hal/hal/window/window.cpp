
#include "window.hpp"


sandbox::hal::window::window(size_t width, size_t height, const std::string& name)
    : m_impl(impl::create(width, height, name))
{
}


void sandbox::hal::window::main_loop()
{
    m_impl->main_loop();
}


bool sandbox::hal::window::closed() const
{
    return m_impl->closed();
}
