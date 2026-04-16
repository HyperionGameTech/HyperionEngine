/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/AccelerationStructure.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/math/Mat4f.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GpuBlas final : public GpuBlasBase
{
    HYP_OBJECT_BODY(DX12GpuBlas);

public:
    DX12GpuBlas(
        const DX12GpuBufferRef& packedVerticesBuffer,
        const DX12GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<MaterialInstance>& material,
        const Mat4f& transform);
    ~DX12GpuBlas() override;

    bool IsCreated() const override;

    RendererResult Create() override;

    void SetTransform(const Mat4f& transform) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif
};

HYP_CLASS(NoScriptBindings)
class DX12GpuTlas final : public GpuTlasBase
{
    HYP_OBJECT_BODY(DX12GpuTlas);

public:
    DX12GpuTlas();
    ~DX12GpuTlas() override;

    bool IsCreated() const override;

    void AddGpuBlas(const GpuBlasRef& blas) override;
    void RemoveGpuBlas(const GpuBlasRef& blas) override;
    bool HasGpuBlas(const GpuBlasRef& blas) override;

    RendererResult Create() override;

    RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif
};

} // namespace Hyperion
