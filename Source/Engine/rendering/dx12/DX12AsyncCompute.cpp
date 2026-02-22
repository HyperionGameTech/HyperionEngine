/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

/* GpuBuffer and GpuImage must be included before DX12AsyncCompute to prevent incomplete types */
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12AsyncCompute.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12Fence.hpp>

#include <Core/logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface* g_renderInterface;

DX12AsyncCompute::DX12AsyncCompute()
    : m_commandBuffer(new DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE_COMPUTE)),
      m_fence(new DX12Fence()),
      m_commandListType(D3D12_COMMAND_LIST_TYPE_COMPUTE),
      m_isSupported(false),
      m_isSubmitted(false)
{
}

DX12AsyncCompute::~DX12AsyncCompute()
{
    if (m_fence != nullptr)
    {
        if (m_isSubmitted && !CheckStatus())
        {
            m_fence->Wait();
        }

        m_fence->Release();
        m_fence = nullptr;

        m_commandBuffer->Release();
        m_commandBuffer = nullptr;
    }
}

bool DX12AsyncCompute::CheckStatus()
{
    Assert(m_fence != nullptr);

    if (!m_isSubmitted)
    {
        return true;
    }

    if (m_fence->GetD3D12Fence()->GetCompletedValue() < m_fence->GetValue())
    {
        return false;
    }

    m_isSubmitted = false;

    return true;
}

void DX12AsyncCompute::Create()
{
    HYP_SCOPE;

    const DX12QueueData* computeQueueData = g_renderInterface->GetQueueData(D3D12_COMMAND_LIST_TYPE_COMPUTE);

    m_isSupported = computeQueueData != nullptr && computeQueueData->commandQueue != nullptr;

    if (!m_isSupported)
    {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");

        m_commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;

        m_commandBuffer->Release();
        m_commandBuffer = new DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    CheckResult(m_commandBuffer->Create());

    CheckResult(m_fence->Create(/* createSignalled */ true));

    HRESULT res = g_renderInterface->GetDevice()->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&m_commandAllocator));
    Assert(SUCCEEDED(res));
}

void DX12AsyncCompute::Submit()
{
    Assert(CheckStatus());
    Assert(m_commandAllocator != nullptr);

    CheckResult(m_fence->Wait(true));
    CheckResult(m_fence->Reset());

    const DX12QueueData* queueData = g_renderInterface->GetQueueData(m_commandListType);
    Assert(queueData != nullptr && queueData->commandQueue != nullptr);

    ID3D12GraphicsCommandList* commandList = m_commandBuffer->GetCommandList();
    Assert(commandList != nullptr);

    Assert(SUCCEEDED(m_commandAllocator->Reset()));
    Assert(SUCCEEDED(commandList->Reset(m_commandAllocator.Get(), nullptr)));

    renderQueue.Execute(m_commandBuffer);

    Assert(SUCCEEDED(commandList->Close()));

    ID3D12CommandList* commandLists[] = { commandList };
    queueData->commandQueue->ExecuteCommandLists(ArraySize(commandLists), commandLists);

    HRESULT res = queueData->commandQueue->Signal(m_fence->GetD3D12Fence(), m_fence->GetValue());
    Assert(SUCCEEDED(res));

    m_isSubmitted = true;
}

} // namespace Hyperion
