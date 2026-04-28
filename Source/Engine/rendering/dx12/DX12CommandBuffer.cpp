/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/Vertex.hpp>

#include <DX12CommandBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface* g_renderInterface;

DX12CommandBuffer::DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE type)
    : m_type(type),
      m_isRecording(false),
      m_currentAllocatorIndex(0),
      m_currentCommandListIndex(0),
      m_boundGraphicsPipeline(nullptr)
{
}

DX12CommandBuffer::~DX12CommandBuffer()
{
}

bool DX12CommandBuffer::IsCreated() const
{
    return m_commandAllocators[0] != nullptr && m_commandLists[0] != nullptr;
}

RendererResult DX12CommandBuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    ID3D12Device* device = g_renderInterface->GetDevice();

    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        if (m_commandAllocators[i] == nullptr)
        {
            HRESULT res = device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&m_commandAllocators[i]));
            if (!SUCCEEDED(res))
            {
                return HYP_MAKE_ERROR(RendererError, "Failed to create command allocator!", res);
            }
        }
    }

    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        if (m_commandLists[i] == nullptr)
        {
            HRESULT res = device->CreateCommandList(0, m_type, m_commandAllocators[i].Get(), nullptr, IID_PPV_ARGS(&m_commandLists[i]));
            if (!SUCCEEDED(res))
            {
                return HYP_MAKE_ERROR(RendererError, "Failed to create command list!", res);
            }

            m_commandLists[i]->Close();
        }
    }

    return {};
}

void DX12CommandBuffer::Begin()
{
    Assert(m_commandLists[m_currentCommandListIndex] != nullptr);
    Assert(!m_isRecording, "Command buffer is already recording!");

    const uint32 index = m_currentAllocatorIndex;
    m_currentAllocatorIndex = (m_currentAllocatorIndex + 1) % NumFramesInFlight;
    m_currentCommandListIndex = index;

    Assert(m_commandAllocators[index] != nullptr);

    Assert(SUCCEEDED(m_commandAllocators[index]->Reset()));
    Assert(SUCCEEDED(m_commandLists[index]->Reset(m_commandAllocators[index].Get(), nullptr)));

    m_isRecording = true;
}

void DX12CommandBuffer::End()
{
    Assert(m_commandLists[m_currentCommandListIndex] != nullptr);
    Assert(m_isRecording, "Command buffer is not recording!");

    ID3D12GraphicsCommandList* commandList = m_commandLists[m_currentCommandListIndex].Get();
    
    HRESULT closeResult = commandList->Close();
    if (FAILED(closeResult))
    {
        HYP_FAIL("Failed to close command buffer! Code: {}", closeResult);
    }

    m_isRecording = false;
}

void DX12CommandBuffer::BindVertexBuffer(const DX12GpuBuffer* buffer)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::MESH_VERTEX_BUFFER,
        "Not a vertex buffer! Got buffer type: {}", buffer->GetBufferType());

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = buffer->GetResource()->GetGPUVirtualAddress();
    vbView.SizeInBytes = buffer->Size();

    if (m_boundGraphicsPipeline != nullptr)
    {
        vbView.StrideInBytes = static_cast<UINT>(m_boundGraphicsPipeline->GetInputLayout().VertexSize());
    }

    m_commandLists[m_currentCommandListIndex]->IASetVertexBuffers(0, 1, &vbView);
}

void DX12CommandBuffer::BindIndexBuffer(const DX12GpuBuffer* buffer, GpuElemType elemType)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::MESH_INDEX_BUFFER,
        "Not an index buffer! Got buffer type: {}", buffer->GetBufferType());

    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = buffer->GetResource()->GetGPUVirtualAddress();
    ibView.SizeInBytes = buffer->Size();
    ibView.Format = ToDXGIFormat(elemType);

    m_commandLists[m_currentCommandListIndex]->IASetIndexBuffer(&ibView);
}

void DX12CommandBuffer::DrawIndexed(
    uint32 numIndices,
    uint32 numInstances,
    uint32 instanceIndex) const
{
    AssertDebug(m_boundGraphicsPipeline != nullptr);

    m_commandLists[m_currentCommandListIndex]->DrawIndexedInstanced(
        numIndices,
        numInstances,
        0,
        0,
        instanceIndex);
}

void DX12CommandBuffer::DrawIndexedIndirect(
    const DX12GpuBuffer* buffer,
    uint32 bufferOffset) const
{
    AssertDebug(m_boundGraphicsPipeline != nullptr);

    static ID3D12CommandSignature* s_drawIndexedCommandSignature = nullptr;

    if (s_drawIndexedCommandSignature == nullptr)
    {
        D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

        D3D12_COMMAND_SIGNATURE_DESC sigDesc = {};
        sigDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
        sigDesc.NumArgumentDescs = 1;
        sigDesc.pArgumentDescs = &argDesc;

        HRESULT hr = g_renderInterface->GetDevice()->CreateCommandSignature(
            &sigDesc,
            nullptr,
            IID_PPV_ARGS(&s_drawIndexedCommandSignature));

        if (FAILED(hr))
        {
            HYP_LOG(RenderingBackend, Error, "Failed to create draw indexed command signature!");
            return;
        }
    }

    m_commandLists[m_currentCommandListIndex]->ExecuteIndirect(
        s_drawIndexedCommandSignature,
        1,
        buffer->GetResource(),
        bufferOffset,
        nullptr,
        0);
}

void DX12CommandBuffer::Submit(
    ID3D12CommandQueue* commandQueue,
    ID3D12Fence* fence,
    uint64 fenceValue)
{
    AssertOnThread(g_renderThread);

    Assert(commandQueue != nullptr);

    if (m_isRecording)
    {
        End();
    }

    ID3D12CommandList* commandLists[] = { m_commandLists[m_currentCommandListIndex].Get() };
    commandQueue->ExecuteCommandLists(ArraySize(commandLists), commandLists);

    if (fence != nullptr)
    {
        commandQueue->Signal(fence, fenceValue);
    }
}

} // namespace Hyperion
