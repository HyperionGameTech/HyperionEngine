/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Fence.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>

#include <DX12Fence.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Fence

DX12Fence::DX12Fence()
    : m_eventHandle(nullptr),
      m_value(0)
{
}

DX12Fence::~DX12Fence()
{
    if (m_eventHandle != nullptr)
    {
        CloseHandle(m_eventHandle);
        m_eventHandle = nullptr;
    }
}

RendererResult DX12Fence::Create(bool createSignalled)
{
    Assert(m_fence == nullptr);
    Assert(m_eventHandle == nullptr);

    m_value = 0;

    m_eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (m_eventHandle == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create wait event for D3D12 fence");
    }

    HRESULT hr = g_renderInterface->GetDevice()->CreateFence(
        createSignalled ? 1 : 0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&m_fence));

    if (FAILED(hr))
    {
        CloseHandle(m_eventHandle);
        m_eventHandle = nullptr;

        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D12 fence", hr);
    }

    m_value = createSignalled ? 1 : 0;

    return {};
}

RendererResult DX12Fence::Wait(bool timeoutLoop)
{
    Assert(m_fence != nullptr);
    Assert(m_eventHandle != nullptr);

    if (m_fence->GetCompletedValue() >= m_value)
    {
        return {};
    }

    HRESULT hr = m_fence->SetEventOnCompletion(m_value, m_eventHandle);

    if (FAILED(hr))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to set D3D12 fence completion event", hr);
    }

    DWORD waitResult;

    do
    {
        waitResult = WaitForSingleObject(m_eventHandle, timeoutLoop ? 100 : INFINITE);
    }
    while (timeoutLoop && waitResult == WAIT_TIMEOUT);

    if (waitResult != WAIT_OBJECT_0)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed while waiting for D3D12 fence completion");
    }

    return {};
}

RendererResult DX12Fence::Reset()
{
    Assert(m_fence != nullptr);

    ++m_value;

    return {};
}

#pragma endregion DX12Fence

} // namespace Hyperion
