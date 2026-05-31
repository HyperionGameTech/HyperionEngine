/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/Swapchain.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/Defines.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Rendering/dx12/DX12DescriptorHeaps.hpp>
#include <Rendering/dx12/DX12Shared.hpp>

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
    void Destroy();
    void FlushGPU();

    HWND m_hwnd;
    ComPtr<IDXGISwapChain4> m_swapChain;
    Array<ID3D12Resource*, DX12Allocator> m_backBuffers;

    DX12DescriptorHandle m_rtvDescriptorHandle;
    Array<D3D12_CPU_DESCRIPTOR_HANDLE, DX12Allocator> m_rtvHandles;

    uint32 m_currentBackBufferIndex;

    bool m_allowTearing;

    // GPU flush resources for safe swapchain recreation
    ComPtr<ID3D12CommandAllocator> m_flushAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_flushCommandList;
    ComPtr<ID3D12Fence> m_flushFence;
    HANDLE m_flushEvent = nullptr;
    uint64 m_flushFenceValue = 0;
};

} // namespace Hyperion
