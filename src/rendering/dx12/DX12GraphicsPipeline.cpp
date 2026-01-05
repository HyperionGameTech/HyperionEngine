/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12GraphicsPipeline.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12GraphicsPipeline

DX12GraphicsPipeline::DX12GraphicsPipeline()
    : GraphicsPipelineBase()
{
}

DX12GraphicsPipeline::DX12GraphicsPipeline(const DX12ShaderRef& shader)
    : GraphicsPipelineBase()
{
    m_shader = shader;
}

DX12GraphicsPipeline::~DX12GraphicsPipeline()
{
}

bool DX12GraphicsPipeline::IsCreated() const
{
    return false;
}

RendererResult DX12GraphicsPipeline::Create()
{
    return Rebuild();
}

void DX12GraphicsPipeline::Bind(CommandBuffer* cmd)
{
    // @TODO
}

void DX12GraphicsPipeline::Bind(CommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent)
{
    // @TODO
}

void DX12GraphicsPipeline::SetPushConstants(const void* data, SizeType size)
{
    // @TODO
}

#ifdef HYP_DEBUG_MODE
void DX12GraphicsPipeline::SetDebugName(Name name)
{
    GraphicsPipelineBase::SetDebugName(name);
}
#endif

RendererResult DX12GraphicsPipeline::Rebuild()
{
    // @TODO
    HYPERION_RETURN_OK;
}

#pragma endregion DX12GraphicsPipeline

} // namespace Hyperion
