/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Fence final : public ObjectBase
{
    HYP_OBJECT_BODY(DX12Fence);

public:
    DX12Fence();
    virtual ~DX12Fence() override;

    RendererResult Create();
    RendererResult Wait(bool timeoutLoop = false);
    RendererResult Reset();
};

} // namespace Hyperion
