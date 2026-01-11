/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/GpuBuffer.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/CommandBuffer.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/Shared.hpp>

// Uncomment to enable trace collection for commands.
// #define HYP_RHI_COMMAND_STACK_TRACE

#ifdef HYP_RHI_COMMAND_STACK_TRACE
#include <core/debug/StackDump.hpp>
#endif

namespace Hyperion {

class CmdBase;
class View;
class RenderGroup;

class CmdBase
{
public:
#ifdef HYP_RHI_COMMAND_STACK_TRACE
    RawStackTrace trace;
#else
    CmdBase() = default;
#endif
    
    static void PrepareStatic(CmdBase*, Frame*)
    {
    }
};

class BindVertexBuffer final : public CmdBase
{
public:
    BindVertexBuffer(GpuBuffer* buffer)
        : m_buffer(buffer)
    {
        HYP_GFX_ASSERT(buffer && buffer->IsCreated());
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        BindVertexBuffer* cmdCasted = static_cast<BindVertexBuffer*>(cmd);

        commandBuffer->BindVertexBuffer(cmdCasted->m_buffer);

        static_assert(std::is_trivially_destructible_v<BindVertexBuffer>);
        // cmdCasted->~BindVertexBuffer();
    }

private:
    GpuBuffer* m_buffer;
};

class BindIndexBuffer final : public CmdBase
{
public:
    BindIndexBuffer(GpuBuffer* buffer)
        : m_buffer(buffer)
    {
        HYP_GFX_ASSERT(buffer && buffer->IsCreated());
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        BindIndexBuffer* cmdCasted = static_cast<BindIndexBuffer*>(cmd);

        commandBuffer->BindIndexBuffer(cmdCasted->m_buffer);

        static_assert(std::is_trivially_destructible_v<BindIndexBuffer>);
        // cmdCasted->~BindIndexBuffer();
    }

private:
    GpuBuffer* m_buffer;
};

class DrawIndexed final : public CmdBase
{
public:
    DrawIndexed(uint32 numIndices, uint32 numInstances = 1, uint32 instanceIndex = 0)
        : m_numIndices(numIndices),
          m_numInstances(numInstances),
          m_instanceIndex(instanceIndex)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        DrawIndexed* cmdCasted = static_cast<DrawIndexed*>(cmd);

        commandBuffer->DrawIndexed(cmdCasted->m_numIndices, cmdCasted->m_numInstances, cmdCasted->m_instanceIndex);

        static_assert(std::is_trivially_destructible_v<DrawIndexed>);
        // cmdCasted->~DrawIndexed();
    }

private:
    uint32 m_numIndices;
    uint32 m_numInstances;
    uint32 m_instanceIndex;
};

class DrawIndexedIndirect final : public CmdBase
{
public:
    DrawIndexedIndirect(GpuBuffer* buffer, uint32 bufferOffset)
        : m_buffer(buffer),
          m_bufferOffset(bufferOffset)
    {
        HYP_GFX_ASSERT(buffer != nullptr && buffer->IsCreated());
#if HYP_VULKAN
        HYP_GFX_ASSERT(bufferOffset + /*sizeof(VkDrawIndexedIndirectCommand)*/ 20 <= buffer->Size());
#endif
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        DrawIndexedIndirect* cmdCasted = static_cast<DrawIndexedIndirect*>(cmd);

        commandBuffer->DrawIndexedIndirect(cmdCasted->m_buffer, cmdCasted->m_bufferOffset);

        static_assert(std::is_trivially_destructible_v<DrawIndexedIndirect>);
        // cmdCasted->~DrawIndexedIndirect();
    }

private:
    GpuBuffer* m_buffer;
    uint32 m_bufferOffset;
};

class DrawQuad final : public CmdBase
{
public:
    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);
};

class BeginFramebuffer final : public CmdBase
{
public:
    BeginFramebuffer(Framebuffer* framebuffer);
    static void PrepareStatic(CmdBase* cmd, Frame* frame);

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        BeginFramebuffer* cmdCasted = static_cast<BeginFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->BeginCapture(commandBuffer);

        static_assert(std::is_trivially_destructible_v<BeginFramebuffer>);
        // cmdCasted->~BeginFramebuffer();
    }

private:
    Framebuffer* m_framebuffer;
};

class EndFramebuffer final : public CmdBase
{
public:
    EndFramebuffer(Framebuffer* framebuffer);
    static void PrepareStatic(CmdBase* cmd, Frame* frame);

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        EndFramebuffer* cmdCasted = static_cast<EndFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->EndCapture(commandBuffer);

        static_assert(std::is_trivially_destructible_v<EndFramebuffer>);
        // cmdCasted->~EndFramebuffer();
    }

private:
    Framebuffer* m_framebuffer;
};

class ClearFramebuffer final : public CmdBase
{
public:
    HYP_API ClearFramebuffer(Framebuffer* framebuffer)
        : m_framebuffer(framebuffer)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        ClearFramebuffer* cmdCasted = static_cast<ClearFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->Clear(commandBuffer);

        static_assert(std::is_trivially_destructible_v<ClearFramebuffer>);
        // cmdCasted->~ClearFramebuffer();
    }

private:
    Framebuffer* m_framebuffer;
};

class BindGraphicsPipeline final : public CmdBase
{
public:
    BindGraphicsPipeline(GraphicsPipeline* pipeline, const Viewport& viewport)
        : m_pipeline(pipeline),
          m_viewport(viewport)
    {
    }

    BindGraphicsPipeline(GraphicsPipeline* pipeline, Vec2i viewportOffset, Vec2u viewportExtent)
        : m_pipeline(pipeline),
          m_viewport(Viewport { viewportExtent, viewportOffset })
    {
    }

    BindGraphicsPipeline(GraphicsPipeline* pipeline)
        : m_pipeline(pipeline),
            m_viewport()
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    GraphicsPipeline* m_pipeline;
    Viewport m_viewport;
};

class BindComputePipeline final : public CmdBase
{
public:
    BindComputePipeline(ComputePipeline* pipeline)
        : m_pipeline(pipeline)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    ComputePipeline* m_pipeline;
};

class BindRaytracingPipeline final : public CmdBase
{
public:
    BindRaytracingPipeline(RaytracingPipeline* pipeline)
        : m_pipeline(pipeline)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    RaytracingPipeline* m_pipeline;
};

class BindDescriptorSet final : public CmdBase
{
public:
    BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);
    BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);
    BindDescriptorSet(DescriptorSet* descriptorSet, RaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, RaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);

    static void PrepareStatic(CmdBase* cmd, Frame* frame);
    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    DescriptorSet* m_descriptorSet;
    union
    {
        GraphicsPipeline* m_graphicsPipeline;
        ComputePipeline* m_computePipeline;
        RaytracingPipeline* m_raytracingPipeline;
    };
    DescriptorSetOffsetMap m_offsets;
    uint32 m_bindIndex;
    uint8 m_pipelineType : 2; // 0 = Graphics, 1 = Compute, 2 = Raytracing
};

class BindDescriptorTable final : public CmdBase
{
public:
    BindDescriptorTable(DescriptorTable* descriptorTable, GraphicsPipeline* graphicsPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex);
    BindDescriptorTable(DescriptorTable* descriptorTable, ComputePipeline* computePipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex);
    BindDescriptorTable(DescriptorTable* descriptorTable, RaytracingPipeline* raytracingPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex);

    static void PrepareStatic(CmdBase* cmd, Frame* frame);
    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    DescriptorTable* m_descriptorTable;

    union
    {
        GraphicsPipeline* m_graphicsPipeline;
        ComputePipeline* m_computePipeline;
        RaytracingPipeline* m_raytracingPipeline;
    };

    DescriptorTableOffsetMap m_offsets;
    uint32 m_frameIndex;

    uint8 m_pipelineType : 2; // 0 = Graphics, 1 = Compute, 2 = Raytracing
};

class InsertBarrier final : public CmdBase
{
public:
    InsertBarrier(GpuBuffer* buffer, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(buffer),
          m_image(nullptr),
          m_state(state),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(false)
    {
    }

    InsertBarrier(GpuImage* image, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(false)
    {
    }

    InsertBarrier(GpuImage* image, const ResourceState& state, const ImageSubResource& subResource, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_subResource(subResource),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(true)
    {
    }

#if defined(HYP_VULKAN) && defined(HYP_DEBUG_MODE)
    HYP_API void CheckNotInRenderPass(CommandBuffer* commandBuffer) const;
#endif

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        InsertBarrier* cmdCasted = static_cast<InsertBarrier*>(cmd);

#if defined(HYP_VULKAN) && defined(HYP_DEBUG_MODE)
        cmdCasted->CheckNotInRenderPass(commandBuffer);
#endif

        if (cmdCasted->m_buffer)
        {
            cmdCasted->m_buffer->InsertBarrier(commandBuffer, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
        }
        else if (cmdCasted->m_image)
        {
            if (cmdCasted->m_hasSubResource)
            {
                cmdCasted->m_image->InsertBarrier(commandBuffer, cmdCasted->m_subResource, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
            }
            else
            {
                cmdCasted->m_image->InsertBarrier(commandBuffer, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
            }
        }

        static_assert(std::is_trivially_destructible_v<InsertBarrier>);
        // cmdCasted->~InsertBarrier();
    }

private:
    GpuBuffer* m_buffer;
    GpuImage* m_image;
    ResourceState m_state;
    ShaderModuleType m_shaderModuleType;
    ImageSubResource m_subResource;
    bool m_hasSubResource : 1;
};

class Blit final : public CmdBase
{
public:
    Blit(GpuImage* srcImage, GpuImage* dstImage)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_hasMipFaceInfo(false),
          m_hasSrcRect(false),
          m_hasDstRect(false)
    {
    }

    Blit(GpuImage* srcImage, GpuImage* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_hasMipFaceInfo(false),
          m_hasSrcRect(true),
          m_hasDstRect(true)
    {
    }

    Blit(GpuImage* srcImage, GpuImage* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect, uint32 srcMip, uint32 dstMip, uint32 srcFace, uint32 dstFace)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_mipFaceInfo(MipFaceInfo { srcMip, dstMip, srcFace, dstFace }),
          m_hasMipFaceInfo(true),
          m_hasSrcRect(true),
          m_hasDstRect(true)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        Blit* cmdCasted = static_cast<Blit*>(cmd);

        if (cmdCasted->m_hasMipFaceInfo)
        {
            MipFaceInfo info = cmdCasted->m_mipFaceInfo;

            if (cmdCasted->m_hasSrcRect && cmdCasted->m_hasDstRect)
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, cmdCasted->m_srcRect, cmdCasted->m_dstRect, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
            else
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
        }
        else
        {
            if (cmdCasted->m_hasSrcRect && cmdCasted->m_hasDstRect)
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, cmdCasted->m_srcRect, cmdCasted->m_dstRect);
            }
            else
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage);
            }
        }

        static_assert(std::is_trivially_destructible_v<Blit>);
        // cmdCasted->~Blit();
    }

private:
    struct MipFaceInfo
    {
        uint32 srcMip;
        uint32 dstMip;
        uint32 srcFace;
        uint32 dstFace;
    };

    GpuImage* m_srcImage;
    GpuImage* m_dstImage;

    MipFaceInfo m_mipFaceInfo;

    Rect<uint32> m_srcRect;
    Rect<uint32> m_dstRect;

    bool m_hasMipFaceInfo : 1;
    bool m_hasSrcRect : 1;
    bool m_hasDstRect : 1;
};

class BlitRect final : public CmdBase
{
public:
    BlitRect(GpuImage* srcImage, GpuImage* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        BlitRect* cmdCasted = static_cast<BlitRect*>(cmd);

        cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, cmdCasted->m_srcRect, cmdCasted->m_dstRect);

        static_assert(std::is_trivially_destructible_v<BlitRect>);
        // cmdCasted->~BlitRect();
    }

private:
    GpuImage* m_srcImage;
    GpuImage* m_dstImage;
    Rect<uint32> m_srcRect;
    Rect<uint32> m_dstRect;
};

class CopyImageToBuffer final : public CmdBase
{
public:
    CopyImageToBuffer(GpuImage* image, GpuBuffer* buffer)
        : m_image(image),
          m_buffer(buffer)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        CopyImageToBuffer* cmdCasted = static_cast<CopyImageToBuffer*>(cmd);

        cmdCasted->m_image->CopyToBuffer(commandBuffer, cmdCasted->m_buffer);

        static_assert(std::is_trivially_destructible_v<CopyImageToBuffer>);
        // cmdCasted->~CopyImageToBuffer();
    }

private:
    GpuImage* m_image;
    GpuBuffer* m_buffer;
};

class CopyBufferToImage final : public CmdBase
{
public:
    CopyBufferToImage(GpuBuffer* srcBuffer, GpuImage* dstImage)
        : m_srcBuffer(srcBuffer),
          m_dstImage(dstImage),
          m_srcBufferOffset(0),
          m_dstMipIndex(UINT8_MAX),
          m_dstArrayLayer(UINT16_MAX)
    {
    }

    CopyBufferToImage(GpuBuffer* srcBuffer, GpuImage* dstImage, uint32 srcBufferOffset, uint32 dstMipIndex, uint32 dstArrayLayer)
        : m_srcBuffer(srcBuffer),
          m_dstImage(dstImage),
          m_srcBufferOffset(srcBufferOffset),
          m_dstMipIndex(dstMipIndex),
          m_dstArrayLayer(dstArrayLayer)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        CopyBufferToImage* cmdCasted = static_cast<CopyBufferToImage*>(cmd);

        cmdCasted->m_dstImage->CopyFromBuffer(
            commandBuffer,
            cmdCasted->m_srcBuffer,
            cmdCasted->m_srcBufferOffset,
            cmdCasted->m_dstMipIndex,
            cmdCasted->m_dstArrayLayer);

        static_assert(std::is_trivially_destructible_v<CopyBufferToImage>);
        // cmdCasted->~CopyBufferToImage();
    }

private:
    GpuBuffer* m_srcBuffer;
    GpuImage* m_dstImage;

    uint32 m_srcBufferOffset;
    uint8 m_dstMipIndex;
    uint16 m_dstArrayLayer;
};

class CopyBuffer final : public CmdBase
{
public:
    CopyBuffer(GpuBuffer* srcBuffer, GpuBuffer* dstBuffer, uint32 count)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_srcOffset(0),
          m_dstOffset(0),
          m_count(count)
    {
        AssertDebug(srcBuffer && dstBuffer);
        AssertDebug(count <= srcBuffer->Size(), "Source buffer copy range out of bounds");
        AssertDebug(count <= dstBuffer->Size(), "Destination buffer copy range out of bounds");
    }

    CopyBuffer(GpuBuffer* srcBuffer, GpuBuffer* dstBuffer, uint32 srcOffset, uint32 dstOffset, uint32 count)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_srcOffset(srcOffset),
          m_dstOffset(dstOffset),
          m_count(count)
    {
        AssertDebug(srcBuffer && dstBuffer);
        AssertDebug(srcOffset + count <= srcBuffer->Size(), "Source buffer copy range out of bounds");
        AssertDebug(dstOffset + count <= dstBuffer->Size(), "Destination buffer copy range out of bounds");
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        CopyBuffer* cmdCasted = static_cast<CopyBuffer*>(cmd);

        AssertDebug(cmdCasted->m_srcBuffer && cmdCasted->m_dstBuffer);
        AssertDebug(cmdCasted->m_srcOffset + cmdCasted->m_count <= cmdCasted->m_srcBuffer->Size(), "Source buffer copy range out of bounds: {}", cmdCasted->m_srcOffset + cmdCasted->m_count);
        AssertDebug(cmdCasted->m_dstOffset + cmdCasted->m_count <= cmdCasted->m_dstBuffer->Size(), "Destination buffer copy range out of bounds {}", cmdCasted->m_dstOffset + cmdCasted->m_count);

        cmdCasted->m_dstBuffer->CopyFrom(
            commandBuffer,
            cmdCasted->m_srcBuffer,
            cmdCasted->m_srcOffset,
            cmdCasted->m_dstOffset,
            cmdCasted->m_count);

        static_assert(std::is_trivially_destructible_v<CopyBuffer>);
        // cmdCasted->~CopyBuffer();
    }

private:
    GpuBuffer* m_srcBuffer;
    GpuBuffer* m_dstBuffer;
    uint32 m_srcOffset;
    uint32 m_dstOffset;
    uint32 m_count;
};

class GenerateMipmaps final : public CmdBase
{
public:
    GenerateMipmaps(GpuImage* image)
        : m_image(image)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        GenerateMipmaps* cmdCasted = static_cast<GenerateMipmaps*>(cmd);

        cmdCasted->m_image->GenerateMipmaps(commandBuffer);

        static_assert(std::is_trivially_destructible_v<GenerateMipmaps>);
        // cmdCasted->~GenerateMipmaps();
    }

private:
    GpuImage* m_image;
};

class DispatchCompute final : public CmdBase
{
public:
    DispatchCompute(ComputePipeline* pipeline, Vec3u workgroupCount)
        : m_pipeline(pipeline),
          m_workgroupCount(workgroupCount)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    ComputePipeline* m_pipeline;
    Vec3u m_workgroupCount;
};

class TraceRays final : public CmdBase
{
public:
    TraceRays(RaytracingPipeline* pipeline, const Vec3u& workgroupCount)
        : m_pipeline(pipeline),
          m_workgroupCount(workgroupCount)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    RaytracingPipeline* m_pipeline;
    Vec3u m_workgroupCount;
};

class SetStencilState final : public CmdBase
{
public:
    explicit SetStencilState(uint8 referenceValue, uint8 compareMask = 0xFF, uint8 writeMask = 0xFF)
        : m_referenceValue(referenceValue),
          m_compareMask(compareMask),
          m_writeMask(writeMask)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    uint8 m_referenceValue;
    uint8 m_compareMask;
    uint8 m_writeMask;
};

class SetCurrentShader final : public CmdBase
{
public:
    explicit SetCurrentShader(Shader* shader)
        : m_shader(shader)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    Shader* m_shader;
};

class SetCurrentView final : public CmdBase
{
public:
    explicit SetCurrentView(View* view)
        : m_view(view)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    View* m_view;
};

class SetCurrentRenderGroup final : public CmdBase
{
public:
    explicit SetCurrentRenderGroup(RenderGroup* renderGroup)
        : m_renderGroup(renderGroup)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    RenderGroup* m_renderGroup;
};

class SetShaderUniform final : public CmdBase
{
public:
    SetShaderUniform(uint32 uniformIndex, StringHash name, GpuBuffer* buffer, uint32 bufferOffset = 0)
        : uniformIndex(uniformIndex),
          uniform { name, { .buffer = buffer }, ShaderUniform::UT_Buffer, bufferOffset }
    {
    }
    
    SetShaderUniform(uint32 uniformIndex, StringHash name, GpuImageView* imageView)
        : uniformIndex(uniformIndex),
          uniform { name, { .imageView = imageView }, ShaderUniform::UT_ImageView }
    {
    }
    
    SetShaderUniform(uint32 uniformIndex, StringHash name, Sampler* sampler)
        : uniformIndex(uniformIndex),
          uniform { name, { .sampler = sampler }, ShaderUniform::UT_Sampler }
    {
    }
    
    SetShaderUniform(uint32 uniformIndex, StringHash name, GpuTlas* tlas)
        : uniformIndex(uniformIndex),
          uniform { name, { .tlas = tlas }, ShaderUniform::UT_Tlas }
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    uint32 uniformIndex;
    ShaderUniform uniform;
};

class CommitDrawState final : public CmdBase
{
public:
    CommitDrawState() = default;

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);
};

class RenderQueueBase
{
protected:
    using InvokeCmdFnPtr = void (*)(CmdBase*, CommandBuffer*);
    using PrepareCmdFnPtr = void (*)(CmdBase*, Frame* frame);
    using MoveCmdFnPtr = void (*)(CmdBase*, void* where);

    struct CmdHeader
    {
        uint32 offset;
        uint32 size;
        InvokeCmdFnPtr invokeFnPtr;
        PrepareCmdFnPtr prepareFnPtr;
    };

    RenderQueueBase() = default;
};

template <class AllocatorType>
class TRenderQueue : public RenderQueueBase
{
    template <class OtherAllocatorType>
    friend class TRenderQueue;

    using Base = RenderQueueBase;

    using Base::CmdHeader;
    using Base::InvokeCmdFnPtr;
    using Base::MoveCmdFnPtr;
    using Base::PrepareCmdFnPtr;

public:
    TRenderQueue()
        : m_offset(0)
    {
    }

    explicit TRenderQueue(AllocatorType* pAllocator)
        : m_cmdHeaders(pAllocator),
          m_buffer(pAllocator),
          m_offset(0)
    {
        AssertDebug(pAllocator != nullptr);
    }

    TRenderQueue(const TRenderQueue& other) = delete;
    TRenderQueue& operator=(const TRenderQueue& other) = delete;

    TRenderQueue(TRenderQueue&& other) noexcept = delete;
    TRenderQueue& operator=(TRenderQueue&& other) noexcept = delete;

    ~TRenderQueue();

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_offset == 0;
    }

    template <class CmdType>
    void Add(CmdType&& cmd)
    {
        using TCmd = NormalizedType<CmdType>;
        static_assert(alignof(TCmd) <= 16, "CmdType should have alignment <= 16!");

        static_assert(std::is_trivially_copyable_v<TCmd> && std::is_trivially_destructible_v<TCmd>,
            "CmdType should be trivially copyable and destructible!");

        constexpr SizeType CmdSize = sizeof(TCmd);

        const uint32 alignedOffset = ByteUtil::AlignAs(m_offset, alignof(TCmd));

        if (m_buffer.Size() < alignedOffset + CmdSize)
        {
            m_buffer.SetSize(MathUtil::Ceil<SizeType>(1.5 * (alignedOffset + CmdSize)), /* zeroize */ false);
        }

        void* startPtr = m_buffer.Data() + alignedOffset;
        new (startPtr) TCmd(std::forward<CmdType>(cmd));

        CmdHeader& header = m_cmdHeaders.EmplaceBack();
        header.offset = alignedOffset;
        header.size = CmdSize;
        header.invokeFnPtr = &TCmd::InvokeStatic;
        header.prepareFnPtr = &TCmd::PrepareStatic;

        m_offset = alignedOffset + CmdSize;
    }

    template <class CmdType>
    TRenderQueue& operator<<(CmdType&& cmd)
    {
        Add(std::forward<CmdType>(cmd));

        return *this;
    }

    template <class OtherAllocatorType>
    void Concat(TRenderQueue<OtherAllocatorType>& other)
    {
        m_cmdHeaders.Reserve(m_cmdHeaders.Size() + other.m_cmdHeaders.Size());

        // since we guarantee <= 16 byte alignment, we should just align our offset to 16 to make sure everything fits
        const uint32 newStartOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_buffer.GetCapacity() < newStartOffset + other.m_offset)
        {
            m_buffer.SetSize(MathUtil::Ceil<SizeType>(1.5 * (newStartOffset + other.m_offset)), /* zeroize */ false);
        }
        else
        {
            ubyte* prevPtr = m_buffer.Data();

            // No need to reconstruct commands if the allocation did not change
            m_buffer.SetSize(newStartOffset + other.m_offset, /* zeroize */ false);

            // Sanity check to ensure SetSize() did not change our capacity. (it shouldn't)
            AssertDebug(m_buffer.Data() == prevPtr);
        }

        SizeType cmdsOffset = m_cmdHeaders.Size();

        // Reconstruct the commands into our memory
        Memory::MemCpy(m_buffer.Data() + newStartOffset, other.m_buffer.Data(), other.m_offset);

        // Add headers and update offsets
        for (const CmdHeader& cmdHeader : other.m_cmdHeaders)
        {
            CmdHeader& newCmdHeader = m_cmdHeaders.PushBack(cmdHeader);
            newCmdHeader.offset += newStartOffset;
        }

        //        // Copy from other buffer, starting at the new offset
        //        m_buffer.Write(other.m_offset, newStartOffset, other.m_buffer.Data());

        m_offset = newStartOffset + other.m_offset;
    }

    void Prepare(Frame* frame);
    void Execute(CommandBuffer* commandBuffer);

    void Clear()
    {
        m_cmdHeaders.Clear();
        m_cmdHeaders.Refit();

        m_buffer.Clear();

        m_offset = 0;
    }

private:
    Array<CmdHeader, AllocatorType> m_cmdHeaders;
    TByteBuffer<AllocatorType> m_buffer;
    uint32 m_offset;
};

template <class AllocatorType>
TRenderQueue<AllocatorType>::~TRenderQueue()
{
    Assert(m_cmdHeaders.Empty(), "RenderQueue destroyed with pending commands!");
}

using RenderQueue = TRenderQueue<RenderAllocator>;

} // namespace Hyperion
