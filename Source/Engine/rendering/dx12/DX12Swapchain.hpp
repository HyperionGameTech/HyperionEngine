/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Swapchain.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/Defines.hpp>

#include <rendering/RenderObject.hpp>

#include <rendering/dx12/DX12DescriptorHeaps.hpp>
#include <rendering/dx12/DX12Shared.hpp>

#include <dxgi1_5.h>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Swapchain final : public SwapchainBase
{
    HYP_OBJECT_BODY(DX12Swapchain);

public:
    DX12Swapchain(HWND hwnd, const Vec2u& extent);
    ~DX12Swapchain() override;

    bool IsCreated() const override;

    RendererResult Create() override;
    void SetExtent(Vec2u newExtent) override;
    void Recreate() override;

    void PrepareForFrame(DX12Frame* frame);
    void PresentFrame(DX12Frame* frame);

    HYP_FORCE_INLINE IDXGISwapChain4* GetDXGISwapChain() const
    {
        return m_swapChain.Get();
    }

    HYP_FORCE_INLINE uint32 GetCurrentBackBufferIndex() const
    {
        return m_currentBackBufferIndex;
    }

    HYP_FORCE_INLINE ID3D12Resource* GetBackBuffer(uint32 index) const
    {
        return m_backBuffers[index];
    }

    HYP_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32 index) const
    {
        return m_rtvHandles[index];
    }

private:
    HWND m_hwnd;
    ComPtr<IDXGISwapChain4> m_swapChain;
    Array<ID3D12Resource*> m_backBuffers;
    
    DX12DescriptorHandle m_rtvDescriptorHandle;
    Array<D3D12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;

    uint32 m_currentBackBufferIndex;

    bool m_allowTearing;
    bool m_vsync;
};

} // namespace Hyperion
