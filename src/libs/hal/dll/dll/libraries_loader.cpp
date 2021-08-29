

#include "libraries_loader.hpp"

using namespace sandbox::hal;

#ifdef WIN32

dll::library_handler sandbox::hal::dll::load_library(const char* path)
{
    auto mod = LoadLibrary(path);
    if (mod == nullptr) {
        throw loading_library_error(path);
    }
    return library_handler{mod, FreeLibrary};
}


dll::proc_ptr sandbox::hal::dll::get_proc_address(const library_handler& handler, const char* proc_name)
{
    return GetProcAddress(handler.get(), proc_name);
}

#endif


const char* sandbox::hal::dll::loading_library_error::what() const
{
    return m_err_msg.c_str();
}


sandbox::hal::dll::loading_library_error::loading_library_error(const std::string& lib_path)
    : m_err_msg("Cannot load library " + lib_path)
{
}
