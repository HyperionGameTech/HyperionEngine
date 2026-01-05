/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Swapchain.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Swapchain final : public SwapchainBase
{
    HYP_OBJECT_BODY(DX12Swapchain);

public:
    DX12Swapchain(const Vec2u& extent);
    virtual ~DX12Swapchain() override;

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;
    virtual void SetExtent(Vec2u newExtent) override;
    virtual void Recreate() override;
};

} // namespace Hyperion
