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
    CmdBase() = default;
    CmdBase(const CmdBase& other) = delete;
    CmdBase& operator=(const CmdBase& other) = delete;
    CmdBase(CmdBase&& other) noexcept = default;
    CmdBase& operator=(CmdBase&& other) noexcept = default;

    static inline void PrepareStatic(CmdBase* cmd, FrameBase* frame)
    {
    }

protected:
    ~CmdBase() = default;
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

        cmdCasted->~BindVertexBuffer();
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

        cmdCasted->~BindIndexBuffer();
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

        cmdCasted->~DrawIndexed();
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

        cmdCasted->~DrawIndexedIndirect();
    }

private:
    GpuBufferBase* m_buffer;
    uint32 m_bufferOffset;
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

        cmdCasted->~BeginFramebuffer();
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

        cmdCasted->~EndFramebuffer();
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

        cmdCasted->~ClearFramebuffer();
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

        cmdCasted->~BindGraphicsPipeline();
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

        cmdCasted->~BindComputePipeline();
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

        cmdCasted->~BindRaytracingPipeline();
    }

private:
    RaytracingPipelineBase* m_pipeline;
};

class BindDescriptorSet final : public CmdBase
{
public:
    BindDescriptorSet(DescriptorSetBase* descriptorSet, GraphicsPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, GraphicsPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, ComputePipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, ComputePipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, RaytracingPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(DescriptorSetBase* descriptorSet, RaytracingPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

        cmdCasted->m_pipeline.Visit([cmdCasted, commandBuffer](auto* pipeline)
            {
                cmdCasted->m_descriptorSet->Bind(commandBuffer, pipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
            });

        cmdCasted->~BindDescriptorSet();
    }

private:
    DescriptorSetBase* m_descriptorSet;
    Variant<GraphicsPipelineBase*, ComputePipelineBase*, RaytracingPipelineBase*> m_pipeline;
    ArrayMap<Name, uint32> m_offsets;
    uint32 m_bindIndex;
};

class BindDescriptorTable final : public CmdBase
{
public:
    BindDescriptorTable(DescriptorTableBase* descriptorTable, GraphicsPipelineBase* graphicsPipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_pipeline(graphicsPipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex)
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(DescriptorTableBase* descriptorTable, ComputePipelineBase* computePipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_pipeline(computePipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex)
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(DescriptorTableBase* descriptorTable, RaytracingPipelineBase* raytracingPipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_pipeline(raytracingPipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex)
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        BindDescriptorTable* cmdCasted = static_cast<BindDescriptorTable*>(cmd);

        cmdCasted->m_pipeline.Visit([cmdCasted, commandBuffer](auto* pipeline)
            {
                cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, pipeline, cmdCasted->m_offsets);
            });

        cmdCasted->~BindDescriptorTable();
    }

private:
    DescriptorTableBase* m_descriptorTable;
    Variant<GraphicsPipelineBase*, ComputePipelineBase*, RaytracingPipelineBase*> m_pipeline;
    ArrayMap<Name, ArrayMap<Name, uint32>> m_offsets;
    uint32 m_frameIndex;
};

class InsertBarrier final : public CmdBase
{
public:
    InsertBarrier(GpuBufferBase* buffer, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(buffer),
          m_image(nullptr),
          m_state(state),
          m_shaderModuleType(shaderModuleType)
    {
    }

    InsertBarrier(GpuImageBase* image, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_shaderModuleType(shaderModuleType)
    {
    }

    InsertBarrier(GpuImageBase* image, const ResourceState& state, const ImageSubResource& subResource, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_subResource(subResource),
          m_shaderModuleType(shaderModuleType)
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
            if (cmdCasted->m_subResource)
            {
                cmdCasted->m_image->InsertBarrier(commandBuffer, *cmdCasted->m_subResource, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
            }
            else
            {
                cmdCasted->m_image->InsertBarrier(commandBuffer, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
            }
        }

        cmdCasted->~InsertBarrier();
    }

private:
    GpuBufferBase* m_buffer;
    GpuImageBase* m_image;
    ResourceState m_state;
    Optional<ImageSubResource> m_subResource;
    ShaderModuleType m_shaderModuleType;
};

class Blit final : public CmdBase
{
public:
    Blit(GpuImageBase* srcImage, GpuImageBase* dstImage)
        : m_srcImage(srcImage),
          m_dstImage(dstImage)
    {
    }

    Blit(GpuImageBase* srcImage, GpuImageBase* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect)
    {
    }

    Blit(GpuImageBase* srcImage, GpuImageBase* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect, uint32 srcMip, uint32 dstMip, uint32 srcFace, uint32 dstFace)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_mipFaceInfo(MipFaceInfo { srcMip, dstMip, srcFace, dstFace })
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        Blit* cmdCasted = static_cast<Blit*>(cmd);

        if (cmdCasted->m_mipFaceInfo)
        {
            MipFaceInfo info = *cmdCasted->m_mipFaceInfo;

            if (cmdCasted->m_srcRect && cmdCasted->m_dstRect)
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, *cmdCasted->m_srcRect, *cmdCasted->m_dstRect, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
            else
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
        }
        else
        {
            if (cmdCasted->m_srcRect && cmdCasted->m_dstRect)
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, *cmdCasted->m_srcRect, *cmdCasted->m_dstRect);
            }
            else
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage);
            }
        }

        cmdCasted->~Blit();
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

    Optional<Rect<uint32>> m_srcRect;
    Optional<Rect<uint32>> m_dstRect;

    Optional<MipFaceInfo> m_mipFaceInfo;
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

        cmdCasted->~BlitRect();
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

        cmdCasted->~CopyImageToBuffer();
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

        cmdCasted->~CopyBufferToImage();
    }

private:
    GpuBufferBase* m_buffer;
    GpuImageBase* m_image;
};

class CopyBuffer final : public CmdBase
{
public:
    CopyBuffer(GpuBufferBase* srcBuffer, GpuBufferBase* dstBuffer, SizeType size)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_size(size)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBufferBase* commandBuffer)
    {
        CopyBuffer* cmdCasted = static_cast<CopyBuffer*>(cmd);

        cmdCasted->m_dstBuffer->CopyFrom(commandBuffer, cmdCasted->m_srcBuffer, cmdCasted->m_size);

        cmdCasted->~CopyBuffer();
    }

private:
    GpuBufferBase* m_srcBuffer;
    GpuBufferBase* m_dstBuffer;
    SizeType m_size;
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

        cmdCasted->~GenerateMipmaps();
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

        cmdCasted->~DispatchCompute();
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

        cmdCasted->~TraceRays();
    }

private:
    RaytracingPipelineBase* m_pipeline;
    Vec3u m_workgroupCount;
};

class RenderQueue
{
    using InvokeCmdFnPtr = void (*)(CmdBase*, CommandBufferBase*);
    using PrepareCmdFnPtr = void (*)(CmdBase*, FrameBase* frame);
    using MoveCmdFnPtr = void (*)(CmdBase*, void* where);

    struct CmdHeader
    {
        uint32 dataOffset;
        InvokeCmdFnPtr invokeFnPtr;
        PrepareCmdFnPtr prepareFnPtr;
        MoveCmdFnPtr moveFnPtr;
    };

    template <class CmdType>
    static inline void MoveCmdStatic(CmdBase* cmd, void* where)
    {
        new (where) CmdType(std::move(*static_cast<CmdType*>(cmd)));
        static_cast<CmdType*>(cmd)->~CmdType();
    }

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

        constexpr SizeType cmdSize = sizeof(TCmd);

        const uint32 alignedOffset = ByteUtil::AlignAs(m_offset, alignof(TCmd));

        if (m_buffer.Size() < alignedOffset + cmdSize)
        {
            ubyte* prevPtr = m_buffer.Data();

            ByteBuffer newBuffer;
            newBuffer.SetSize(2 * (alignedOffset + cmdSize));

            ubyte* newPtr = newBuffer.Data();

            ReconstructCommands(m_cmdHeaders.ToSpan(), prevPtr, newPtr);

            m_buffer = std::move(newBuffer);

            AssertDebug(m_buffer.Data() == newPtr);
        }

        void* startPtr = m_buffer.Data() + alignedOffset;
        new (startPtr) TCmd(std::forward<CmdType>(cmd));

        m_cmdHeaders.PushBack(CmdHeader {
            alignedOffset,
            &TCmd::InvokeStatic,
            &TCmd::PrepareStatic,
            &MoveCmdStatic<TCmd> });

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
            ubyte* prevPtr = m_buffer.Data();

            ByteBuffer newBuffer;
            newBuffer.SetSize(2 * (newStartOffset + other.m_offset));

            ubyte* newPtr = newBuffer.Data();

            ReconstructCommands(m_cmdHeaders.ToSpan(), prevPtr, newPtr);

            m_buffer = std::move(newBuffer);

            AssertDebug(m_buffer.Data() == newPtr);
        }
        else
        {
            ubyte* prevPtr = m_buffer.Data();

            // No need to reconstruct commands if the allocation did not change
            m_buffer.SetSize(newStartOffset + other.m_offset);

            // Sanity check to ensure SetSize() did not change our capacity. (it shouldn't)
            AssertDebug(m_buffer.Data() == prevPtr);
        }

        SizeType cmdsOffset = m_cmdHeaders.Size();

        // Reconstruct the commands into our memory
        ReconstructCommands(other.m_cmdHeaders.ToSpan(), other.m_buffer.Data(), m_buffer.Data() + newStartOffset);

        // Add headers and update offsets
        for (const CmdHeader& cmdHeader : other.m_cmdHeaders)
        {
            CmdHeader& newCmdHeader = m_cmdHeaders.PushBack(cmdHeader);
            newCmdHeader.dataOffset = newStartOffset + cmdHeader.dataOffset;
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
    // Call when buffer is resized to ensure proper move of resources into new memory locations
    static void ReconstructCommands(Span<CmdHeader> cmdHeaders, void* prevPtr, void* newPtr)
    {
        const uintptr_t uPrevPtr = reinterpret_cast<uintptr_t>(prevPtr);
        const uintptr_t uNewPtr = reinterpret_cast<uintptr_t>(newPtr);

        for (CmdHeader& cmdHeader : cmdHeaders)
        {
            cmdHeader.moveFnPtr(reinterpret_cast<CmdBase*>(uPrevPtr + cmdHeader.dataOffset), reinterpret_cast<void*>(uNewPtr + cmdHeader.dataOffset));
        }
    }

    Array<CmdHeader> m_cmdHeaders;
    ByteBuffer m_buffer;
    uint32 m_offset;
};

} // namespace hyperion
