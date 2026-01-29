/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderPipeline.hpp>

#include <rendering/dx12/DX12Shared.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GraphicsPipeline final : public GraphicsPipelineBase
{
    HYP_OBJECT_BODY(DX12GraphicsPipeline);

public:
    DX12GraphicsPipeline();
    explicit DX12GraphicsPipeline(const DX12ShaderRef& shader);
    ~DX12GraphicsPipeline() override;

    HYP_FORCE_INLINE ID3D12RootSignature* GetRootSignature() const
    {
        return m_rootSignature.Get();
    }

    HYP_FORCE_INLINE ID3D12PipelineState* GetPipelineState() const
    {
        return m_pipelineState.Get();
    }

    bool IsCreated() const override;

    RendererResult Create() override;

    void Bind(CommandBuffer* cmd) override;
    void Bind(CommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

    void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult Rebuild() override;

    RendererResult BuildRootSignature();

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
};

} // namespace Hyperion
