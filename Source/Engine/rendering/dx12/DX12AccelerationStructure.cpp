/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12AccelerationStructure.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/Shared.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/math/MathUtil.hpp>

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
        m_buffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureBuffer, accelerationStructureSize);
        CheckResultOrReturn(m_buffer->Create());
    }

    if (!m_scratchBuffer || m_scratchBuffer->Size() < scratchBufferSize)
    {
        m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::ScratchBuffer, scratchBufferSize);
        CheckResultOrReturn(m_scratchBuffer->Create());
    }

    ID3D12GraphicsCommandList* commandList = RI.GetCurrentCommandBuffer()->GetCommandList();

    ComPtr<ID3D12GraphicsCommandList4> commandList4;
    if (FAILED(commandList->QueryInterface(IID_PPV_ARGS(&commandList4))))
    {
        return HYP_MAKE_ERROR(RendererError, "Command list does not support DXR");
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {};
    buildDesc.Inputs = inputs;
    buildDesc.SourceAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = m_scratchBuffer->GetResource()->GetGPUVirtualAddress();

    commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

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

    uint32 storageId = uint32(m_blases.Size());

    m_blases.PushBack(dx12Blas);
    m_keys.PushBack(key);
    m_keyToBlasAndStorageId.Set(key, { dx12Blas, storageId });

    dx12Blas->AddRef();

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
    RTUpdateStateFlags flags;
    return Rebuild(flags);
}

RendererResult DX12GpuTlas::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }

    return {};
}

RendererResult DX12GpuTlas::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_blases.Empty())
    {
        return {};
    }

    const size_t instancesBufferSize = m_blases.Size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

    if (!m_instancesBuffer || m_instancesBuffer->Size() < instancesBufferSize)
    {
        m_instancesBuffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureInstanceBuffer, instancesBufferSize);
        CheckResultOrReturn(m_instancesBuffer->Create());
    }

    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = static_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(m_instancesBuffer->Map());

    for (size_t i = 0; i < m_blases.Size(); i++)
    {
        DX12GpuBlas* blas = m_blases[i];

        instanceDescs[i].InstanceID = i;
        instanceDescs[i].InstanceContributionToHitGroupIndex = i;
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

        Mat4f transform = blas->GetTransform();
        std::memcpy(instanceDescs[i].Transform, transform.values, sizeof(instanceDescs[i].Transform));

        instanceDescs[i].AccelerationStructure = blas->GetBuffer()->GetResource()->GetGPUVirtualAddress();
        instanceDescs[i].InstanceID = i;
    }

    m_instancesBuffer->Unmap();

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
        m_buffer = RI.MakeGpuBuffer(GpuBufferType::AccelerationStructureBuffer, accelerationStructureSize);
        CheckResultOrReturn(m_buffer->Create());
    }

    if (!m_scratchBuffer || m_scratchBuffer->Size() < scratchBufferSize)
    {
        m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::ScratchBuffer, scratchBufferSize);
        CheckResultOrReturn(m_scratchBuffer->Create());
    }

    ID3D12GraphicsCommandList* commandList = RI.GetCurrentCommandBuffer()->GetCommandList();

    ComPtr<ID3D12GraphicsCommandList4> commandList4;
    if (FAILED(commandList->QueryInterface(IID_PPV_ARGS(&commandList4))))
    {
        return HYP_MAKE_ERROR(RendererError, "Command list does not support DXR");
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {};
    buildDesc.Inputs = inputs;
    buildDesc.SourceAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = m_buffer->GetResource()->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = m_scratchBuffer->GetResource()->GetGPUVirtualAddress();

    commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    m_flags = ACCELERATION_STRUCTURE_FLAGS_NONE;
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

    return {};
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
