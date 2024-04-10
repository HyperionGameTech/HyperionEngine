#ifndef HYPERION_V2_CRASH_HANDLER_HPP
#define HYPERION_V2_CRASH_HANDLER_HPP

#include <util/Defines.hpp>

#include <rendering/backend/RendererResult.hpp>

namespace hyperion::v2 {

using renderer::Result;

class HYP_API CrashHandler
{
public:
    void Initialize();
    void HandleGPUCrash(Result result);

private:
    bool m_is_initialized = false;
};

} // namespace hyperion::v2

#endif