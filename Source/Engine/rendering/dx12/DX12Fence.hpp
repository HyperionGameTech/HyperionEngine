/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>
#include <rendering/dx12/DX12Shared.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Fence final : public ObjectBase
{
    HYP_OBJECT_BODY(DX12Fence);

public:
    DX12Fence();
    ~DX12Fence() override;

    HYP_FORCE_INLINE ID3D12Fence* GetD3D12Fence() const
    {
        return m_fence.Get();
    }

    HYP_FORCE_INLINE uint64 GetValue() const
    {
        return m_value;
    }

    RendererResult Create(bool createSignalled = false);
    RendererResult Wait(bool timeoutLoop = false);
    RendererResult Reset();

private:
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_eventHandle;
    uint64 m_value;
};

} // namespace Hyperion
