/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/dx12/DX12Shared.hpp>

#include <D3D12MemAlloc.h>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GpuBuffer final : public GpuBufferBase
{
    HYP_OBJECT_BODY(DX12GpuBuffer);

public:
    DX12GpuBuffer(GpuBufferType type, size_t size, size_t alignment = 0);
    ~DX12GpuBuffer() override;

    HYP_FORCE_INLINE ID3D12Resource* GetResource() const
    {
        return m_resource.Get();
    }

    HYP_FORCE_INLINE D3D12MA::Allocation* GetAllocation() const
    {
        return m_allocation.Get();
    }
    
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
        size_t minimumSize,
        bool* outSizeChanged = nullptr) override;
        
    RendererResult EnsureCapacity(
        size_t minimumSize,
        size_t alignment,
        bool* outSizeChanged = nullptr) override;

    void Memset(size_t count, ubyte value) override;

    void Copy(size_t count, const void* ptr) override;
    void Copy(size_t offset, size_t count, const void* ptr) override;

    void Read(size_t count, void* outPtr) const override;
    void Read(size_t offset, size_t count, void* outPtr) const override;

    void* Map() const override;
    void Unmap() const override;

    void Flush(size_t offset, size_t count) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
};

} // namespace Hyperion
