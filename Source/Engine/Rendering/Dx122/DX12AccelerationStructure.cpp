/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/dx12/DX12AccelerationStructure.hpp>
#include <Rendering/dx12/DX12RenderInterface.hpp>
#include <Rendering/dx12/DX12CommandBuffer.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/Bindless.hpp>

#include <Rendering/util/DeletionQueue.hpp>

#include <Core/math/MathUtil.hpp>
#include <Core/utilities/Range.hpp>

#include <DX12AccelerationStructure.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12AccelerationGeometry

DX12AccelerationGeometry::DX12AccelerationGeometry(
    const DX12GpuBufferRef& packedVerticesBuffer,
    const DX12GpuBufferRef& packedIndicesBuffer,
    uint32 numVertices, uint32 numIndices,
    const Handle<MaterialInstance>& material)
    : m_isCreated(false),
      m_packedVerticesBuffer(packedVerticesBuffer),
      m_packedIndicesBuffer(packedIndicesBuffer),
      m_numVertices(numVertices),
      m_numIndices(numIndices),
      m_material(material)
{
}

DX12AccelerationGeometry::~DX12AccelerationGeometry()
{
    EnqueueDeletion(std::move(m_packedVerticesBuffer));
    EnqueueDeletion(std::move(m_packedIndicesBuffer));

    m_isCreated = false;
}

bool DX12AccelerationGeometry::IsCreated() const
{
    return m_isCreated;
}

RendererResult DX12AccelerationGeometry::Create()
{
    if (m_isCreated)
    {
        return {};
    }

    if (!m_packedVerticesBuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not valid");
    }

    if (!m_packedVerticesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not created");
    }

    if (!m_packedIndicesBuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not valid");
    }

    if (!m_packedIndicesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not created");
    }

    m_isCreated = true;

    return RendererResult();
}

#pragma endregion DX12AccelerationGeometry

#pragma region DX12AccelerationStructureBase

DX12AccelerationStructureBase::DX12AccelerationStructureBase(const Mat4f& transform)
    : m_transform(transform),
      m_flags(ACCELERATION_STRUCTURE_FLAGS_NONE)
{
}

DX12AccelerationStructureBase::~DX12AccelerationStructureBase()
{
    m_geometries.Clear();
    m_buffer.Reset();
    m_scratchBuffer.Reset();
}

void DX12AccelerationStructureBase::RemoveGeometry(uint32 index)
{
    const auto it = m_geometries.Begin() + index;

    if (it >= m_geometries.End())
    {
        return;
    }

    EnqueueDeletion(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

void DX12AccelerationStructureBase::RemoveGeometry(const DX12AccelerationGeometryRef& geometry)
{
    auto it = m_geometries.Find(geometry);
    if (it != m_geometries.End())
    {
        m_geometries.Erase(it);
        SetNeedsRebuildFlag();
    }
}

#if HYP_DEBUG_MODE
void DX12AccelerationStructureBase::SetDebugName(Name name)
{
    m_debugName = name;

    if (!name.IsValid())
    {
        return;
    }

    if (m_buffer)
    {
        m_buffer->SetDebugName(name);
    }

    if (m_scratchBuffer)
    {
        m_scratchBuffer->SetDebugName(NAME_FMT("{}_scratch", *name));
    }
}
#endif

#pragma endregion DX12AccelerationStructureBase

#pragma region DX12GpuBlas

DX12GpuBlas::DX12GpuBlas(
    const DX12GpuBufferRef& packedVerticesBuffer,
    const DX12GpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<MaterialInstance>& material,
    const Mat4f& transform)
    : GpuBlasBase(),
      DX12AccelerationStructureBase(transform),
      m_packedVerticesBuffer(packedVerticesBuffer),
      m_packedIndicesBuffer(packedIndicesBuffer)
{
    m_material = material;

    m_geometries.PushBack(MakeHandle<DX12AccelerationGeometry>(
        m_packedVerticesBuffer,
        m_packedIndicesBuffer,
        numVertices,
        numIndices,
        m_material));
}

DX12GpuBlas::~DX12GpuBlas()
{
}

bool DX12GpuBlas::IsCreated() const
{
    return m_buffer && m_buffer->IsCreated();
}

RendererResult DX12GpuBlas::Create()
{
    if (IsCreated())
    {
        return {};
    }

    if (!m_packedVerticesBuffer.IsValid() || !m_packedVerticesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not valid");
    }

    if (!m_packedIndicesBuffer.IsValid() || !m_packedIndicesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not valid");
    }

    RTUpdateStateFlags flags;
    return Rebuild(flags);
}

void DX12GpuBlas::SetTransform(const Mat4f& transform)
{
    DX12AccelerationStructureBase::SetTransform(transform);
}

void DX12GpuBlas::SetMaterialBinding(uint32 materialBinding)
{
    if (m_materialBinding == materialBinding)
    {
        return;
    }

    m_materialBinding = materialBinding;

    if (!IsCreated())
    {
        return;
    }

    m_flags |= ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE;
}

RendererResult DX12GpuBlas::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MATERIAL;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }

    return {};
}

RendererResult DX12GpuBlas::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (!m_packedVerticesBuffer.IsValid() || !m_packedVerticesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not valid");
    }

    if (!m_packedIndicesBuffer.IsValid() || !m_packedIndicesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not valid");
    }

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometryDesc.Triangles.VertexBuffer.StartAddress = m_packedVerticesBuffer->GetResource()->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(PackedVertex);
    geometryDesc.Triangles.VertexCount = m_packedVerticesBuffer->Size() / sizeof(PackedVertex);
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.IndexBuffer = m_packedIndicesBuffer->GetResource()->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount = m_packedIndicesBuffer->Size() / sizeof(uint32);
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
        | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    inputs.NumDescs = 1;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &geometryDesc;

    ID3D12Device* device = RI.GetDevice();

    ComPtr<ID3D12Device5> device5;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5))))
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support DXR");
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo {};
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

    const size_t accelerationStructureSize = MathUtil::NextMultiple(prebuildInfo.ResultDataMaxSizeInBytes, 256ull);
    const size_t scratchBufferSize = MathUtil::NextMultiple(prebuildInfo.ScratchDataSizeInBytes, 256ull);

    if (!m_buffer || m_buffer->Size() < accelerationStructureSize)
    {
        m_buffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureBuffer, accelerationStructureSize, 256);
        CheckResultOrReturn(m_buffer->Create());
    }

    if (!m_scratchBuffer || m_scratchBuffer->Size() < scratchBufferSize)
    {
        m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::ScratchBuffer, scratchBufferSize);
        CheckResultOrReturn(m_scratchBuffer->Create());
    }

    // Use the current command buffer if it is recording, otherwise acquire a transient one
    // (BLAS builds may be triggered outside the frame's Begin/End recording window).
    DX12CommandBuffer* pCmdBuffer = RI.GetCurrentCommandBuffer();
    bool ownCmdBuffer = false;

    if (!pCmdBuffer->IsRecording())
    {
        pCmdBuffer = &RI.GetTransientCommandBuffer();
        ownCmdBuffer = true;
    }

    ComPtr<ID3D12GraphicsCommandList4> commandList4;
    if (FAILED(pCmdBuffer->GetCommandList()->QueryInterface(IID_PPV_ARGS(&commandList4))))
    {
        if (ownCmdBuffer)
        {
            RI.SubmitTransientCommandBuffer(*pCmdBuffer);
        }
        return HYP_MAKE_ERROR(RendererError, "Command list does not support DXR");
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {};
    buildDesc.Inputs = inputs;
    buildDesc.SourceAccelerationStructureData = 0;
    buildDesc.DestAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = m_scratchBuffer->GetResource()->GetGPUVirtualAddress();

    commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    if (ownCmdBuffer)
    {
        RI.SubmitTransientCommandBuffer(*pCmdBuffer);
    }

    m_flags = ACCELERATION_STRUCTURE_FLAGS_NONE;
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

    return {};
}

#ifdef HYP_DEBUG_MODE
void DX12GpuBlas::SetDebugName(Name name)
{
    GpuBlasBase::SetDebugName(name);
    DX12AccelerationStructureBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuBlas

#pragma region DX12GpuTlas

DX12GpuTlas::DX12GpuTlas()
    : GpuTlasBase(),
      DX12AccelerationStructureBase()
{
}

DX12GpuTlas::~DX12GpuTlas()
{
    m_instancesBuffer.Reset();
    m_scratchBuffer.Reset();

    for (DX12GpuBlas* blas : m_blases)
    {
        blas->Release();
    }

    m_blases.Clear();

    if (RI.bindlessStorage != nullptr)
    {
        for (auto& it : m_keyToBlasAndStorageId)
        {
            const uint32 storageId = it.second.second;

            RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2);
            RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 1);

            RI.bindlessStorage->ReleaseId(BindlessStorage_Buffers, storageId);
        }
    }
}

bool DX12GpuTlas::IsCreated() const
{
    return m_buffer && m_buffer->IsCreated();
}

void DX12GpuTlas::AddGpuBlas(uint64 key, GpuBlas* blas)
{
    if (!blas)
    {
        return;
    }

    DX12GpuBlas* dx12Blas = static_cast<DX12GpuBlas*>(blas);

    if (m_keyToBlasAndStorageId.Contains(key))
    {
        return;
    }

    Assert(dx12Blas->IsCreated());
    Assert(!dx12Blas->GetGeometries().Empty());

    for (const DX12AccelerationGeometryRef& geometry : dx12Blas->GetGeometries())
    {
        Assert(geometry != nullptr);
        Assert(geometry->GetPackedVerticesBuffer() != nullptr);
        Assert(geometry->GetPackedIndicesBuffer() != nullptr);
    }

    auto& entry = m_keyToBlasAndStorageId[key];
    entry.first = dx12Blas;
    entry.second = ~0u;

    dx12Blas->AddRef();

    m_blases.PushBack(dx12Blas);
    m_keys.PushBack(key);

    SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

void DX12GpuTlas::RemoveGpuBlas(uint64 key)
{
    auto it = m_keyToBlasAndStorageId.Find(key);

    if (it != m_keyToBlasAndStorageId.End())
    {
        DX12GpuBlas* blas = it->second.first;
        uint32 storageId = it->second.second;

        RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2);
        RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 1);

        RI.bindlessStorage->ReleaseId(BindlessStorage_Buffers, storageId);

        auto blasesIt = m_blases.Find(blas);
        Assert(blasesIt != m_blases.End());

        blas->Release();

        auto keysIt = m_keys.Begin() + std::distance(m_blases.Begin(), blasesIt);
        m_keys.Erase(keysIt);

        m_blases.Erase(blasesIt);

        m_keyToBlasAndStorageId.Erase(it);

        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }
}

bool DX12GpuTlas::HasGpuBlas(uint64 key)
{
    return m_keyToBlasAndStorageId.Contains(key);
}

RendererResult DX12GpuTlas::Create()
{
    if (IsCreated())
    {
        return {};
    }

    if (m_blases.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Top level acceleration structure must have at least one GpuBlas");
    }

    for (DX12GpuBlas* blas : m_blases)
    {
        Assert(blas != nullptr);

        CheckResultOrReturn(blas->Create());
    }

    CheckResultOrReturn(BuildInstancesBuffer());

    RTUpdateStateFlags updateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs {};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
            | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        inputs.NumDescs = uint32(m_blases.Size());
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.InstanceDescs = m_instancesBuffer->GetResource()->GetGPUVirtualAddress();

        ID3D12Device* device = RI.GetDevice();

        ComPtr<ID3D12Device5> device5;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5))))
        {
            return HYP_MAKE_ERROR(RendererError, "Device does not support DXR");
        }

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo {};
        device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

        const size_t accelerationStructureSize = MathUtil::NextMultiple(prebuildInfo.ResultDataMaxSizeInBytes, 256ull);
        const size_t scratchBufferSize = MathUtil::NextMultiple(prebuildInfo.ScratchDataSizeInBytes, 256ull);

        if (!m_buffer || m_buffer->Size() < accelerationStructureSize)
        {
            m_buffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureBuffer, accelerationStructureSize, 256);
            CheckResultOrReturn(m_buffer->Create());
        }

        if (!m_scratchBuffer || m_scratchBuffer->Size() < scratchBufferSize)
        {
            m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::ScratchBuffer, scratchBufferSize);
            CheckResultOrReturn(m_scratchBuffer->Create());
        }

        // Use the current command buffer if it is recording, otherwise acquire a transient one.
        DX12CommandBuffer* pCmdBuffer = RI.GetCurrentCommandBuffer();
        bool ownCmdBuffer = false;

        if (!pCmdBuffer->IsRecording())
        {
            pCmdBuffer = &RI.GetTransientCommandBuffer();
            ownCmdBuffer = true;
        }

        ComPtr<ID3D12GraphicsCommandList4> commandList4;
        if (FAILED(pCmdBuffer->GetCommandList()->QueryInterface(IID_PPV_ARGS(&commandList4))))
        {
            if (ownCmdBuffer)
            {
                RI.SubmitTransientCommandBuffer(*pCmdBuffer);
            }
            return HYP_MAKE_ERROR(RendererError, "Command list does not support DXR");
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {};
        buildDesc.Inputs = inputs;
        buildDesc.SourceAccelerationStructureData = 0;
        buildDesc.DestAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
        buildDesc.ScratchAccelerationStructureData = m_scratchBuffer->GetResource()->GetGPUVirtualAddress();

        commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

        if (ownCmdBuffer)
        {
            RI.SubmitTransientCommandBuffer(*pCmdBuffer);
        }
    }

    updateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

    CheckResultOrReturn(BuildMeshDescriptionsBuffer());
    updateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    Assert(updateStateFlags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    return RendererResult();
}

RendererResult DX12GpuTlas::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }

    Range<uint32> dirtyRange = Range<uint32>::Invalid();

    for (uint32 i = 0; i < uint32(m_blases.Size()); i++)
    {
        DX12GpuBlas* blas = m_blases[i];
        Assert(blas != nullptr);

        RTUpdateStateFlags blasUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;
        CheckResultOrReturn(blas->UpdateStructure(blasUpdateStateFlags));

        if (blasUpdateStateFlags)
        {
            dirtyRange |= Range { i, i + 1 };
        }
    }

    if (dirtyRange)
    {
        CheckResultOrReturn(BuildInstancesBuffer(dirtyRange.GetStart(), dirtyRange.GetEnd()));
        CheckResultOrReturn(BuildMeshDescriptionsBuffer(dirtyRange.GetStart(), dirtyRange.GetEnd()));

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs {};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        // PERFORM_UPDATE must be set for in-place updates; SourceAccelerationStructureData
        // must be non-null and 256-byte aligned when this flag is present.
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
            | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
            | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        inputs.NumDescs = uint32(m_blases.Size());
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.InstanceDescs = m_instancesBuffer->GetResource()->GetGPUVirtualAddress();

        ID3D12Device* device = RI.GetDevice();

        ComPtr<ID3D12Device5> device5;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5))))
        {
            return HYP_MAKE_ERROR(RendererError, "Device does not support DXR");
        }

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo {};
        device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

        const size_t accelerationStructureSize = MathUtil::NextMultiple(prebuildInfo.ResultDataMaxSizeInBytes, 256ull);
        const size_t scratchBufferSize = MathUtil::NextMultiple(prebuildInfo.UpdateScratchDataSizeInBytes, 256ull);

        if (!m_buffer || m_buffer->Size() < accelerationStructureSize)
        {
            m_buffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureBuffer, accelerationStructureSize, 256);
            CheckResultOrReturn(m_buffer->Create());
        }

        if (!m_scratchBuffer || m_scratchBuffer->Size() < scratchBufferSize)
        {
            m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::ScratchBuffer, scratchBufferSize);
            CheckResultOrReturn(m_scratchBuffer->Create());
        }

        // Use the current command buffer if it is recording, otherwise acquire a transient one.
        DX12CommandBuffer* pCmdBuffer = RI.GetCurrentCommandBuffer();
        bool ownCmdBuffer = false;

        if (!pCmdBuffer->IsRecording())
        {
            pCmdBuffer = &RI.GetTransientCommandBuffer();
            ownCmdBuffer = true;
        }

        ComPtr<ID3D12GraphicsCommandList4> commandList4;
        if (FAILED(pCmdBuffer->GetCommandList()->QueryInterface(IID_PPV_ARGS(&commandList4))))
        {
            if (ownCmdBuffer)
            {
                RI.SubmitTransientCommandBuffer(*pCmdBuffer);
            }
            return HYP_MAKE_ERROR(RendererError, "Command list does not support DXR");
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {};
        buildDesc.Inputs = inputs;
        buildDesc.SourceAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
        buildDesc.DestAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
        buildDesc.ScratchAccelerationStructureData = m_scratchBuffer->GetResource()->GetGPUVirtualAddress();

        commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

        if (ownCmdBuffer)
        {
            RI.SubmitTransientCommandBuffer(*pCmdBuffer);
        }

        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS | RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;
    }

    return RendererResult();
}

RendererResult DX12GpuTlas::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_blases.Empty())
    {
        return {};
    }

    for (DX12GpuBlas* blas : m_blases)
    {
        Assert(blas != nullptr);
        Assert(blas->IsCreated());
        Assert(!blas->GetGeometries().Empty());

        for (const DX12AccelerationGeometryRef& geometry : blas->GetGeometries())
        {
            Assert(geometry != nullptr);
            Assert(geometry->GetPackedVerticesBuffer() != nullptr);
            Assert(geometry->GetPackedIndicesBuffer() != nullptr);
        }
    }

    CheckResultOrReturn(BuildInstancesBuffer());
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    // Full rebuild — do NOT include PERFORM_UPDATE; SourceAccelerationStructureData must be 0.
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
        | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    inputs.NumDescs = uint32(m_blases.Size());
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.InstanceDescs = m_instancesBuffer->GetResource()->GetGPUVirtualAddress();

    ID3D12Device* device = RI.GetDevice();

    ComPtr<ID3D12Device5> device5;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5))))
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support DXR");
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo {};
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

    const size_t accelerationStructureSize = MathUtil::NextMultiple(prebuildInfo.ResultDataMaxSizeInBytes, 256ull);
    const size_t scratchBufferSize = MathUtil::NextMultiple(prebuildInfo.ScratchDataSizeInBytes, 256ull);

    if (!m_buffer || m_buffer->Size() < accelerationStructureSize)
    {
        m_buffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureBuffer, accelerationStructureSize, 256);
        CheckResultOrReturn(m_buffer->Create());
    }

    if (!m_scratchBuffer || m_scratchBuffer->Size() < scratchBufferSize)
    {
        m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::ScratchBuffer, scratchBufferSize);
        CheckResultOrReturn(m_scratchBuffer->Create());
    }

    // Use the current command buffer if it is recording, otherwise acquire a transient one.
    DX12CommandBuffer* pCmdBuffer = RI.GetCurrentCommandBuffer();
    bool ownCmdBuffer = false;

    if (!pCmdBuffer->IsRecording())
    {
        pCmdBuffer = &RI.GetTransientCommandBuffer();
        ownCmdBuffer = true;
    }

    ComPtr<ID3D12GraphicsCommandList4> commandList4;
    if (FAILED(pCmdBuffer->GetCommandList()->QueryInterface(IID_PPV_ARGS(&commandList4))))
    {
        if (ownCmdBuffer)
        {
            RI.SubmitTransientCommandBuffer(*pCmdBuffer);
        }
        return HYP_MAKE_ERROR(RendererError, "Command list does not support DXR");
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {};
    buildDesc.Inputs = inputs;
    buildDesc.SourceAccelerationStructureData = 0;
    buildDesc.DestAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = m_scratchBuffer->GetResource()->GetGPUVirtualAddress();

    commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    if (ownCmdBuffer)
    {
        RI.SubmitTransientCommandBuffer(*pCmdBuffer);
    }

    m_flags = ACCELERATION_STRUCTURE_FLAGS_NONE;
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

    CheckResultOrReturn(BuildMeshDescriptionsBuffer());
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return {};
}

RendererResult DX12GpuTlas::BuildInstancesBuffer()
{
    return BuildInstancesBuffer(0, uint32(m_blases.Size()));
}

RendererResult DX12GpuTlas::BuildInstancesBuffer(uint32 first, uint32 last)
{
    if (last <= first)
    {
        return RendererResult();
    }

    last = MathUtil::Min(uint32(m_blases.Size()), last);

    constexpr size_t minInstancesBufferSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    const size_t instancesBufferSize = MathUtil::Max(minInstancesBufferSize, m_blases.Size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

    bool instancesBufferRecreated = false;

    if (m_instancesBuffer && m_instancesBuffer->Size() < instancesBufferSize)
    {
        EnqueueDeletion(std::move(m_instancesBuffer));
    }

    if (!m_instancesBuffer)
    {
        m_instancesBuffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureInstanceBuffer, instancesBufferSize);
        m_instancesBuffer->SetIsCpuAccessible(true);
        CheckResultOrReturn(m_instancesBuffer->Create());

        instancesBufferRecreated = true;
    }

    if (instancesBufferRecreated)
    {
        m_instancesBuffer->Memset(m_instancesBuffer->Size(), 0x0);

        first = 0;
        last = uint32(m_blases.Size());
    }

    if (m_blases.Empty() || last <= first)
    {
        return RendererResult();
    }

    Array<D3D12_RAYTRACING_INSTANCE_DESC, RenderAllocator> instances;
    instances.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        DX12GpuBlas* blas = m_blases[i];
        Assert(blas != nullptr);

        D3D12_RAYTRACING_INSTANCE_DESC& desc = instances[i - first];
        desc.InstanceID = i;
        desc.InstanceContributionToHitGroupIndex = i;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

        const Mat4f transform = blas->GetTransform();
        std::memcpy(desc.Transform, transform.values, sizeof(desc.Transform));

        desc.AccelerationStructure = blas->GetBuffer()->GetResource()->GetGPUVirtualAddress();
    }

    Assert(m_instancesBuffer != nullptr);
    Assert(m_instancesBuffer->Size() >= (first + instances.Size()) * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

    m_instancesBuffer->Copy(
        first * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        instances.Size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        instances.Data());

    m_instancesBuffer->Flush(
        first * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        instances.Size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

    return RendererResult();
}

RendererResult DX12GpuTlas::BuildMeshDescriptionsBuffer()
{
    return BuildMeshDescriptionsBuffer(0u, uint32(m_blases.Size()));
}

RendererResult DX12GpuTlas::BuildMeshDescriptionsBuffer(uint32 first, uint32 last)
{
    if (last <= first)
    {
        return RendererResult();
    }

    last = MathUtil::Min(uint32(m_blases.Size()), last);

    constexpr size_t minMeshDescriptionsBufferSize = sizeof(MeshDescription);
    const size_t meshDescriptionsBufferSize = MathUtil::Max(minMeshDescriptionsBufferSize, sizeof(MeshDescription) * m_blases.Size());

    bool meshDescriptionsBufferRecreated = false;

    if (m_meshDescriptionsBuffer && m_meshDescriptionsBuffer->Size() < meshDescriptionsBufferSize)
    {
        EnqueueDeletion(std::move(m_meshDescriptionsBuffer));
    }

    if (!m_meshDescriptionsBuffer)
    {
        m_meshDescriptionsBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, meshDescriptionsBufferSize);

#if HYP_DEBUG_MODE
        m_meshDescriptionsBuffer->SetDebugName(NAME("ASMeshDescriptionsBuffer"));
#endif

        m_meshDescriptionsBuffer->SetIsCpuAccessible(true);
        CheckResultOrReturn(m_meshDescriptionsBuffer->Create());

        meshDescriptionsBufferRecreated = true;
    }

    if (meshDescriptionsBufferRecreated)
    {
        m_meshDescriptionsBuffer->Memset(m_meshDescriptionsBuffer->Size(), 0x0);

        first = 0;
        last = uint32(m_blases.Size());
    }

    if (m_blases.Empty() || last <= first)
    {
        return RendererResult();
    }

    Array<MeshDescription, RenderAllocator> meshDescriptions;
    meshDescriptions.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        DX12GpuBlas* blas = m_blases[i];
        uint64 key = m_keys[i];

        MeshDescription& meshDescription = meshDescriptions[i - first];
        meshDescription = {};

        Assert(blas->GetGeometries().Any(), "No geometries added to GpuBlas node %u!", i);

        Assert(blas->GetGeometries()[0]->GetPackedVerticesBuffer() && blas->GetGeometries()[0]->GetPackedVerticesBuffer()->IsCreated());
        Assert(blas->GetGeometries()[0]->GetPackedIndicesBuffer() && blas->GetGeometries()[0]->GetPackedIndicesBuffer()->IsCreated());

        uint32 storageId = ~0u;

        {
            storageId = m_keyToBlasAndStorageId[key].second;

            if (storageId == ~0u)
            {
                storageId = RI.bindlessStorage->AllocateId(BindlessStorage_Buffers);
                AssertDebug(!(storageId & StorageIdDirtyBit));
                storageId |= StorageIdDirtyBit;
            }

            if (storageId & StorageIdDirtyBit)
            {
                storageId &= ~StorageIdDirtyBit;

                m_keyToBlasAndStorageId[key].second = storageId;

                RI.bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2, blas->GetGeometries()[0]->GetPackedVerticesBuffer());
                RI.bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2 + 1, blas->GetGeometries()[0]->GetPackedIndicesBuffer());
            }
        }

        meshDescription.bindlessIndex = storageId;
        meshDescription.materialIndex = blas->GetMaterialBinding();
        meshDescription.numIndices = blas->GetGeometries()[0]->NumIndices();
        meshDescription.numVertices = blas->GetGeometries()[0]->NumVertices();
    }

    Assert(m_meshDescriptionsBuffer != nullptr);
    Assert(m_meshDescriptionsBuffer->Size() >= (first + meshDescriptions.Size()) * sizeof(MeshDescription));

    m_meshDescriptionsBuffer->Copy(
        first * sizeof(MeshDescription),
        meshDescriptions.Size() * sizeof(MeshDescription),
        meshDescriptions.Data());

    m_meshDescriptionsBuffer->Flush(
        first * sizeof(MeshDescription),
        meshDescriptions.Size() * sizeof(MeshDescription));

    return RendererResult();
}

#ifdef HYP_DEBUG_MODE
void DX12GpuTlas::SetDebugName(Name name)
{
    GpuTlasBase::SetDebugName(name);
    DX12AccelerationStructureBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuTlas

} // namespace Hyperion
