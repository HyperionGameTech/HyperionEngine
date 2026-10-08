/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/CommandBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

#include <Core/Math/Rect.hpp>

namespace Hyperion {

class WebGPUGraphicsPipeline;
class WebGPUComputePipeline;
class WebGPUFramebuffer;
class WebGPUGpuBuffer;

struct WebGPUBoundBindGroup
{
    static constexpr uint32 MaxDynamicOffsets = DescriptorSetOffsetMap::MaxOffsets;

    WGPUBindGroup bindGroup = nullptr;
    WGPUBindGroupLayout layout = nullptr;
    uint32 numDynamicOffsets = 0;
    uint32 dynamicOffsets[MaxDynamicOffsets] = {};
    bool isDirty = false;
};

// WebGPU only accepts state inside a pass, and the engine binds pipelines and sets before the pass begins,
// so binds are recorded here and applied when a draw or dispatch is issued.
HYP_CLASS(NoScriptBindings)
class WebGPUCommandBuffer final : public CommandBufferBase
{
    HYP_OBJECT_BODY(WebGPUCommandBuffer);

public:
    static constexpr uint32 MaxBindGroups = 4;

    WebGPUCommandBuffer();
    WebGPUCommandBuffer(const WebGPUCommandBuffer& other) = delete;
    WebGPUCommandBuffer& operator=(const WebGPUCommandBuffer& other) = delete;
    ~WebGPUCommandBuffer() override;

    HYP_FORCE_INLINE WGPUCommandEncoder GetEncoder() const
    {
        return m_encoder;
    }

    bool IsCreated() const override;
    RendererResult Create() override;

    bool IsRecording() const override
    {
        return m_encoder != nullptr;
    }

    void Begin() override;
    void End() override;

    void BindVertexBuffer(const WebGPUGpuBuffer* buffer) override;
    void BindIndexBuffer(const WebGPUGpuBuffer* buffer, GpuElemType elemType = GpuElemType::UnsignedInt) override;

    void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const override;

    void DrawIndexedIndirect(
        const WebGPUGpuBuffer* buffer,
        uint32 bufferOffset) const override;

    HYP_FORCE_INLINE Span<const WGPUCommandBuffer> GetFinishedCommandBuffers() const
    {
        return m_finishedCommandBuffers.ToSpan();
    }

    void ReleaseFinishedCommandBuffers();

    void OnSubmitted();

    void BeginRenderPass(WebGPUFramebuffer* framebuffer);
    void EndRenderPass(WebGPUFramebuffer* framebuffer);

    void EndActivePass();

    HYP_FORCE_INLINE bool IsRenderPassOpen() const
    {
        return m_renderPass != nullptr;
    }

    void SetGraphicsPipeline(WebGPUGraphicsPipeline* pipeline, const Viewport& viewport, uint32 stencilReference);
    void SetComputePipeline(WebGPUComputePipeline* pipeline);

    void SetBindGroup(
        bool isCompute,
        uint32 bindIndex,
        WGPUBindGroup bindGroup,
        WGPUBindGroupLayout layout,
        const uint32* dynamicOffsets,
        uint32 numDynamicOffsets);

    void DrawRectClear(WGPURenderPipeline pipeline, const Rect<uint32>& rect);

    WGPUComputePassEncoder PrepareDispatch();

    void AddPendingReadback(WebGPUGpuBuffer* buffer);

    HYP_FORCE_INLINE const Array<WebGPUGpuBuffer*, WebGPUAllocator>& GetPendingReadbacks() const
    {
        return m_pendingReadbacks;
    }

    // Dynamic offsets have to be multiples of 256 in WebGPU and the engine uses element index * element size, so a range
    // that does not line up is copied into an aligned slot of a shared buffer. Returns the offset of that slot.
    uint32 RealignDynamicRange(const WebGPUGpuBuffer* buffer, uint32 offset, uint32 size);

private:
    bool PrepareDraw() const;
    void ResetBoundState();

    // A copy cannot be encoded inside a pass, and realigned ranges are only known as draws are recorded. So each pass
    // starts a new pair of encoders: one that runs just before it and collects those copies, and one holding the pass.
    void BeginPassSegment();
    void FinishSegment();

    WGPUCommandEncoder m_encoder;
    WGPUCommandEncoder m_copyEncoder;
    Array<WGPUCommandBuffer, WebGPUAllocator> m_finishedCommandBuffers;

    mutable WGPURenderPassEncoder m_renderPass;
    WGPUComputePassEncoder m_computePass;

    WebGPUFramebuffer* m_framebuffer;

    WebGPUGraphicsPipeline* m_graphicsPipeline;
    WebGPUComputePipeline* m_computePipeline;

    mutable WGPURenderPipeline m_appliedRenderPipeline;
    WGPUComputePipeline m_appliedComputePipeline;

    mutable WebGPUBoundBindGroup m_graphicsBindGroups[MaxBindGroups];
    WebGPUBoundBindGroup m_computeBindGroups[MaxBindGroups];

    Viewport m_viewport;
    uint32 m_stencilReference;
    mutable bool m_isViewportDirty;

    const WebGPUGpuBuffer* m_vertexBuffer;
    const WebGPUGpuBuffer* m_indexBuffer;
    GpuElemType m_indexElemType;
    mutable bool m_isVertexBufferDirty;
    mutable bool m_isIndexBufferDirty;

    Array<WebGPUGpuBuffer*, WebGPUAllocator> m_pendingReadbacks;

    struct RealignedRange
    {
        const WebGPUGpuBuffer* buffer;
        uint32 offset;
        uint32 size;
        uint32 realignedOffset;
    };

    Array<RealignedRange, WebGPUAllocator> m_realignedRanges;
};

} // namespace Hyperion
