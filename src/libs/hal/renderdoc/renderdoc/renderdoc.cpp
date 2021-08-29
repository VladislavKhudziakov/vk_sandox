#include "renderdoc.hpp"

#include <dll/libraries_loader.hpp>

#ifdef SANDBOX_RENDERDOC_LIB_PATH
    #include <renderdoc_app.h>
#endif

using namespace sandbox::hal;

class renderdoc::renderdoc::impl
{
public:
    impl()
#ifdef SANDBOX_RENDERDOC_LIB_PATH
        : m_library{dll::load_library(SANDBOX_RENDERDOC_LIB_PATH)}
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            reinterpret_cast<pRENDERDOC_GetAPI>(dll::get_proc_address(m_library, "RENDERDOC_GetAPI"));

        int load_successful = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**) &m_api);

        if (!load_successful) {
            throw std::runtime_error("Cannot load renderdoc library");
        }
    }
#else
    {
    }
#endif

    void start_capture() const
    {
#ifdef SANDBOX_RENDERDOC_LIB_PATH
        m_api->StartFrameCapture(NULL, NULL);
#endif
    }

    void end_capture() const
    {
#ifdef SANDBOX_RENDERDOC_LIB_PATH
        m_api->EndFrameCapture(NULL, NULL);
#endif
    }

private:
    dll::library_handler m_library{nullptr, [](dll::library_ptr) {}};
#ifdef SANDBOX_RENDERDOC_LIB_PATH
    RENDERDOC_API_1_1_2* m_api{nullptr};
#endif
};


std::weak_ptr<renderdoc::renderdoc::impl> renderdoc::renderdoc::global_instance{};


renderdoc::renderdoc::renderdoc()
{
    if (auto instance = global_instance.lock(); instance != nullptr) {
        m_local_instance = instance;
    } else {
        m_local_instance = std::make_shared<renderdoc::renderdoc::impl>();
        global_instance = m_local_instance;
    }
}


void renderdoc::renderdoc::start_capture() const
{
    m_local_instance->start_capture();
}


void renderdoc::renderdoc::end_capture() const
{
    m_local_instance->end_capture();
}