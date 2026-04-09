/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#if !HYP_VULKAN && !HYP_DX12
#error Rendering backend undefined
#endif // !HYP_VULKAN && !HYP_DX12

#include <rendering/GpuBuffer.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/CommandBuffer.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/Vertex.hpp>
#include <rendering/Shared.hpp>

// Uncomment to enable trace collection for commands.
// #define HYP_RHI_COMMAND_STACK_TRACE

#ifdef HYP_RHI_COMMAND_STACK_TRACE
#include <Core/debug/StackDump.hpp>
#endif

#include <Core/threading/AtomicFlag.hpp>

namespace Hyperion {

class CmdBase;
class View;

class alignas(void*) CmdBase
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
        Assert(buffer && buffer->IsCreated());
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
        Assert(buffer && buffer->IsCreated());
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
        Assert(buffer != nullptr && buffer->IsCreated());
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        DrawIndexedIndirect* cmdCasted = static_cast<DrawIndexedIndirect*>(cmd);

#if HYP_VULKAN
        AssertDebug(cmdCasted->m_bufferOffset + /*sizeof(VkDrawIndexedIndirectCommand)*/ 20 <= cmdCasted->m_buffer->Size());
#endif

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

class SetCurrentFramebuffer final : public CmdBase
{
public:
    SetCurrentFramebuffer(Framebuffer* framebuffer);
    static void PrepareStatic(CmdBase* cmd, Frame* frame);

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    Framebuffer* m_framebuffer;
};

class ClearFramebuffer final : public CmdBase
{
public:
    explicit ClearFramebuffer(Framebuffer* framebuffer, uint8 attachmentsMask = uint8(-1))
        : framebuffer(framebuffer),
          rect {},
          attachmentsMask(attachmentsMask)
    {
    }

    ClearFramebuffer(Framebuffer* framebuffer, const Rect<uint32>& rect, uint8 attachmentsMask = uint8(-1))
        : framebuffer(framebuffer),
          rect(rect),
          attachmentsMask(attachmentsMask)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    Framebuffer* framebuffer;
    Rect<uint32> rect;
    uint8 attachmentsMask;
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

class BindRayTracingPipeline final : public CmdBase
{
public:
    BindRayTracingPipeline(RayTracingPipeline* pipeline)
        : m_pipeline(pipeline)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    RayTracingPipeline* m_pipeline;
};

class BindDescriptorSet final : public CmdBase
{
public:
    BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);
    BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);
    BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);

    static void PrepareStatic(CmdBase* cmd, Frame* frame);
    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    DescriptorSet* m_descriptorSet;
    union
    {
        GraphicsPipeline* m_graphicsPipeline;
        ComputePipeline* m_computePipeline;
        RayTracingPipeline* m_rayTracingPipeline;
    };
    DescriptorSetOffsetMap m_offsets;
    uint32 m_bindIndex;
    uint8 m_pipelineType : 2; // 0 = Graphics, 1 = Compute, 2 = RayTracing
};

class InsertBarrier final : public CmdBase
{
public:
    InsertBarrier(
        GpuBuffer* buffer,
        const ResourceState& state,
        ShaderModuleType shaderModuleType = ShaderModuleType::None)
        : m_buffer(buffer),
          m_image(nullptr),
          m_state(state),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(false),
          m_onlyDepth(false),
          m_onlyStencil(false)
    {
    }

    InsertBarrier(
        GpuImage* image,
        const ResourceState& state,
        ShaderModuleType shaderModuleType = ShaderModuleType::None,
        bool onlyDepth = false,
        bool onlyStencil = false)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(false),
          m_onlyDepth(onlyDepth),
          m_onlyStencil(onlyStencil)
    {
    }

    InsertBarrier(
        GpuImage* image,
        const ResourceState& state,
        const ImageSubResource& subResource,
        ShaderModuleType shaderModuleType = ShaderModuleType::None)
        : m_buffer(nullptr),
          m_image(image),
          m_state(state),
          m_subResource(subResource),
          m_shaderModuleType(shaderModuleType),
          m_hasSubResource(true),
          m_onlyDepth(false),
          m_onlyStencil(false)
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
                cmdCasted->m_image->InsertBarrier(
                    commandBuffer,
                    cmdCasted->m_subResource,
                    cmdCasted->m_state,
                    cmdCasted->m_shaderModuleType,
                    cmdCasted->m_onlyDepth,
                    cmdCasted->m_onlyStencil);
            }
            else
            {
                cmdCasted->m_image->InsertBarrier(
                    commandBuffer,
                    cmdCasted->m_state,
                    cmdCasted->m_shaderModuleType,
                    cmdCasted->m_onlyDepth,
                    cmdCasted->m_onlyStencil);
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
    bool m_onlyDepth : 1;
    bool m_onlyStencil : 1;
};

class Blit final : public CmdBase
{
public:
    Blit(GpuImage* srcImage, GpuImage* dstImage)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_hasSubResource(false),
          m_hasRect(false)
    {
    }

    Blit(GpuImage* srcImage, GpuImage* dstImage,
        const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_hasSubResource(false),
          m_hasRect(true)
    {
    }

    Blit(GpuImage* srcImage, GpuImage* dstImage,
        const Rect<uint32>& srcRect, const Rect<uint32>& dstRect,
        const ImageSubResource& srcSubResource, const ImageSubResource& dstSubResource)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_srcSubResource(srcSubResource),
          m_dstSubResource(dstSubResource),
          m_hasSubResource(true),
          m_hasRect(true)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        Blit* cmdCasted = static_cast<Blit*>(cmd);

        if (cmdCasted->m_hasSubResource)
        {
            cmdCasted->m_dstImage->Blit(
                commandBuffer, 
                cmdCasted->m_srcImage,
                cmdCasted->m_srcRect, cmdCasted->m_dstRect,
                cmdCasted->m_srcSubResource, cmdCasted->m_dstSubResource);
        }
        else
        {
            if (cmdCasted->m_hasRect)
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
    GpuImage* m_srcImage;
    GpuImage* m_dstImage;

    ImageSubResource m_srcSubResource;
    ImageSubResource m_dstSubResource;

    Rect<uint32> m_srcRect;
    Rect<uint32> m_dstRect;

    bool m_hasSubResource : 1;
    bool m_hasRect : 1;
};

class BlitRect final : public CmdBase
{
public:
    HYP_DEPRECATED BlitRect(GpuImage* srcImage, GpuImage* dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        BlitRect* cmdCasted = static_cast<BlitRect*>(cmd);

        cmdCasted->m_dstImage->Blit(
            commandBuffer,
            cmdCasted->m_srcImage,
            cmdCasted->m_srcRect, cmdCasted->m_dstRect);

        static_assert(std::is_trivially_destructible_v<BlitRect>);
        // cmdCasted->~BlitRect();
    }

private:
    GpuImage* m_srcImage;
    GpuImage* m_dstImage;
    Rect<uint32> m_srcRect;
    Rect<uint32> m_dstRect;
};

class CopyImage final : public CmdBase
{
public:
    CopyImage(GpuImage* srcImage, GpuImage* dstImage, const Vec3u& extent)
        : srcImage(srcImage),
          dstImage(dstImage),
          srcOffset(Vec3u::Zero()),
          dstOffset(Vec3u::Zero()),
          extent(extent)
    {
    }

    CopyImage(
        GpuImage* srcImage,
        GpuImage* dstImage,
        const Vec3u& extent,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource)
        : srcImage(srcImage),
          dstImage(dstImage),
          srcOffset(Vec3u::Zero()),
          dstOffset(Vec3u::Zero()),
          extent(extent),
          srcSubResource(srcSubResource),
          dstSubResource(dstSubResource)
    {
    }

    CopyImage(
        GpuImage* srcImage,
        GpuImage* dstImage,
        const Vec3u& srcOffset,
        const Vec3u& dstOffset,
        const Vec3u& extent,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource)
        : srcImage(srcImage),
          dstImage(dstImage),
          srcOffset(srcOffset),
          dstOffset(dstOffset),
          extent(extent),
          srcSubResource(srcSubResource),
          dstSubResource(dstSubResource)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        CopyImage* cmdCasted = static_cast<CopyImage*>(cmd);

        cmdCasted->dstImage->CopyFrom(
            commandBuffer,
            cmdCasted->srcImage,
            cmdCasted->srcOffset,
            cmdCasted->dstOffset,
            cmdCasted->extent,
            cmdCasted->srcSubResource,
            cmdCasted->dstSubResource);

        static_assert(std::is_trivially_destructible_v<CopyImage>);
        // cmdCasted->~CopyImage();
    }

private:
    GpuImage* srcImage;
    GpuImage* dstImage;

    Vec3u srcOffset;
    Vec3u dstOffset;

    Vec3u extent;

    ImageSubResource srcSubResource;
    ImageSubResource dstSubResource;
};

class CopyImageToBuffer final : public CmdBase
{
public:
    CopyImageToBuffer(GpuImage* image, GpuBuffer* buffer)
        : m_image(image),
          m_buffer(buffer)
    {
        // by default, only copy one mip level
        m_subResource = ImageSubResource {};
        m_subResource.baseArrayLayer = 0;
        m_subResource.numLayers = UINT16_MAX;
        m_subResource.baseMipLevel = 0;
        m_subResource.numLevels = 1;
    }
    
    CopyImageToBuffer(GpuImage* image, GpuBuffer* buffer, const ImageSubResource& subResource)
        : m_image(image),
          m_buffer(buffer),
          m_subResource(subResource)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        CopyImageToBuffer* cmdCasted = static_cast<CopyImageToBuffer*>(cmd);

        cmdCasted->m_image->CopyToBuffer(commandBuffer, cmdCasted->m_buffer, cmdCasted->m_subResource);

        static_assert(std::is_trivially_destructible_v<CopyImageToBuffer>);
        // cmdCasted->~CopyImageToBuffer();
    }

private:
    GpuImage* m_image;
    GpuBuffer* m_buffer;
    ImageSubResource m_subResource;
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
    explicit DispatchCompute(Vec3u workgroupCount)
        : m_pipeline(nullptr),
          m_workgroupCount(workgroupCount)
    {
    }

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
    explicit TraceRays(const Vec3u& workgroupCount)
        : m_pipeline(nullptr),
          m_workgroupCount(workgroupCount)
    {
    }

    TraceRays(RayTracingPipeline* pipeline, const Vec3u& workgroupCount)
        : m_pipeline(pipeline),
          m_workgroupCount(workgroupCount)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    RayTracingPipeline* m_pipeline;
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
    explicit SetCurrentShader(const ShaderDesc& shaderDesc)
        : shaderDesc(shaderDesc)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    ShaderDesc shaderDesc;
};

class SetCurrentViewport final : public CmdBase
{
public:
    explicit SetCurrentViewport(const Viewport& viewport)
        : viewport(viewport)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    Viewport viewport;
};

class SetTopology final : public CmdBase
{
public:
    explicit SetTopology(Topology topology)
        : topology(topology)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    Topology topology;
};

class SetInputLayout final : public CmdBase
{
public:
    explicit SetInputLayout(const VertexInputLayoutDesc& inputLayout)
        : inputLayout(inputLayout)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    VertexInputLayoutDesc inputLayout;
};

class SetCurrentBlendFunction final : public CmdBase
{
public:
    explicit SetCurrentBlendFunction(const BlendFunction& blendFunction)
        : blendFunction(blendFunction)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    BlendFunction blendFunction;
};

class SetDepthWrite final : public CmdBase
{
public:
    explicit SetDepthWrite(bool depthWrite)
        : depthWrite(depthWrite)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    bool depthWrite;
};

class SetDepthTest final : public CmdBase
{
public:
    explicit SetDepthTest(bool depthTest)
        : depthTest(depthTest)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    bool depthTest;
};

class SetDepthBias final : public CmdBase
{
public:
    explicit SetDepthBias(int depthBias, float depthBiasSlope)
        : depthBias(depthBias),
          depthBiasSlope(depthBiasSlope)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    int depthBias;
    float depthBiasSlope;
};

class SetDepthClamp final : public CmdBase
{
public:
    explicit SetDepthClamp(bool depthClamp)
        : depthClamp(depthClamp)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    bool depthClamp;
};

class SetStencilTest final : public CmdBase
{
public:
    explicit SetStencilTest(bool stencilTest)
        : stencilTest(stencilTest)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    bool stencilTest;
};

class SetStencilFunction final : public CmdBase
{
public:
    explicit SetStencilFunction(StencilFunction stencilFunction)
        : stencilFunction(stencilFunction)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    StencilFunction stencilFunction;
};

class SetFillMode final : public CmdBase
{
public:
    explicit SetFillMode(FillMode fillMode)
        : fillMode(fillMode)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    FillMode fillMode;
};

class SetFaceCullMode final : public CmdBase
{
public:
    explicit SetFaceCullMode(FaceCullMode faceCullMode)
        : faceCullMode(faceCullMode)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    FaceCullMode faceCullMode;
};

class SetShaderUniform final : public CmdBase
{
public:
    SetShaderUniform(uint32 uniformIndex, const ShaderUniform& uniform)
        : uniformIndex(uniformIndex),
          shaderDataOffset(ShaderDataOffset::Invalid()),
          uniform(uniform)
    {
    }

    SetShaderUniform(uint32 uniformIndex, StringHash name, GpuBuffer* buffer)
        : uniformIndex(uniformIndex),
          shaderDataOffset(ShaderDataOffset::Invalid()),
          uniform { name, buffer }
    {
    }

    SetShaderUniform(uint32 uniformIndex, StringHash name, GpuBuffer* buffer, const ShaderDataOffset& shaderDataOffset)
        : uniformIndex(uniformIndex),
          shaderDataOffset(shaderDataOffset),
          uniform { name, buffer }
    {
    }
    
    SetShaderUniform(uint32 uniformIndex, StringHash name, GpuImageView* imageView)
        : uniformIndex(uniformIndex),
          uniform { name, imageView }
    {
    }
    
    SetShaderUniform(uint32 uniformIndex, StringHash name, Sampler* sampler)
        : uniformIndex(uniformIndex),
          uniform { name, sampler }
    {
    }
    
    SetShaderUniform(uint32 uniformIndex, StringHash name, GpuTlas* tlas)
        : uniformIndex(uniformIndex),
          uniform { name, tlas }
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);

private:
    uint32 uniformIndex;
    ShaderUniform uniform;
    ShaderDataOffset shaderDataOffset;
};

class CommitDrawState final : public CmdBase
{
public:
    CommitDrawState() = default;

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer);
};

class CommandRecorderBase
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

    CommandRecorderBase() = default;
};

template <class AllocatorType>
class TCommandRecorder final : public CommandRecorderBase
{
    template <class OtherAllocatorType>
    friend class TCommandRecorder;

public:
    using Base = CommandRecorderBase;

    using Base::CmdHeader;
    using Base::InvokeCmdFnPtr;
    using Base::MoveCmdFnPtr;
    using Base::PrepareCmdFnPtr;

    TCommandRecorder()
        : m_offset(0),
          m_writableState(true)
    {
    }

    explicit TCommandRecorder(AllocatorType* pAllocator)
        : m_cmdHeaders(pAllocator),
          m_buffer(pAllocator),
          m_offset(0),
          m_writableState(true)
    {
        AssertDebug(pAllocator != nullptr);
    }

    TCommandRecorder(const TCommandRecorder& other) = delete;
    TCommandRecorder& operator=(const TCommandRecorder& other) = delete;

    TCommandRecorder(TCommandRecorder&& other) noexcept = delete;
    TCommandRecorder& operator=(TCommandRecorder&& other) noexcept = delete;

    ~TCommandRecorder();

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_offset == 0;
    }

    HYP_FORCE_INLINE bool IsWritable() const
    {
        return m_writableState.Load();
    }

    template <class CmdType>
    void Add(CmdType&& cmd)
    {
        using TCmd = NormalizedType<CmdType>;
        static_assert(alignof(TCmd) <= 16, "CmdType should have alignment <= 16!");

        //static_assert(std::is_trivially_copyable_v<TCmd> && std::is_trivially_destructible_v<TCmd>,
         //   "CmdType should be trivially copyable and destructible!");

        constexpr size_t CmdSize = sizeof(TCmd);

        const uint32 alignedOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_buffer.Size() < alignedOffset + CmdSize)
        {
            m_buffer.SetSize(MathUtil::Ceil<size_t>(1.5 * (alignedOffset + CmdSize)), /* zeroize */ false);
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
    TCommandRecorder& operator<<(CmdType&& cmd)
    {
        Add(std::forward<CmdType>(cmd));

        return *this;
    }

    template <class OtherAllocatorType>
    void Concat(TCommandRecorder<OtherAllocatorType>& other)
    {
        other.m_writableState.Acquire();

        m_cmdHeaders.Reserve(m_cmdHeaders.Size() + other.m_cmdHeaders.Size());

        // since we guarantee <= 16 byte alignment, we should just align our offset to 16 to make sure everything fits
        const uint32 newStartOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_buffer.GetCapacity() < newStartOffset + other.m_offset)
        {
            m_buffer.SetSize(MathUtil::Ceil<size_t>(1.5 * (newStartOffset + other.m_offset)), /* zeroize */ false);
        }
        else
        {
            ubyte* prevPtr = m_buffer.Data();

            // No need to reconstruct commands if the allocation did not change
            m_buffer.SetSize(newStartOffset + other.m_offset, /* zeroize */ false);

            // Sanity check to ensure SetSize() did not change our capacity. (it shouldn't)
            AssertDebug(m_buffer.Data() == prevPtr);
        }

        size_t cmdsOffset = m_cmdHeaders.Size();

        // Reconstruct the commands into our memory
        Memory::Copy(m_buffer.Data() + newStartOffset, other.m_buffer.Data(), other.m_offset);

        // Add headers and update offsets
        for (const CmdHeader& cmdHeader : other.m_cmdHeaders)
        {
            CmdHeader& newCmdHeader = m_cmdHeaders.PushBack(cmdHeader);
            newCmdHeader.offset += newStartOffset;
        }

        //        // Copy from other buffer, starting at the new offset
        //        m_buffer.Write(other.m_offset, newStartOffset, other.m_buffer.Data());

        m_offset = newStartOffset + other.m_offset;

        other.m_cmdHeaders.Clear();
        other.m_offset = 0;

        // @NOTE: Keep it in write state
    }

    void Prepare(Frame* frame);
    void Execute(CommandBuffer* commandBuffer);

    void Reset(bool freeMemory)
    {
        m_cmdHeaders.Clear();

        if (freeMemory)
        {
            m_cmdHeaders.Refit();
            m_buffer.Clear();
        }

        m_offset = 0;
    }

    void Reserve(uint32 numCmdHeaders, uint32 bufferSizeBytes = 0)
    {
        m_cmdHeaders.Reserve(numCmdHeaders);

        if (bufferSizeBytes > 0 && bufferSizeBytes > m_buffer.Size())
        {
            m_buffer.SetSize(ByteUtil::AlignAs(bufferSizeBytes, 16));
        }
    }

    void Done()
    {
        m_writableState.Release();
    }

private:
    Array<CmdHeader, AllocatorType> m_cmdHeaders;
    TByteBuffer<AllocatorType> m_buffer;
    uint32 m_offset;

    AtomicFlag m_writableState;
};

template <class AllocatorType>
TCommandRecorder<AllocatorType>::~TCommandRecorder()
{
    Assert(m_cmdHeaders.Empty(), "CommandRecorder destroyed with pending commands!");
}

using CommandRecorder = TCommandRecorder<RenderAllocator>;

} // namespace Hyperion
