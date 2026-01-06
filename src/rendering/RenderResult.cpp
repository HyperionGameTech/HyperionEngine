/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderResult.hpp>

namespace Hyperion {

bool CheckResult(const RendererResult& result)
{
    Assert(result, "Renderer error [{}]: {}", result.GetError().GetErrorCode(), *result.GetError().GetMessage());

    if (!result)
    {
        return false;
    }

    return true;
}

} // namespace Hyperion
