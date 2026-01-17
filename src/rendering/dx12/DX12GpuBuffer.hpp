/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GpuBuffer final : public GpuBufferBase
{
    HYP_OBJECT_BODY(DX12GpuBuffer);

public:
    DX12GpuBuffer(GpuBufferType type, SizeType size, SizeType alignment = 0);
    ~DX12GpuBuffer() override;
    
    RendererResult Create() override;

    bool IsCreated() const override;
    bool IsCpuAccessible() const override;

    void InsertBarrier(DX12CommandBuffer* commandBuffer, ResourceState newState) const override;
    void InsertBarrier(DX12CommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const override;
    
    void CopyFrom(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuBuffer* srcBuffer,
        uint32 count) override;

    void CopyFrom(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuBuffer* srcBuffer,
        uint32 srcOffset, uint32 dstOffset,
        uint32 count) override;

    RendererResult EnsureCapacity(
        SizeType minimumSize,
        bool* outSizeChanged = nullptr) override;
        
    RendererResult EnsureCapacity(
        SizeType minimumSize,
        SizeType alignment,
        bool* outSizeChanged = nullptr) override;

    void Memset(SizeType count, ubyte value) override;

    void Copy(SizeType count, const void* ptr) override;
    void Copy(SizeType offset, SizeType count, const void* ptr) override;

    void Read(SizeType count, void* outPtr) const override;
    void Read(SizeType offset, SizeType count, void* outPtr) const override;

    void* Map() const override;
    void Unmap() const override;

    void Flush(SizeType offset, SizeType count) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    // @TODO
};

} // namespace Hyperion
