/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

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
