/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/DescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12DescriptorSet final : public DescriptorSetBase
{
    HYP_OBJECT_BODY(DX12DescriptorSet);

public:
    explicit DX12DescriptorSet(const DescriptorSetLayout& layout);
    ~DX12DescriptorSet() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    void Update(bool force = false) override;

    void Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const RaytracingPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const RaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    DescriptorSetRef Clone() const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif
};

HYP_CLASS(NoScriptBindings)
class DX12DescriptorTable final : public DescriptorTableBase
{
    HYP_OBJECT_BODY(DX12DescriptorTable);

public:
    explicit DX12DescriptorTable(const DescriptorTableDeclaration* decl)
        : DescriptorTableBase(decl)
    {
    }

    ~DX12DescriptorTable() override = default;
};

} // namespace Hyperion
