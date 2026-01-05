/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderPipeline.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GraphicsPipeline final : public GraphicsPipelineBase
{
    HYP_OBJECT_BODY(DX12GraphicsPipeline);

public:
    DX12GraphicsPipeline();
    explicit DX12GraphicsPipeline(const DX12ShaderRef& shader);
    virtual ~DX12GraphicsPipeline() override;

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

    virtual void Bind(CommandBuffer* cmd) override;
    virtual void Bind(CommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

    virtual void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    virtual RendererResult Rebuild() override;
};

} // namespace Hyperion
