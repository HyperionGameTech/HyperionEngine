/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Frame.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Frame final : public FrameBase
{
    HYP_OBJECT_BODY(DX12Frame);

public:
    DX12Frame();
    explicit DX12Frame(uint32 frameIndex);
    ~DX12Frame() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    void OnFrameStart() override;
};

} // namespace Hyperion
