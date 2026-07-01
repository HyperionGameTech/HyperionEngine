/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12GpuBuffer.hpp>
#include <Rendering/DX12/DX12GpuImage.hpp>
#include <Rendering/DX12/DX12CommandBuffer.hpp>
#include <Rendering/DX12/DX12GraphicsPipeline.hpp>
#include <Rendering/DX12/DX12RenderInterface.hpp>
#include <Rendering/DX12/DX12Helpers.hpp>

#include <Rendering/Vertex.hpp>
#include <Rendering/CrashHandler.hpp>

#include <Core/Reflection/Enum.hpp>

#include <d3d12sdklayers.h>

#include <DX12CommandBuffer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface RI;

DX12CommandBuffer::DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE type, ID3D12CommandQueue* commandQueue)
    : m_type(type),
      m_commandQueue(commandQueue),
      m_isRecording(false),
      m_boundGraphicsPipeline(nullptr),
      m_boundViewHeap(nullptr),
      m_boundSamplerHeap(nullptr),
      m_indirectCommandSignature(nullptr)
{
    Assert(m_commandQueue != nullptr);

#ifdef HYP_DX12_DEBUG_RESOURCE_STATES
    m_debugCommandList = nullptr;
#endif
}

DX12CommandBuffer::DX12CommandBuffer(DX12CommandBuffer&& other) noexcept
    : m_type(other.m_type),
      m_commandQueue(other.m_commandQueue),
      m_commandList(std::move(other.m_commandList)),
      m_allocator(std::move(other.m_allocator)),
      m_isRecording(other.m_isRecording),
      m_boundViewHeap(other.m_boundViewHeap),
      m_boundSamplerHeap(other.m_boundSamplerHeap),
      m_boundGraphicsPipeline(other.m_boundGraphicsPipeline),
      m_indirectCommandSignature(other.m_indirectCommandSignature)
{
    other.m_isRecording = false;
    other.m_boundViewHeap = nullptr;
    other.m_boundSamplerHeap = nullptr;
    other.m_boundGraphicsPipeline = nullptr;
    other.m_indirectCommandSignature = nullptr;

#ifdef HYP_DX12_DEBUG_RESOURCE_STATES
    m_debugCommandList = other.m_debugCommandList;
    other.m_debugCommandList = nullptr;
#endif
}

DX12CommandBuffer& DX12CommandBuffer::operator=(DX12CommandBuffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

#ifdef HYP_DX12_DEBUG_RESOURCE_STATES
    if (m_debugCommandList != nullptr)
    {
        m_debugCommandList->Release();
        m_debugCommandList = nullptr;
    }

    m_debugCommandList = other.m_debugCommandList;
    other.m_debugCommandList = nullptr;
#endif

    if (m_commandList != nullptr)
    {
        m_commandList->Close();
        m_commandList.Reset();
    }

    m_allocator.Reset();

    m_type = other.m_type;
    m_commandQueue = other.m_commandQueue;
    m_commandList = std::move(other.m_commandList);
    m_allocator = std::move(other.m_allocator);
    m_isRecording = other.m_isRecording;
    m_boundViewHeap = other.m_boundViewHeap;
    m_boundSamplerHeap = other.m_boundSamplerHeap;
    m_boundGraphicsPipeline = other.m_boundGraphicsPipeline;
    m_indirectCommandSignature = other.m_indirectCommandSignature;

    other.m_isRecording = false;
    other.m_boundViewHeap = nullptr;
    other.m_boundSamplerHeap = nullptr;
    other.m_boundGraphicsPipeline = nullptr;
    other.m_indirectCommandSignature = nullptr;

    return *this;
}

DX12CommandBuffer::~DX12CommandBuffer()
{
#ifdef HYP_DX12_DEBUG_RESOURCE_STATES
    if (m_debugCommandList != nullptr)
    {
        m_debugCommandList->Release();
        m_debugCommandList = nullptr;
    }
#endif

    if (m_indirectCommandSignature != nullptr)
    {
        m_indirectCommandSignature->Release();
        m_indirectCommandSignature = nullptr;
    }

    if (m_commandList != nullptr)
    {
        m_commandList->Close();
        m_commandList.Reset();
    }

    m_allocator.Reset();
}

bool DX12CommandBuffer::IsCreated() const
{
    return m_commandList != nullptr;
}

RendererResult DX12CommandBuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    ID3D12Device* device = RI.GetDevice();

    // Create allocator
    HRESULT res = device->CreateCommandAllocator(m_type, __uuidof(ID3D12CommandAllocator), &m_allocator);

    if (!SUCCEEDED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create command allocator!", res);
    }

#ifdef HYP_RHI_DEBUG_NAMES
    const char* typeName = (m_type == D3D12_COMMAND_LIST_TYPE_DIRECT)
        ? "Direct"
        : (m_type == D3D12_COMMAND_LIST_TYPE_COMPUTE) ? "Compute"
                                                      : "Copy";

    WideString name = WideString(HYP_FORMAT("D3D12 Command Allocator [{}]", typeName));
    m_allocator->SetName(name.Data());
#endif

    // Create command list
    res = device->CreateCommandList(
        0, m_type,
        m_allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList));

    if (!SUCCEEDED(res))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create command list!", res);
    }

#ifdef HYP_RHI_DEBUG_NAMES
    name = WideString(HYP_FORMAT("D3D12 Command List [{}]", typeName));
    m_commandList->SetName(name.Data());
#endif

    m_commandList->Close();

    return {};
}

#ifdef HYP_RHI_DEBUG_NAMES
void DX12CommandBuffer::SetDebugName(const wchar_t* name)
{
    if (m_allocator != nullptr)
    {
        m_allocator->SetName(name);
    }

    if (m_commandList != nullptr)
    {
        m_commandList->SetName(name);
    }
}
#endif

void DX12CommandBuffer::Begin()
{
    AssertDebug(m_commandList != nullptr);
    AssertDebug(m_allocator != nullptr);
    AssertDebug(!m_isRecording, "Command buffer is already recording!");

    // Reset allocator before resetting command list
    Assert(SUCCEEDED(m_allocator->Reset()));
    Assert(SUCCEEDED(m_commandList->Reset(m_allocator.Get(), nullptr)));

    m_boundDescriptorSets.Clear();
    m_boundGraphicsPipeline = nullptr;

    m_isRecording = true;
}

void DX12CommandBuffer::End()
{
    AssertDebug(m_commandList != nullptr);
    AssertDebug(m_isRecording, "Command buffer is not recording!");

    HRESULT closeResult = m_commandList->Close();
    if (FAILED(closeResult))
    {
        HYP_FAIL("Failed to close command buffer! Code: {}", closeResult);
    }

    m_boundGraphicsPipeline = nullptr;

    // Reset bound descriptor heaps tracking since the command list has been reset
    ResetBoundDescriptorHeaps();

    m_isRecording = false;
}

void DX12CommandBuffer::BindVertexBuffer(const DX12GpuBuffer* buffer)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::VertexBuffer,
                "Not a vertex buffer! Got buffer type: {}", buffer->GetBufferType());

    AssertResourceState(*buffer, RS_VERTEX_BUFFER);

    D3D12_VERTEX_BUFFER_VIEW vbView {};
    vbView.BufferLocation = buffer->GetResource()->GetGPUVirtualAddress();
    vbView.SizeInBytes = buffer->Size();

    // Stride must be set correctly for the input layout. If the pipeline is not bound yet,
    // we cannot determine the correct stride. The pipeline should always be bound before
    // binding vertex buffers to ensure proper vertex attribute interpretation.
    if (m_boundGraphicsPipeline == nullptr)
    {
        HYP_FAIL("Graphics pipeline must be bound before binding vertex buffer to ensure correct stride!");
        return;
    }

    vbView.StrideInBytes = static_cast<UINT>(m_boundGraphicsPipeline->GetInputLayout().VertexSize());
    AssertDebug(vbView.StrideInBytes > 0, "Vertex stride must be greater than 0!");

    m_commandList->IASetVertexBuffers(0, 1, &vbView);
}

void DX12CommandBuffer::BindIndexBuffer(const DX12GpuBuffer* buffer, GpuElemType elemType)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::IndexBuffer,
                "Not an index buffer! Got buffer type: {}", buffer->GetBufferType());

    AssertResourceState(*buffer, RS_INDEX_BUFFER);

    D3D12_INDEX_BUFFER_VIEW ibView {};
    ibView.BufferLocation = buffer->GetResource()->GetGPUVirtualAddress();
    ibView.SizeInBytes = buffer->Size();
    ibView.Format = ToDXGIFormat(elemType);

    m_commandList->IASetIndexBuffer(&ibView);
}

void DX12CommandBuffer::DrawIndexed(uint32 numIndices, uint32 numInstances, uint32 instanceIndex) const
{
    AssertDebug(m_boundGraphicsPipeline != nullptr);

    m_commandList->DrawIndexedInstanced(
        numIndices,
        numInstances,
        0,
        0,
        instanceIndex);
}

void DX12CommandBuffer::DrawIndexedIndirect(const DX12GpuBuffer* buffer, uint32 bufferOffset) const
{
    AssertDebug(m_boundGraphicsPipeline != nullptr);
    AssertResourceState(*buffer, RS_INDIRECT_ARG);

    AssertDebug(bufferOffset % 4 == 0);

    if (HYP_UNLIKELY(!m_indirectCommandSignature))
    {
        D3D12_INDIRECT_ARGUMENT_DESC argDesc {};
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

        D3D12_COMMAND_SIGNATURE_DESC sigDesc {};
        sigDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
        sigDesc.NumArgumentDescs = 1;
        sigDesc.pArgumentDescs = &argDesc;

        HRESULT hr = RI.GetDevice()->CreateCommandSignature(
            &sigDesc,
            nullptr,
            __uuidof(ID3D12CommandSignature),
            (void**)&m_indirectCommandSignature);

        if (FAILED(hr))
        {
            HYP_LOG(RenderingBackend, Error, "Failed to create draw indexed command signature!");
            return;
        }
    }

    m_commandList->ExecuteIndirect(
        m_indirectCommandSignature,
        1,
        buffer->GetResource(),
        bufferOffset,
        nullptr,
        0);
}

void DX12CommandBuffer::Submit(
    ID3D12Fence* fence,
    uint64 fenceValue)
{
    AssertOnThread(g_renderThread);

    AssertDebug(m_commandQueue != nullptr);

    if (m_isRecording)
    {
        End();
    }

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    if (fence != nullptr)
    {
        HRESULT hr = m_commandQueue->Signal(fence, fenceValue);
        if (!SUCCEEDED(hr))
        {
            CrashHandler::Dump();

            const char* errorMsg = CheckDeviceRemovedReason(RI.GetDevice());
            HYP_LOG(RenderingBackend, Fatal, "Device removed: {}", errorMsg);
        }
    }
}

#ifdef HYP_DX12_DEBUG_RESOURCE_STATES

void DX12CommandBuffer::AssertResourceState(
    const DX12GpuBuffer& buffer,
    ResourceState expectedState) const
{
    ID3D12DebugCommandList* debugCommandList = GetDebugCommandList();
    Assert(debugCommandList != nullptr);

    BOOL succeeded = debugCommandList->AssertResourceState(
        buffer.GetResource(),
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        ToDX12ResourceStates(expectedState));

    Assert(succeeded == TRUE, "Resource state assertion failed! Expected Buffer (Name={}, ResourceState={}) to have state: {}",
            buffer.GetDebugName(),
            EnumToString(buffer.GetResourceState()),
            EnumToString(expectedState));
}

void DX12CommandBuffer::AssertResourceState(
    const DX12GpuImage& image,
    ResourceState expectedState) const
{
    ID3D12DebugCommandList* debugCommandList = GetDebugCommandList();
    Assert(debugCommandList != nullptr);

    BOOL succeeded = debugCommandList->AssertResourceState(
        image.GetResource(),
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        ToDX12ResourceStates(expectedState));

    Assert(succeeded == TRUE, "Resource state assertion failed! Expected Image (Name={}, ResourceState={}) to have state: {}",
            image.GetDebugName(),
            EnumToString(image.GetResourceState()),
            EnumToString(expectedState));
}

void DX12CommandBuffer::AssertResourceState(
    const DX12GpuImage& image,
    ResourceState expectedState,
    const ImageSubResource& subResource,
    bool onlyDepth,
    bool onlyStencil) const
{
    ID3D12DebugCommandList* debugCommandList = GetDebugCommandList();
    Assert(debugCommandList != nullptr);

    ImageSubResource newSubResource = subResource;
    newSubResource.numLayers = MathUtil::Min(newSubResource.numLayers, image.NumArrayLayers() - newSubResource.baseArrayLayer);
    newSubResource.numLevels = MathUtil::Min(newSubResource.numLevels, image.NumMips() - newSubResource.baseMipLevel);

    const uint32 planeSlice = onlyStencil ? 1u : 0u;
    const uint32 subResourceIndex = D3D12CalcSubresource(
        newSubResource.baseMipLevel,
        newSubResource.baseArrayLayer,
        planeSlice,
        newSubResource.numLevels,
        newSubResource.numLevels);

    BOOL succeeded = debugCommandList->AssertResourceState(
        image.GetResource(),
        subResourceIndex,
        ToDX12ResourceStates(expectedState));

    if (succeeded == TRUE)
    {
        return;
    }

    // collect states to create debug message.
    Array<Pair<ImageSubResource, ResourceState>> debugResourceStates;
    debugResourceStates.Reserve(newSubResource.numLayers * newSubResource.numLevels);

    for (uint16 layerIndex = newSubResource.baseArrayLayer; layerIndex < newSubResource.baseArrayLayer + newSubResource.numLayers; ++layerIndex)
    {
        for (uint8 mipIndex = newSubResource.baseMipLevel; mipIndex < newSubResource.baseMipLevel + newSubResource.numLevels; ++mipIndex)
        {
            ImageSubResource currSubResource {};
            currSubResource.baseArrayLayer = layerIndex;
            currSubResource.numLayers = 1;
            currSubResource.baseMipLevel = mipIndex;
            currSubResource.numLevels = 1;

            debugResourceStates.EmplaceBack(currSubResource, image.GetSubResourceState(currSubResource));
        }
    }

    String message;
    message = HYP_FORMAT("Resource state assertion failed! Expected Image (Name={}) to have state: {}\n",
                         image.GetDebugName(),
                         EnumToString(expectedState));

    for (auto& it : debugResourceStates)
    {
        message += HYP_FORMAT("    Layer: {}, Mip: {}, State: {}\n",
                              it.first.baseArrayLayer,
                              it.first.baseMipLevel,
                              EnumToString(it.second));
    }

    Assert(false, "{}", message);
}

ID3D12DebugCommandList* DX12CommandBuffer::GetDebugCommandList() const
{
    if (!m_debugCommandList)
    {
        ComPtr<ID3D12DebugCommandList> debugCommandList;
        if (SUCCEEDED(m_commandList->QueryInterface(IID_PPV_ARGS(&debugCommandList))))
        {
            m_debugCommandList = debugCommandList.Detach();
        }
    }

    return m_debugCommandList;
}

#endif

} // namespace Hyperion
