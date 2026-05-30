/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

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

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface RI;

DX12AsyncCompute::DX12AsyncCompute()
    : m_commandBuffer(nullptr),
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
    }

    if (m_commandBuffer != nullptr)
    {
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

    const DX12QueueData* computeQueueData = RI.GetQueueData(D3D12_COMMAND_LIST_TYPE_COMPUTE);

    m_isSupported = computeQueueData != nullptr && computeQueueData->commandQueue != nullptr;

    if (!m_isSupported)
    {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");

        m_commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;
    }

    CheckResult(m_fence->Create());

    m_commandBuffer = new DX12CommandBuffer(m_commandListType);
    CheckResult(m_commandBuffer->Create());
}

void DX12AsyncCompute::Submit()
{
    Assert(CheckStatus(), "GPU work must be completed from previous submission before DX12AsyncCompute::Submit() is ever called!");

    if (m_fence->isSubmitted)
    {
        CheckResult(m_fence->Wait(true));
    }

    const DX12QueueData* queueData = RI.GetQueueData(m_commandListType);
    Assert(queueData != nullptr && queueData->commandQueue != nullptr);

    // Begin resets the allocator and opens the command list for recording
    m_commandBuffer->Begin();

    cr.Execute(m_commandBuffer);

    m_commandBuffer->End();

    ID3D12CommandList* commandLists[] = { m_commandBuffer->GetCommandList() };
    queueData->commandQueue->ExecuteCommandLists(UINT(std::size(commandLists)), commandLists);

    m_fence->Increment();

    HRESULT res = queueData->commandQueue->Signal(m_fence->GetD3D12Fence(), m_fence->GetValue());
    Assert(SUCCEEDED(res));

    m_isSubmitted = true;
}

} // namespace Hyperion
