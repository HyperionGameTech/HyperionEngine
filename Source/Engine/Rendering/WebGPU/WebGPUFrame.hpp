/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/Frame.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class WebGPUFrame final : public FrameBase
{
    HYP_OBJECT_BODY(WebGPUFrame);

public:
    WebGPUFrame();
    explicit WebGPUFrame(uint32 frameIndex);
    ~WebGPUFrame() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    void OnFrameStart() override;

    void WriteCommandBuffer(CommandBuffer* commandBuffer) override;

    void ResetTransientStates();
};

} // namespace Hyperion
