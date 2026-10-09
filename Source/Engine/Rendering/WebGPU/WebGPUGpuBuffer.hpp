/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GpuBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

#include <Core/Threading/Mutex.hpp>

#include <atomic>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class WebGPUGpuBuffer final : public GpuBufferBase
{
    HYP_OBJECT_BODY(WebGPUGpuBuffer);

public:
    WebGPUGpuBuffer(GpuBufferType type, size_t size, size_t alignment = 0);
    WebGPUGpuBuffer(const WebGPUGpuBuffer& other) = delete;
    WebGPUGpuBuffer& operator=(const WebGPUGpuBuffer& other) = delete;
    ~WebGPUGpuBuffer() override;

    HYP_FORCE_INLINE WGPUBuffer GetWGPUBuffer() const
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE uint64 GetAllocatedSize() const
    {
        return m_allocatedSize;
    }

    HYP_FORCE_INLINE const ubyte* GetShadowCopy() const
    {
        return m_shadowCopy;
    }

    RendererResult Create() override;
    bool IsCreated() const override;
    bool IsCpuAccessible() const override;

    void InsertBarrier(WebGPUCommandBuffer* commandBuffer, ResourceState newState) const override;
    void InsertBarrier(WebGPUCommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const override;

    void CopyFrom(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuBuffer* srcBuffer,
        uint32 count) override;

    void CopyFrom(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuBuffer* srcBuffer,
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

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

    void UploadDirtyRange(WGPUQueue queue);

    void BeginReadback();

    HYP_FORCE_INLINE bool IsReadbackPending() const
    {
        return m_isReadbackPending.load(std::memory_order_acquire);
    }

private:
    bool HasShadowCopy() const;
    void MarkDirty(size_t offset, size_t count) const;
    void WaitForReadback() const;
    void DestroyBuffer();

    static void OnReadbackMapped(WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* userdata2);

    WGPUBuffer m_buffer;
    uint64 m_allocatedSize;

    // CPU-side copy handed out by Map(). WebGPU buffers cannot stay mapped while in use by the GPU.
    mutable ubyte* m_shadowCopy;

    mutable Mutex m_dirtyMutex;
    mutable size_t m_dirtyBegin;
    mutable size_t m_dirtyEnd;
    mutable bool m_isRegisteredDirty;

    std::atomic<bool> m_isReadbackPending;
};

} // namespace Hyperion
