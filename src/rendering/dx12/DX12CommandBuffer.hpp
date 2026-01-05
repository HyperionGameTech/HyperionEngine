/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/CommandBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12CommandBuffer final : public CommandBufferBase
{
    HYP_OBJECT_BODY(DX12CommandBuffer);

public:
    DX12CommandBuffer() = default;
    virtual ~DX12CommandBuffer() override = default;

    virtual RendererResult Create() override;
    virtual RendererResult Begin() override;
    virtual RendererResult End() override;

    virtual void BindVertexBuffer(const DX12GpuBuffer* buffer) override;
    virtual void BindIndexBuffer(const DX12GpuBuffer* buffer, GpuElemType elemType = GET_UNSIGNED_INT) override;

    virtual void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const override;

    virtual void DrawIndexedIndirect(
        const DX12GpuBuffer* buffer,
        uint32 bufferOffset) const override;

private:
};

} // namespace Hyperion
