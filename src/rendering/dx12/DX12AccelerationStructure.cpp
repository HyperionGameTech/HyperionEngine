/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12AccelerationStructure.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12AccelerationStructure.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12GpuBlas

DX12GpuBlas::DX12GpuBlas(
    const DX12GpuBufferRef& packedVerticesBuffer,
    const DX12GpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
    : GpuBlasBase()
{
    m_material = material;
    // @TODO
}

DX12GpuBlas::~DX12GpuBlas()
{
}

bool DX12GpuBlas::IsCreated() const
{
    return false;
}

RendererResult DX12GpuBlas::Create()
{
    // @TODO
    return {};
}

void DX12GpuBlas::SetTransform(const Mat4f& transform)
{
    // @TODO
}

#ifdef HYP_DEBUG_MODE
void DX12GpuBlas::SetDebugName(Name name)
{
    GpuBlasBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuBlas

#pragma region DX12GpuTlas

DX12GpuTlas::DX12GpuTlas()
    : GpuTlasBase()
{
}

DX12GpuTlas::~DX12GpuTlas()
{
}

bool DX12GpuTlas::IsCreated() const
{
    return false;
}

void DX12GpuTlas::AddGpuBlas(const GpuBlasRef& blas)
{
    // @TODO
}

void DX12GpuTlas::RemoveGpuBlas(const GpuBlasRef& blas)
{
    // @TODO
}

bool DX12GpuTlas::HasGpuBlas(const GpuBlasRef& blas)
{
    return false;
}

RendererResult DX12GpuTlas::Create()
{
    // @TODO
    return {};
}

RendererResult DX12GpuTlas::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    // @TODO
    return {};
}

#ifdef HYP_DEBUG_MODE
void DX12GpuTlas::SetDebugName(Name name)
{
    GpuTlasBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuTlas

} // namespace Hyperion
