#pragma once

#if !defined(NDEBUG) && defined(SANDBOX_RENDERDOC_LIB_PATH)
    #define RDOC_INIT_SCOPE()                    \
        sandbox::hal::renderdoc::renderdoc rdoc_ \
        {                                        \
        }
    #define RDOC_START_CAPTURE() rdoc_.start_capture()
    #define RDOC_END_CAPTURE() rdoc_.end_capture()
#else
    #define RDOC_INIT_SCOPE()
    #define RDOC_START_CAPTURE()
    #define RDOC_END_CAPTURE()
#endif

#include <memory>

namespace sandbox::hal::renderdoc
{
    class renderdoc
    {
        class impl;

    public:
        renderdoc();

        void start_capture() const;
        void end_capture() const;

    private:
        static std::weak_ptr<impl> global_instance;
        std::shared_ptr<impl> m_local_instance{};
    };
} // namespace sandbox::hal::renderdoc
