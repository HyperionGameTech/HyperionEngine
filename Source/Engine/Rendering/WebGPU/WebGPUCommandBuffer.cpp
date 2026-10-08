/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPUFramebuffer.hpp>
#include <Rendering/WebGPU/WebGPUGraphicsPipeline.hpp>
#include <Rendering/WebGPU/WebGPUComputePipeline.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <WebGPUCommandBuffer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

WebGPUCommandBuffer::WebGPUCommandBuffer()
    : m_encoder(nullptr),
      m_copyEncoder(nullptr),
      m_renderPass(nullptr),
      m_computePass(nullptr),
      m_framebuffer(nullptr),
      m_graphicsPipeline(nullptr),
      m_computePipeline(nullptr),
      m_appliedRenderPipeline(nullptr),
      m_appliedComputePipeline(nullptr),
      m_stencilReference(0),
      m_isViewportDirty(false),
      m_vertexBuffer(nullptr),
      m_indexBuffer(nullptr),
      m_indexElemType(GpuElemType::UnsignedInt),
      m_isVertexBufferDirty(false),
      m_isIndexBufferDirty(false)
{
}

WebGPUCommandBuffer::~WebGPUCommandBuffer()
{
    if (m_renderPass != nullptr)
    {
        wgpuRenderPassEncoderRelease(m_renderPass);
    }

    if (m_computePass != nullptr)
    {
        wgpuComputePassEncoderRelease(m_computePass);
    }

    if (m_copyEncoder != nullptr)
    {
        wgpuCommandEncoderRelease(m_copyEncoder);
    }

    if (m_encoder != nullptr)
    {
        wgpuCommandEncoderRelease(m_encoder);
    }

    ReleaseFinishedCommandBuffers();
}

bool WebGPUCommandBuffer::IsCreated() const
{
    return true;
}

RendererResult WebGPUCommandBuffer::Create()
{
    return {};
}

void WebGPUCommandBuffer::ResetBoundState()
{
    m_framebuffer = nullptr;
    m_graphicsPipeline = nullptr;
    m_computePipeline = nullptr;
    m_appliedRenderPipeline = nullptr;
    m_appliedComputePipeline = nullptr;

    for (uint32 bindIndex = 0; bindIndex < MaxBindGroups; bindIndex++)
    {
        m_graphicsBindGroups[bindIndex] = WebGPUBoundBindGroup {};
        m_computeBindGroups[bindIndex] = WebGPUBoundBindGroup {};
    }

    m_viewport = Viewport {};
    m_stencilReference = 0;
    m_isViewportDirty = false;

    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_isVertexBufferDirty = false;
    m_isIndexBufferDirty = false;
}

void WebGPUCommandBuffer::Begin()
{
    AssertDebug(m_encoder == nullptr, "Command buffer is already recording!");

    ReleaseFinishedCommandBuffers();

    m_realignedRanges.Clear();

    m_encoder = wgpuDeviceCreateCommandEncoder(RI.GetDevice(), nullptr);
    Assert(m_encoder != nullptr);

    ResetBoundState();
}

void WebGPUCommandBuffer::FinishSegment()
{
    if (m_copyEncoder != nullptr)
    {
        m_finishedCommandBuffers.PushBack(wgpuCommandEncoderFinish(m_copyEncoder, nullptr));

        wgpuCommandEncoderRelease(m_copyEncoder);
        m_copyEncoder = nullptr;
    }

    m_finishedCommandBuffers.PushBack(wgpuCommandEncoderFinish(m_encoder, nullptr));

    wgpuCommandEncoderRelease(m_encoder);
    m_encoder = nullptr;
}

void WebGPUCommandBuffer::BeginPassSegment()
{
    FinishSegment();

    m_copyEncoder = wgpuDeviceCreateCommandEncoder(RI.GetDevice(), nullptr);
    m_encoder = wgpuDeviceCreateCommandEncoder(RI.GetDevice(), nullptr);

    m_realignedRanges.Clear();
}

void WebGPUCommandBuffer::ReleaseFinishedCommandBuffers()
{
    for (WGPUCommandBuffer finishedCommandBuffer : m_finishedCommandBuffers)
    {
        wgpuCommandBufferRelease(finishedCommandBuffer);
    }

    m_finishedCommandBuffers.Clear();
}

void WebGPUCommandBuffer::End()
{
    AssertDebug(m_encoder != nullptr, "Command buffer is not recording!");

    EndActivePass();

    FinishSegment();

    ResetBoundState();
}

void WebGPUCommandBuffer::OnSubmitted()
{
    for (WebGPUGpuBuffer* buffer : m_pendingReadbacks)
    {
        buffer->BeginReadback();
    }

    m_pendingReadbacks.Clear();
}

uint32 WebGPUCommandBuffer::RealignDynamicRange(const WebGPUGpuBuffer* buffer, uint32 offset, uint32 size)
{
    for (size_t rangeIndex = m_realignedRanges.Size(); rangeIndex != 0; rangeIndex--)
    {
        const RealignedRange& realignedRange = m_realignedRanges[rangeIndex - 1];

        if (realignedRange.buffer == buffer && realignedRange.offset == offset && realignedRange.size == size)
        {
            return realignedRange.realignedOffset;
        }
    }

    const uint32 copySize = ByteUtil::AlignAs(size, 4u);

    uint32 realignedOffset = 0;

    if (m_encoder == nullptr || uint64(offset) + copySize > buffer->GetAllocatedSize() || !RI.AllocateRealignedRange(copySize, realignedOffset))
    {
        return 0;
    }

    const bool isPassOpen = m_renderPass != nullptr || m_computePass != nullptr;

    wgpuCommandEncoderCopyBufferToBuffer(isPassOpen ? m_copyEncoder : m_encoder, buffer->GetWGPUBuffer(), offset, RI.GetRealignBuffer(), realignedOffset, copySize);

    m_realignedRanges.PushBack(RealignedRange { buffer, offset, size, realignedOffset });

    return realignedOffset;
}

void WebGPUCommandBuffer::AddPendingReadback(WebGPUGpuBuffer* buffer)
{
    if (!m_pendingReadbacks.Contains(buffer))
    {
        m_pendingReadbacks.PushBack(buffer);
    }
}

void WebGPUCommandBuffer::BeginRenderPass(WebGPUFramebuffer* framebuffer)
{
    EndActivePass();

    m_framebuffer = framebuffer;
}

void WebGPUCommandBuffer::EndRenderPass(WebGPUFramebuffer* framebuffer)
{
    if (m_framebuffer != framebuffer)
    {
        return;
    }

    if (m_renderPass == nullptr && framebuffer->HasPendingClears() && m_encoder != nullptr)
    {
        BeginPassSegment();

        m_renderPass = framebuffer->BeginPass(m_encoder);
    }

    EndActivePass();

    m_framebuffer = nullptr;
}

void WebGPUCommandBuffer::EndActivePass()
{
    if (m_renderPass != nullptr)
    {
        wgpuRenderPassEncoderEnd(m_renderPass);
        wgpuRenderPassEncoderRelease(m_renderPass);
        m_renderPass = nullptr;

        m_appliedRenderPipeline = nullptr;
    }

    if (m_computePass != nullptr)
    {
        wgpuComputePassEncoderEnd(m_computePass);
        wgpuComputePassEncoderRelease(m_computePass);
        m_computePass = nullptr;

        m_appliedComputePipeline = nullptr;
    }
}

void WebGPUCommandBuffer::SetGraphicsPipeline(WebGPUGraphicsPipeline* pipeline, const Viewport& viewport, uint32 stencilReference)
{
    if (m_graphicsPipeline != pipeline)
    {
        m_graphicsPipeline = pipeline;

        for (WebGPUBoundBindGroup& boundBindGroup : m_graphicsBindGroups)
        {
            boundBindGroup = WebGPUBoundBindGroup {};
        }
    }

    m_viewport = viewport;
    m_stencilReference = stencilReference;
    m_isViewportDirty = true;
}

void WebGPUCommandBuffer::SetComputePipeline(WebGPUComputePipeline* pipeline)
{
    if (m_computePipeline != pipeline)
    {
        m_computePipeline = pipeline;

        for (WebGPUBoundBindGroup& boundBindGroup : m_computeBindGroups)
        {
            boundBindGroup = WebGPUBoundBindGroup {};
        }
    }
}

void WebGPUCommandBuffer::SetBindGroup(
    bool isCompute,
    uint32 bindIndex,
    WGPUBindGroup bindGroup,
    WGPUBindGroupLayout layout,
    const uint32* dynamicOffsets,
    uint32 numDynamicOffsets)
{
    Assert(bindIndex < MaxBindGroups, "Bind group index {} out of range", bindIndex);
    Assert(numDynamicOffsets <= WebGPUBoundBindGroup::MaxDynamicOffsets);

    WebGPUBoundBindGroup& boundBindGroup = isCompute ? m_computeBindGroups[bindIndex] : m_graphicsBindGroups[bindIndex];

    boundBindGroup.bindGroup = bindGroup;
    boundBindGroup.layout = layout;
    boundBindGroup.numDynamicOffsets = numDynamicOffsets;

    for (uint32 offsetIndex = 0; offsetIndex < numDynamicOffsets; offsetIndex++)
    {
        boundBindGroup.dynamicOffsets[offsetIndex] = dynamicOffsets[offsetIndex];
    }

    boundBindGroup.isDirty = true;
}

void WebGPUCommandBuffer::BindVertexBuffer(const WebGPUGpuBuffer* buffer)
{
    AssertDebug(buffer != nullptr);

    m_vertexBuffer = buffer;
    m_isVertexBufferDirty = true;
}

void WebGPUCommandBuffer::BindIndexBuffer(const WebGPUGpuBuffer* buffer, GpuElemType elemType)
{
    AssertDebug(buffer != nullptr);

    m_indexBuffer = buffer;
    m_indexElemType = elemType;
    m_isIndexBufferDirty = true;
}

bool WebGPUCommandBuffer::PrepareDraw() const
{
    if (m_graphicsPipeline == nullptr || m_framebuffer == nullptr || m_encoder == nullptr)
    {
        return false;
    }

    const uint32 numBindGroups = m_graphicsPipeline->GetNumBindGroups();

    WGPUBindGroupLayout layouts[MaxBindGroups] = {};

    for (uint32 bindIndex = 0; bindIndex < numBindGroups; bindIndex++)
    {
        if (!m_graphicsPipeline->UsesBindGroup(bindIndex))
        {
            layouts[bindIndex] = RI.GetEmptyBindGroupLayout();

            continue;
        }

        if (m_graphicsBindGroups[bindIndex].bindGroup == nullptr)
        {
            return false;
        }

        layouts[bindIndex] = m_graphicsBindGroups[bindIndex].layout;
    }

    WGPURenderPipeline renderPipeline = m_graphicsPipeline->GetOrCreateVariant(layouts, numBindGroups);

    if (renderPipeline == nullptr)
    {
        return false;
    }

    const bool passOpened = m_renderPass == nullptr;

    if (passOpened)
    {
        if (m_computePass != nullptr)
        {
            const_cast<WebGPUCommandBuffer*>(this)->EndActivePass();
        }

        const_cast<WebGPUCommandBuffer*>(this)->BeginPassSegment();

        m_renderPass = m_framebuffer->BeginPass(m_encoder);

        if (m_renderPass == nullptr)
        {
            return false;
        }
    }

    const bool pipelineChanged = passOpened || m_appliedRenderPipeline != renderPipeline;

    if (pipelineChanged)
    {
        wgpuRenderPassEncoderSetPipeline(m_renderPass, renderPipeline);
        m_appliedRenderPipeline = renderPipeline;
    }

    for (uint32 bindIndex = 0; bindIndex < numBindGroups; bindIndex++)
    {
        WebGPUBoundBindGroup& boundBindGroup = m_graphicsBindGroups[bindIndex];

        if (!m_graphicsPipeline->UsesBindGroup(bindIndex))
        {
            if (pipelineChanged)
            {
                wgpuRenderPassEncoderSetBindGroup(m_renderPass, bindIndex, RI.GetEmptyBindGroup(), 0, nullptr);
            }

            continue;
        }

        if (pipelineChanged || boundBindGroup.isDirty)
        {
            wgpuRenderPassEncoderSetBindGroup(m_renderPass, bindIndex, boundBindGroup.bindGroup, boundBindGroup.numDynamicOffsets, boundBindGroup.dynamicOffsets);
            boundBindGroup.isDirty = false;
        }
    }

    if (passOpened || m_isViewportDirty)
    {
        const Vec2u framebufferExtent = m_framebuffer->GetAttachmentExtent();

        Vec2i position = m_viewport.position;
        Vec2u extent = m_viewport.extent;

        if (extent == Vec2u::Zero())
        {
            position = Vec2i::Zero();
            extent = m_framebuffer->GetExtent();
        }

        position.x = MathUtil::Clamp(position.x, 0, int32(framebufferExtent.x));
        position.y = MathUtil::Clamp(position.y, 0, int32(framebufferExtent.y));
        extent.x = MathUtil::Min(extent.x, framebufferExtent.x - uint32(position.x));
        extent.y = MathUtil::Min(extent.y, framebufferExtent.y - uint32(position.y));

        wgpuRenderPassEncoderSetViewport(m_renderPass, float(position.x), float(position.y), float(extent.x), float(extent.y), 0.0f, 1.0f);
        wgpuRenderPassEncoderSetScissorRect(m_renderPass, uint32(position.x), uint32(position.y), extent.x, extent.y);
        wgpuRenderPassEncoderSetStencilReference(m_renderPass, m_stencilReference);

        m_isViewportDirty = false;
    }

    if ((passOpened || m_isVertexBufferDirty) && m_vertexBuffer != nullptr)
    {
        wgpuRenderPassEncoderSetVertexBuffer(m_renderPass, 0, m_vertexBuffer->GetWGPUBuffer(), 0, m_vertexBuffer->GetAllocatedSize());
        m_isVertexBufferDirty = false;
    }

    if ((passOpened || m_isIndexBufferDirty) && m_indexBuffer != nullptr)
    {
        wgpuRenderPassEncoderSetIndexBuffer(m_renderPass, m_indexBuffer->GetWGPUBuffer(), ToWGPUIndexFormat(m_indexElemType), 0, m_indexBuffer->GetAllocatedSize());
        m_isIndexBufferDirty = false;
    }

    return m_vertexBuffer != nullptr && m_indexBuffer != nullptr;
}

void WebGPUCommandBuffer::DrawIndexed(uint32 numIndices, uint32 numInstances, uint32 instanceIndex) const
{
    if (!PrepareDraw())
    {
        return;
    }

    wgpuRenderPassEncoderDrawIndexed(m_renderPass, numIndices, numInstances, 0, 0, instanceIndex);
}

void WebGPUCommandBuffer::DrawIndexedIndirect(const WebGPUGpuBuffer* buffer, uint32 bufferOffset) const
{
    AssertDebug(buffer != nullptr);
    AssertDebug(bufferOffset % 4 == 0);

    if (!PrepareDraw())
    {
        return;
    }

    wgpuRenderPassEncoderDrawIndexedIndirect(m_renderPass, buffer->GetWGPUBuffer(), bufferOffset);
}

void WebGPUCommandBuffer::DrawRectClear(WGPURenderPipeline pipeline, const Rect<uint32>& rect)
{
    if (m_framebuffer == nullptr || m_encoder == nullptr)
    {
        return;
    }

    if (m_computePass != nullptr)
    {
        EndActivePass();
    }

    if (m_renderPass == nullptr)
    {
        BeginPassSegment();

        m_renderPass = m_framebuffer->BeginPass(m_encoder);

        if (m_renderPass == nullptr)
        {
            return;
        }
    }

    const Vec2u framebufferExtent = m_framebuffer->GetAttachmentExtent();

    const uint32 x0 = MathUtil::Min(rect.x0, framebufferExtent.x);
    const uint32 y0 = MathUtil::Min(rect.y0, framebufferExtent.y);
    const uint32 x1 = MathUtil::Min(rect.x1, framebufferExtent.x);
    const uint32 y1 = MathUtil::Min(rect.y1, framebufferExtent.y);

    if (x1 <= x0 || y1 <= y0)
    {
        return;
    }

    wgpuRenderPassEncoderSetPipeline(m_renderPass, pipeline);
    wgpuRenderPassEncoderSetViewport(m_renderPass, float(x0), float(y0), float(x1 - x0), float(y1 - y0), 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(m_renderPass, x0, y0, x1 - x0, y1 - y0);
    wgpuRenderPassEncoderDraw(m_renderPass, 3, 1, 0, 0);

    m_appliedRenderPipeline = nullptr;
    m_isViewportDirty = true;
}

WGPUComputePassEncoder WebGPUCommandBuffer::PrepareDispatch()
{
    if (m_computePipeline == nullptr || m_encoder == nullptr)
    {
        return nullptr;
    }

    const uint32 numBindGroups = m_computePipeline->GetNumBindGroups();

    WGPUBindGroupLayout layouts[MaxBindGroups] = {};

    for (uint32 bindIndex = 0; bindIndex < numBindGroups; bindIndex++)
    {
        if (!m_computePipeline->UsesBindGroup(bindIndex))
        {
            layouts[bindIndex] = RI.GetEmptyBindGroupLayout();

            continue;
        }

        if (m_computeBindGroups[bindIndex].bindGroup == nullptr)
        {
            return nullptr;
        }

        layouts[bindIndex] = m_computeBindGroups[bindIndex].layout;
    }

    WGPUComputePipeline computePipeline = m_computePipeline->GetOrCreateVariant(layouts, numBindGroups);

    if (computePipeline == nullptr)
    {
        return nullptr;
    }

    if (m_renderPass != nullptr)
    {
        EndActivePass();
    }

    const bool passOpened = m_computePass == nullptr;

    if (passOpened)
    {
        BeginPassSegment();

        m_computePass = wgpuCommandEncoderBeginComputePass(m_encoder, nullptr);
    }

    const bool pipelineChanged = passOpened || m_appliedComputePipeline != computePipeline;

    if (pipelineChanged)
    {
        wgpuComputePassEncoderSetPipeline(m_computePass, computePipeline);
        m_appliedComputePipeline = computePipeline;
    }

    for (uint32 bindIndex = 0; bindIndex < numBindGroups; bindIndex++)
    {
        WebGPUBoundBindGroup& boundBindGroup = m_computeBindGroups[bindIndex];

        if (!m_computePipeline->UsesBindGroup(bindIndex))
        {
            if (pipelineChanged)
            {
                wgpuComputePassEncoderSetBindGroup(m_computePass, bindIndex, RI.GetEmptyBindGroup(), 0, nullptr);
            }

            continue;
        }

        if (pipelineChanged || boundBindGroup.isDirty)
        {
            wgpuComputePassEncoderSetBindGroup(m_computePass, bindIndex, boundBindGroup.bindGroup, boundBindGroup.numDynamicOffsets, boundBindGroup.dynamicOffsets);
            boundBindGroup.isDirty = false;
        }
    }

    return m_computePass;
}

} // namespace Hyperion
