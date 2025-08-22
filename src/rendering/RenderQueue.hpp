/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Variant.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/rt/RenderRaytracingPipeline.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderFramebuffer.hpp>
#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderCommandBuffer.hpp>
#include <rendering/RenderObject.hpp>

// #define HYP_RHI_COMMAND_STACK_TRACE

#ifdef HYP_RHI_COMMAND_STACK_TRACE
#include <core/debug/StackDump.hpp>
#endif

namespace hyperion {

class CmdBase;

class CmdBase
{
public:
    static inline void PrepareStatic(CmdBase* cmd, FrameBase* frame)
    {
    }
};

class BindVertexBuffer final : public CmdBase
{
public:
    BindVertexBuffer(GpuBufferBase* buffer)
        : m_buffer(buffer)
    {
        Assert(buffer && buffer->IsCreated());
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindVertexBuffer* cmdCasted = static_cast<BindVertexBuffer*>(cmd);

        commandBuffer->BindVertexBuffer(cmdCasted->m_buffer);

        static_assert(std::is_trivially_destructible_v<BindVertexBuffer>);
        // cmdCasted->~BindVertexBuffer();
    }

private:
    GpuBufferBase* m_buffer;
};

class BindIndexBuffer final : public CmdBase
{
public:
    BindIndexBuffer(GpuBufferBase* buffer)
        : m_buffer(buffer)
    {
        Assert(buffer && buffer->IsCreated());
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindIndexBuffer* cmdCasted = static_cast<BindIndexBuffer*>(cmd);

        commandBuffer->BindIndexBuffer(cmdCasted->m_buffer);

        static_assert(std::is_trivially_destructible_v<BindIndexBuffer>);
        // cmdCasted->~BindIndexBuffer();
    }

private:
    GpuBufferBase* m_buffer;
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

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
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
    DrawIndexedIndirect(GpuBufferBase* buffer, uint32 bufferOffset)
        : m_buffer(buffer),
          m_bufferOffset(bufferOffset)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        DrawIndexedIndirect* cmdCasted = static_cast<DrawIndexedIndirect*>(cmd);

        commandBuffer->DrawIndexedIndirect(cmdCasted->m_buffer, cmdCasted->m_bufferOffset);

        static_assert(std::is_trivially_destructible_v<DrawIndexedIndirect>);
        // cmdCasted->~DrawIndexedIndirect();
    }

private:
    GpuBufferBase* m_buffer;
    uint32 m_bufferOffset;
};

class DrawQuad final : public CmdBase
{
public:
    static void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer);
};

class BeginFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API BeginFramebuffer(FramebufferBase* framebuffer);
    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);
#else
    BeginFramebuffer(FramebufferBase* framebuffer)
        : m_framebuffer(framebuffer)
    {
    }
#endif

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BeginFramebuffer* cmdCasted = static_cast<BeginFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->BeginCapture(commandBuffer);

        static_assert(std::is_trivially_destructible_v<BeginFramebuffer>);
        // cmdCasted->~BeginFramebuffer();
    }

private:
    FramebufferBase* m_framebuffer;
};

class EndFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API EndFramebuffer(FramebufferBase* framebuffer);
    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);
#else
    EndFramebuffer(FramebufferBase* framebuffer)
        : m_framebuffer(framebuffer)
    {
    }
#endif

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        EndFramebuffer* cmdCasted = static_cast<EndFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->EndCapture(commandBuffer);

        static_assert(std::is_trivially_destructible_v<EndFramebuffer>);
        // cmdCasted->~EndFramebuffer();
    }

private:
    FramebufferBase* m_framebuffer;
};

class ClearFramebuffer final : public CmdBase
{
public:
    HYP_API ClearFramebuffer(FramebufferBase* framebuffer)
        : m_framebuffer(framebuffer)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        ClearFramebuffer* cmdCasted = static_cast<ClearFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->Clear(commandBuffer);

        static_assert(std::is_trivially_destructible_v<ClearFramebuffer>);
        // cmdCasted->~ClearFramebuffer();
    }

private:
    FramebufferBase* m_framebuffer;
};

class BindGraphicsPipeline final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API BindGraphicsPipeline(GraphicsPipelineBase* pipeline, Vec2i viewportOffset, Vec2u viewportExtent);
    HYP_API BindGraphicsPipeline(GraphicsPipelineBase* pipeline);
#else
    BindGraphicsPipeline(GraphicsPipelineBase* pipeline, Vec2i viewportOffset, Vec2u viewportExtent)
        : m_pipeline(pipeline),
          m_viewportOffset(viewportOffset),
          m_viewportExtent(viewportExtent)
    {
    }

    BindGraphicsPipeline(GraphicsPipelineBase* pipeline)
        : m_pipeline(pipeline)
    {
    }
#endif

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase*);

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);

        if (cmdCasted->m_viewportOffset != Vec2i(0, 0) || cmdCasted->m_viewportExtent != Vec2u(0, 0))
        {
            cmdCasted->m_pipeline->Bind(commandBuffer, cmdCasted->m_viewportOffset, cmdCasted->m_viewportExtent);
        }
        else
        {
            cmdCasted->m_pipeline->Bind(commandBuffer);
        }

        static_assert(std::is_trivially_destructible_v<BindGraphicsPipeline>);
        // cmdCasted->~BindGraphicsPipeline();
    }

private:
    GraphicsPipelineBase* m_pipeline;
    Vec2i m_viewportOffset;
    Vec2u m_viewportExtent;
};

class BindComputePipeline final : public CmdBase
{
public:
    BindComputePipeline(ComputePipelineBase* pipeline)
        : m_pipeline(pipeline)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindComputePipeline* cmdCasted = static_cast<BindComputePipeline*>(cmd);

        cmdCasted->m_pipeline->Bind(commandBuffer);

        static_assert(std::is_trivially_destructible_v<BindComputePipeline>);
        // cmdCasted->~BindComputePipeline();
    }

private:
    ComputePipelineBase* m_pipeline;
};

class BindRaytracingPipeline final : public CmdBase
{
public:
    BindRaytracingPipeline(RaytracingPipelineBase* pipeline)
        : m_pipeline(pipeline)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindRaytracingPipeline* cmdCasted = static_cast<BindRaytracingPipeline*>(cmd);

        cmdCasted->m_pipeline->Bind(commandBuffer);

        static_assert(std::is_trivially_destructible_v<BindRaytracingPipeline>);
        // cmdCasted->~BindRaytracingPipeline();
    }

private:
    RaytracingPipelineBase* m_pipeline;
};

class BindDescriptorSet final : public CmdBase
{
public:
    BindDescriptorSet(DescriptorSetBase* descriptorSet, GraphicsPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_graphicsPipeline(pipeline),
          m_offsets(offsets),
          m_pipelineType(0) // 0 = Graphics
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, GraphicsPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_graphicsPipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex),
          m_pipelineType(0) // 0 = Graphics
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, ComputePipelineBase* pipeline, const DescriptorSetOffsetMap& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_computePipeline(pipeline),
          m_offsets(offsets),
          m_pipelineType(1) // 1 = Compute
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, ComputePipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_computePipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex),
          m_pipelineType(1) // 1 = Compute
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, RaytracingPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_raytracingPipeline(pipeline),
          m_offsets(offsets),
          m_pipelineType(2) // 2 = Raytracing
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, RaytracingPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_raytracingPipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex),
          m_pipelineType(2) // 2 = Raytracing
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

        switch (cmdCasted->m_pipelineType)
        {
        case 0: // Graphics
            cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_graphicsPipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
            break;
        case 1: // Compute
            cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_computePipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
            break;
        case 2: // Raytracing
            cmdCasted->m_descriptorSet->Bind(commandBuffer, cmdCasted->m_raytracingPipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
            break;
        default:
            HYP_UNREACHABLE();
        }
        
        static_assert(std::is_trivially_destructible_v<BindDescriptorSet>);
        // cmdCasted->~BindDescriptorSet();
    }

private:
    DescriptorSetBase* m_descriptorSet;
    union
    {
        GraphicsPipelineBase* m_graphicsPipeline;
        ComputePipelineBase* m_computePipeline;
        RaytracingPipelineBase* m_raytracingPipeline;
    };
    DescriptorSetOffsetMap m_offsets;
    uint32 m_bindIndex;
    uint8 m_pipelineType : 2; // 0 = Graphics, 1 = Compute, 2 = Raytracing
};

class BindDescriptorTable final : public CmdBase
{
public:
    BindDescriptorTable(DescriptorTableBase* descriptorTable, GraphicsPipelineBase* graphicsPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_graphicsPipeline(graphicsPipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex),
          m_pipelineType(0) // 0 = Graphics
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(DescriptorTableBase* descriptorTable, ComputePipelineBase* computePipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_computePipeline(computePipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex),
          m_pipelineType(1) // 1 = Compute
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(DescriptorTableBase* descriptorTable, RaytracingPipelineBase* raytracingPipeline, const DescriptorTableOffsetMap& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_raytracingPipeline(raytracingPipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex),
          m_pipelineType(2) // 2 = Raytracing
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindDescriptorTable* cmdCasted = static_cast<BindDescriptorTable*>(cmd);

        switch (cmdCasted->m_pipelineType)
        {
        case 0: // Graphics
            cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, cmdCasted->m_graphicsPipeline, cmdCasted->m_offsets);
            break;
        case 1: // Compute
            cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, cmdCasted->m_computePipeline, cmdCasted->m_offsets);
            break;
        case 2: // Raytracing
            cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, cmdCasted->m_raytracingPipeline, cmdCasted->m_offsets);
            break;
        default:
            HYP_UNREACHABLE();
        }

        static_assert(std::is_trivially_destructible_v<BindDescriptorTable>);
        // cmdCasted->~BindDescriptorTable();
    }

private:
    DescriptorTableBase* m_descriptorTable;

    union
    {
        GraphicsPipelineBase* m_graphicsPipeline;
        ComputePipelineBase* m_computePipeline;
        RaytracingPipelineBase* m_raytracingPipeline;
    };

    DescriptorTableOffsetMap m_offsets;
    uint32 m_frameIndex;

    uint8 m_pipelineType : 2; // 0 = Graphics, 1 = Compute, 2 = Raytracing
};

class InsertBarrier final : public CmdBase
{
public:
    InsertBarrier(GpuBufferBase* buffer, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(buffer),
          m_image(nullptr),
          m_state(state),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(false)
    {
    }

    InsertBarrier(GpuImageBase* image, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(false)
    {
    }

    InsertBarrier(GpuImageBase* image, const ResourceState& state, const ImageSubResource& subResource, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_subResource(subResource),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(true)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        InsertBarrier* cmdCasted = static_cast<InsertBarrier*>(cmd);

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
    GpuBufferBase* m_buffer;
    GpuImageBase* m_image;
    ResourceState m_state;
    ShaderModuleType m_shaderModuleType;
    ImageSubResource m_subResource;
    bool m_hasSubResource : 1;
};

class Blit final : public CmdBase
{
public:
    Blit(GpuImageBase* srcImage, GpuImageBase* dstImage)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_hasMipFaceInfo(false),
          m_hasSrcRect(false),
          m_hasDstRect(false)
    {
    }

    Blit(GpuImageBase* srcImage, GpuImageBase* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_hasMipFaceInfo(false),
          m_hasSrcRect(true),
          m_hasDstRect(true)
    {
    }

    Blit(GpuImageBase* srcImage, GpuImageBase* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect, uint32 srcMip, uint32 dstMip, uint32 srcFace, uint32 dstFace)
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

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
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

    GpuImageBase* m_srcImage;
    GpuImageBase* m_dstImage;

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
    BlitRect(GpuImageBase* srcImage, GpuImageBase* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BlitRect* cmdCasted = static_cast<BlitRect*>(cmd);

        cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, cmdCasted->m_srcRect, cmdCasted->m_dstRect);

        static_assert(std::is_trivially_destructible_v<BlitRect>);
        // cmdCasted->~BlitRect();
    }

private:
    GpuImageBase* m_srcImage;
    GpuImageBase* m_dstImage;
    Rect<uint32> m_srcRect;
    Rect<uint32> m_dstRect;
};

class CopyImageToBuffer final : public CmdBase
{
public:
    CopyImageToBuffer(GpuImageBase* image, GpuBufferBase* buffer)
        : m_image(image),
          m_buffer(buffer)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        CopyImageToBuffer* cmdCasted = static_cast<CopyImageToBuffer*>(cmd);

        cmdCasted->m_image->CopyToBuffer(commandBuffer, cmdCasted->m_buffer);

        static_assert(std::is_trivially_destructible_v<CopyImageToBuffer>);
        // cmdCasted->~CopyImageToBuffer();
    }

private:
    GpuImageBase* m_image;
    GpuBufferBase* m_buffer;
};

class CopyBufferToImage final : public CmdBase
{
public:
    CopyBufferToImage(GpuBufferBase* buffer, GpuImageBase* image)
        : m_buffer(buffer),
          m_image(image)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        CopyBufferToImage* cmdCasted = static_cast<CopyBufferToImage*>(cmd);

        cmdCasted->m_image->CopyFromBuffer(commandBuffer, cmdCasted->m_buffer);

        static_assert(std::is_trivially_destructible_v<CopyBufferToImage>);
        // cmdCasted->~CopyBufferToImage();
    }

private:
    GpuBufferBase* m_buffer;
    GpuImageBase* m_image;
};

class CopyBuffer final : public CmdBase
{
public:
    CopyBuffer(GpuBufferBase* srcBuffer, GpuBufferBase* dstBuffer, uint32 count)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_srcOffset(0),
          m_dstOffset(0),
          m_count(count)
    {
    }

    CopyBuffer(GpuBufferBase* srcBuffer, GpuBufferBase* dstBuffer, uint32 srcOffset, uint32 dstOffset, uint32 count)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_srcOffset(srcOffset),
          m_dstOffset(dstOffset),
          m_count(count)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        CopyBuffer* cmdCasted = static_cast<CopyBuffer*>(cmd);

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
    GpuBufferBase* m_srcBuffer;
    GpuBufferBase* m_dstBuffer;
    uint32 m_srcOffset;
    uint32 m_dstOffset;
    uint32 m_count;
};

class GenerateMipmaps final : public CmdBase
{
public:
    GenerateMipmaps(GpuImageBase* image)
        : m_image(image)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        GenerateMipmaps* cmdCasted = static_cast<GenerateMipmaps*>(cmd);

        cmdCasted->m_image->GenerateMipmaps(commandBuffer);

        static_assert(std::is_trivially_destructible_v<GenerateMipmaps>);
        // cmdCasted->~GenerateMipmaps();
    }

private:
    GpuImageBase* m_image;
};

class DispatchCompute final : public CmdBase
{
public:
    DispatchCompute(ComputePipelineBase* pipeline, Vec3u workgroupCount)
        : m_pipeline(pipeline),
          m_workgroupCount(workgroupCount)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        DispatchCompute* cmdCasted = static_cast<DispatchCompute*>(cmd);

        cmdCasted->m_pipeline->Dispatch(commandBuffer, cmdCasted->m_workgroupCount);

        static_assert(std::is_trivially_destructible_v<DispatchCompute>);
        // cmdCasted->~DispatchCompute();
    }

private:
    ComputePipelineBase* m_pipeline;
    Vec3u m_workgroupCount;
};

class TraceRays final : public CmdBase
{
public:
    TraceRays(RaytracingPipelineBase* pipeline, const Vec3u& workgroupCount)
        : m_pipeline(pipeline),
          m_workgroupCount(workgroupCount)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        TraceRays* cmdCasted = static_cast<TraceRays*>(cmd);

        cmdCasted->m_pipeline->TraceRays(commandBuffer, cmdCasted->m_workgroupCount);

        static_assert(std::is_trivially_destructible_v<TraceRays>);
        // cmdCasted->~TraceRays();
    }

private:
    RaytracingPipelineBase* m_pipeline;
    Vec3u m_workgroupCount;
};

HYP_DISABLE_OPTIMIZATION;
class RenderQueue
{
    using InvokeCmdFnPtr = void (*)(CmdBase*, CommandBufferBase*);
    using PrepareCmdFnPtr = void (*)(CmdBase*, FrameBase* frame);
    using MoveCmdFnPtr = void (*)(CmdBase*, void* where);

    struct CmdHeader
    {
        uint32 offset;
        uint32 size;
        InvokeCmdFnPtr invokeFnPtr;
        PrepareCmdFnPtr prepareFnPtr;
    };

public:
    RenderQueue();
    RenderQueue(const RenderQueue& other) = delete;
    RenderQueue& operator=(const RenderQueue& other) = delete;
    RenderQueue(RenderQueue&& other) noexcept = delete;
    RenderQueue& operator=(RenderQueue&& other) noexcept = delete;
    ~RenderQueue();

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_offset == 0;
    }

    template <class CmdType>
    void Add(CmdType&& cmd)
    {
        using TCmd = NormalizedType<CmdType>;
        static_assert(alignof(TCmd) <= 16, "CmdType should have alignment <= 16!");

        static_assert(isPodType<CmdType>, "CmdType must be POD");

        constexpr SizeType cmdSize = sizeof(TCmd);

        const uint32 alignedOffset = ByteUtil::AlignAs(m_offset, alignof(TCmd));

        if (m_buffer.Size() < alignedOffset + cmdSize)
        {
            m_buffer.SetSize(2 * (alignedOffset + cmdSize), /* zeroize */ false);
        }

        void* startPtr = m_buffer.Data() + alignedOffset;
        new (startPtr) TCmd(std::forward<CmdType>(cmd));

        CmdHeader& header = m_cmdHeaders.EmplaceBack();
        header.offset = alignedOffset;
        header.size = cmdSize;
        header.invokeFnPtr = &TCmd::InvokeStatic;
        header.prepareFnPtr = &TCmd::PrepareStatic;

        m_offset = alignedOffset + cmdSize;
    }

    template <class CmdType>
    RenderQueue& operator<<(CmdType&& cmd)
    {
        Add(std::forward<CmdType>(cmd));

        return *this;
    }

    void Concat(RenderQueue&& other)
    {
        m_cmdHeaders.Reserve(m_cmdHeaders.Size() + other.m_cmdHeaders.Size());

        // since we guarantee <= 16 byte alignment, we should just align our offset to 16 to make sure everything fits
        const uint32 newStartOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_buffer.GetCapacity() < newStartOffset + other.m_offset)
        {
            m_buffer.SetSize(2 * (newStartOffset + other.m_offset), /* zeroize */ false);
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

        // clear out allocation
        other.m_buffer = ByteBuffer();
        other.m_cmdHeaders.Clear();
        other.m_offset = 0;
    }

    void Prepare(FrameBase* frame);
    void Execute(CommandBufferBase* commandBuffer);

private:

    Array<CmdHeader> m_cmdHeaders;
    ByteBuffer m_buffer;
    uint32 m_offset;
};
HYP_ENABLE_OPTIMIZATION;

} // namespace hyperion
