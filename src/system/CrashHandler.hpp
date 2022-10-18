#ifndef HYPERION_V2_CRASH_HANDLER_HPP
#define HYPERION_V2_CRASH_HANDLER_HPP

#include <rendering/backend/RendererResult.hpp>

namespace hyperion::v2 {

using renderer::Result;

class CrashHandler
{
public:
    void Initialize();
    void HandleGPUCrash(Result result);

private:
    bool m_is_initialized = false;
};

} // namespace hyperion::v2

#endif