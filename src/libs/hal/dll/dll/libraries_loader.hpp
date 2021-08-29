#pragma once

#include <functional>
#include <type_traits>
#include <stdexcept>

#ifdef WIN32
    #include <windows.h>
#endif

namespace sandbox::hal::dll
{
#ifdef WIN32
    static_assert(std::is_pointer_v<HMODULE>);
    using library_ptr = HMODULE;
    using library_handler = std::unique_ptr<std::remove_pointer_t<HMODULE>, std::function<void(library_ptr)>>;
    using proc_ptr = std::remove_reference_t<decltype(GetProcAddress(std::declval<HMODULE>(), "some_proc_name"))>;
#endif

    class loading_library_error : public std::exception
    {
    public:
        explicit loading_library_error(const std::string& lib_path);

        ~loading_library_error() noexcept override = default;
        const char* what() const override;

    private:
        std::string m_err_msg{};
    };


    library_handler load_library(const char*);
    proc_ptr get_proc_address(const library_handler&, const char*);
} // namespace sandbox::hal::dll
