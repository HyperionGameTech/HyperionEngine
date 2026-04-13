/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>

#include <DX12CommandBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface* g_renderInterface;

DX12CommandBuffer::DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE type)
    : m_type(type),
      m_isRecording(false)
{
}

DX12CommandBuffer::~DX12CommandBuffer()
{
}

bool DX12CommandBuffer::IsCreated() const
{
    return m_commandAllocator != nullptr && m_commandList != nullptr;
}

RendererResult DX12CommandBuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    ID3D12Device* device = g_renderInterface->GetDevice();

    if (m_commandAllocator == nullptr)
    {
        HRESULT res = device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&m_commandAllocator));
        if (!SUCCEEDED(res))
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to create command allocator!", res);
        }
    }

    HRESULT res = device->CreateCommandList(0, m_type, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create command list!", res);

    m_commandList->Close();

    return {};
}

void DX12CommandBuffer::Begin()
{
    Assert(m_commandAllocator != nullptr);
    Assert(m_commandList != nullptr);
    Assert(!m_isRecording, "Command buffer is already recording!");

    Assert(SUCCEEDED(m_commandAllocator->Reset()));
    Assert(SUCCEEDED(m_commandList->Reset(m_commandAllocator.Get(), nullptr)));

    m_isRecording = true;
}

void DX12CommandBuffer::End()
{
    Assert(m_commandList != nullptr);
    Assert(m_isRecording, "Command buffer is not recording!");

    Assert(SUCCEEDED(m_commandList->Close()));

    m_isRecording = false;
}

void DX12CommandBuffer::BindVertexBuffer(const DX12GpuBuffer* buffer)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::BindVertexBuffer() not implemented");
}

void DX12CommandBuffer::BindIndexBuffer(const DX12GpuBuffer* buffer, GpuElemType elemType)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::BindIndexBuffer() not implemented");
}

void DX12CommandBuffer::DrawIndexed(
    uint32 numIndices,
    uint32 numInstances,
    uint32 instanceIndex) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::DrawIndexed() not implemented");
}

void DX12CommandBuffer::DrawIndexedIndirect(
    const DX12GpuBuffer* buffer,
    uint32 bufferOffset) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::DrawIndexedIndirect() not implemented");
}

} // namespace Hyperion
