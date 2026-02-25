/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <rendering/RenderResult.hpp>

namespace Hyperion {

class HYP_API CrashHandler
{
public:
    CrashHandler();
    ~CrashHandler();

    void Initialize();
    void HandleGPUCrash(RendererResult result);

private:
    bool m_isInitialized;
};

} // namespace Hyperion
