/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CRASH_HANDLER_HPP
#define HYPERION_CRASH_HANDLER_HPP

#include <core/Defines.hpp>

#include <rendering/backend/RendererResult.hpp>

namespace hyperion {

class HYP_API CrashHandler
{
public:
    CrashHandler();

    void Initialize();
    void HandleGPUCrash(RendererResult result);

private:
    bool m_is_initialized;
};

} // namespace hyperion

#endif