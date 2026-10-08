/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/AccelerationStructure.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

namespace Hyperion {

// WebGPU has no ray tracing, these only exist so the shared renderer code compiles
HYP_CLASS(NoScriptBindings)
class WebGPUTopLevelAS final : public TopLevelASBase
{
    HYP_OBJECT_BODY(WebGPUTopLevelAS);

public:
    explicit WebGPUTopLevelAS(const ASResourceCallbacks& callbacks);
    ~WebGPUTopLevelAS() override;

    bool IsCreated() const override;

    void AddBLAS(uint64 key, WebGPUBottomLevelAS* blas) override;
    bool RemoveBLAS(uint64 key) override;
    bool ContainsBLAS(uint64 key) override;

    RendererResult Create() override;
    RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) override;
};

HYP_CLASS(NoScriptBindings)
class WebGPUBottomLevelAS final : public BottomLevelASBase
{
    HYP_OBJECT_BODY(WebGPUBottomLevelAS);

public:
    WebGPUBottomLevelAS();
    ~WebGPUBottomLevelAS() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    void SetTransform(const Mat4f& transform) override;
};

} // namespace Hyperion
