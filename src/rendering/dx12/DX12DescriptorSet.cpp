/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12DescriptorSet.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12DescriptorSet

DX12DescriptorSet::DX12DescriptorSet(const DescriptorSetLayout& layout)
    : DescriptorSetBase(layout)
{
}

DX12DescriptorSet::~DX12DescriptorSet()
{
}

bool DX12DescriptorSet::IsCreated() const
{
    return false;
}

RendererResult DX12DescriptorSet::Create()
{
    // @TODO
    return {};
}

void DX12DescriptorSet::UpdateDirtyState(bool* outIsDirty)
{
}

void DX12DescriptorSet::Update(bool force)
{
}

void DX12DescriptorSet::Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, uint32 bindIndex) const
{
}

void DX12DescriptorSet::Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
}

void DX12DescriptorSet::Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, uint32 bindIndex) const
{
}

void DX12DescriptorSet::Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
}

void DX12DescriptorSet::Bind(CommandBuffer* commandBuffer, const RaytracingPipeline* pipeline, uint32 bindIndex) const
{
}

void DX12DescriptorSet::Bind(CommandBuffer* commandBuffer, const RaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
}

DescriptorSetRef DX12DescriptorSet::Clone() const
{
    return DescriptorSetRef();
}

#ifdef HYP_DEBUG_MODE
void DX12DescriptorSet::SetDebugName(Name name)
{
    DescriptorSetBase::SetDebugName(name);
}
#endif

#pragma endregion DX12DescriptorSet

} // namespace Hyperion
