/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#if !HYP_VULKAN && !HYP_DX12
#error Rendering backend undefined
#endif // !HYP_VULKAN && !HYP_DX12

#include <Rendering/Framebuffer.hpp>
#include <Rendering/CommandBuffer.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/Vertex.hpp>
#include <Rendering/Shared.hpp>

// Uncomment to enable trace collection for commands.
// #define HYP_RHI_COMMAND_STACK_TRACE

#ifdef HYP_RHI_COMMAND_STACK_TRACE
#include <Core/Debug/StackDump.hpp>
#endif

#include <Core/Threading/AtomicFlag.hpp>

#include <Core/Types.hpp>

#include <cstring>
#include <type_traits>

namespace Hyperion {

class CmdBase;
class View;

class StructuredBuffer;
class RWStructuredBuffer;
class ByteAddressBuffer;
class EngineStatGpuTimer;
class GpuTimerBackendBase;

enum class CommandType : uint8
{
    BindVertexBuffer = 0,
    BindIndexBuffer,
    DrawIndexed,
    DrawIndexedIndirect,
    DrawQuad,
    SetCurrentFramebuffer,
    ClearFramebuffer,
    BindGraphicsPipeline,
    BindComputePipeline,
    BindRayTracingPipeline,
    BindDescriptorSet,
    InsertBarrier,
    InsertUAVBarrier,
    Blit,
    CopyImage,
    FillImage,
    CopyImageToBuffer,
    CopyBufferToImage,
    CopyBuffer,
    GenerateMipmaps,
    DispatchCompute,
    TraceRays,
    SetAsyncShaderLoadingEnabled,
    SetStencilState,
    SetCurrentShader,
    SetCurrentViewport,
    SetTopology,
    SetInputLayout,
    SetCurrentBlendFunction,
    SetDepthWrite,
    SetDepthTest,
    SetDepthCompareOp,
    SetDepthBias,
    SetDepthClamp,
    SetStencilTest,
    SetStencilFunction,
    SetFillMode,
    SetFaceCullMode,
    SetShaderUniform,
    SetShaderUniforms,
    RecordGpuTimestamp,
    CommitDrawState,

    Custom = 0xFFu
};

template <typename T, typename = void>
struct HasCommandType : std::false_type
{
};

template <typename T>
struct HasCommandType<T, std::void_t<decltype(T::ThisCommandType)>> : std::true_type
{
};

template <typename T>
inline constexpr bool HasCommandTypeV = HasCommandType<T>::value;

class alignas(void*) CmdBase
{
public:
#ifdef HYP_RHI_COMMAND_STACK_TRACE
    RawStackTrace trace;
#else
    CmdBase() = default;
#endif
};

class BindVertexBuffer final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    BindVertexBuffer(GpuBuffer* buffer)
        : m_buffer(buffer)
    {
        Assert(buffer);
    }

    static constexpr CommandType ThisCommandType = CommandType::BindVertexBuffer;

private:
    GpuBuffer* m_buffer;
};

class BindIndexBuffer final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    BindIndexBuffer(GpuBuffer* buffer)
        : m_buffer(buffer)
    {
        Assert(buffer);
    }

    static constexpr CommandType ThisCommandType = CommandType::BindIndexBuffer;

private:
    GpuBuffer* m_buffer;
};

class DrawIndexed final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    DrawIndexed(uint32 numIndices, uint32 numInstances = 1, uint32 instanceIndex = 0)
        : m_numIndices(numIndices),
          m_numInstances(numInstances),
          m_instanceIndex(instanceIndex)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::DrawIndexed;

private:
    uint32 m_numIndices;
    uint32 m_numInstances;
    uint32 m_instanceIndex;
};

class DrawIndexedIndirect final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    DrawIndexedIndirect(GpuBuffer* buffer, uint32 bufferOffset)
        : m_buffer(buffer),
          m_bufferOffset(bufferOffset)
    {
        Assert(buffer);
    }

    static constexpr CommandType ThisCommandType = CommandType::DrawIndexedIndirect;

private:
    GpuBuffer* m_buffer;
    uint32 m_bufferOffset;
};

class DrawQuad final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    static constexpr CommandType ThisCommandType = CommandType::DrawQuad;
};

class SetCurrentFramebuffer final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetCurrentFramebuffer(Framebuffer* framebuffer)
        : m_framebuffer(framebuffer)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetCurrentFramebuffer;

private:
    Framebuffer* m_framebuffer;
};

class ClearFramebuffer final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

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

    static constexpr CommandType ThisCommandType = CommandType::ClearFramebuffer;

private:
    Framebuffer* framebuffer;
    Rect<uint32> rect;
    uint8 attachmentsMask;
};

class BindGraphicsPipeline final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

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

    static constexpr CommandType ThisCommandType = CommandType::BindGraphicsPipeline;

private:
    GraphicsPipeline* m_pipeline;
    Viewport m_viewport;
};

class BindComputePipeline final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    BindComputePipeline(ComputePipeline* pipeline)
        : m_pipeline(pipeline)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::BindComputePipeline;

private:
    ComputePipeline* m_pipeline;
};

class BindRayTracingPipeline final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    BindRayTracingPipeline(RayTracingPipeline* pipeline)
        : m_pipeline(pipeline)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::BindRayTracingPipeline;

private:
    RayTracingPipeline* m_pipeline;
};

class BindDescriptorSet final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);
    BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);
    BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets = {});
    BindDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex);

    static constexpr CommandType ThisCommandType = CommandType::BindDescriptorSet;

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
    template <class>
    friend class TCommandRecorder;

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
    void CheckNotInRenderPass(CommandBuffer* commandBuffer) const;
#endif

    static constexpr CommandType ThisCommandType = CommandType::InsertBarrier;

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

class InsertUAVBarrier final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit InsertUAVBarrier(GpuImage* image)
        : m_image(image)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::InsertUAVBarrier;

private:
    GpuImage* m_image;
};

class Blit final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    Blit(Texture* src, Texture* dst)
        : m_src(src),
          m_dst(dst),
          m_hasSubResource(false),
          m_hasRect(false)
    {
    }

    Blit(Texture* src, Texture* dst,
         const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_src(src),
          m_dst(dst),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_hasSubResource(false),
          m_hasRect(true)
    {
    }

    Blit(Texture* src, Texture* dst,
         const Rect<uint32>& srcRect, const Rect<uint32>& dstRect,
         const ImageSubResource& srcSubResource, const ImageSubResource& dstSubResource)
        : m_src(src),
          m_dst(dst),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_srcSubResource(srcSubResource),
          m_dstSubResource(dstSubResource),
          m_hasSubResource(true),
          m_hasRect(true)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::Blit;

private:
    Texture* m_src;
    Texture* m_dst;

    ImageSubResource m_srcSubResource;
    ImageSubResource m_dstSubResource;

    Rect<uint32> m_srcRect;
    Rect<uint32> m_dstRect;

    bool m_hasSubResource : 1;
    bool m_hasRect : 1;
};

class CopyImage final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    static constexpr CommandType ThisCommandType = CommandType::CopyImage;

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

private:
    GpuImage* srcImage;
    GpuImage* dstImage;

    Vec3u srcOffset;
    Vec3u dstOffset;

    Vec3u extent;

    ImageSubResource srcSubResource;
    ImageSubResource dstSubResource;
};

class FillImage final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    FillImage(GpuImage* image, float value);
    FillImage(GpuImage* image, float value, const ImageSubResource& subResource);
    FillImage(GpuImage* image, float value, const ImageSubResource& subResource, const Vec3u& offset, const Vec3u& extent);

    static constexpr CommandType ThisCommandType = CommandType::FillImage;

private:
    GpuImage* m_image;
    float m_value;
    ImageSubResource m_subResource;
    Vec3u m_offset;
    Vec3u m_extent;
};

class CopyImageToBuffer final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

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

    static constexpr CommandType ThisCommandType = CommandType::CopyImageToBuffer;

private:
    GpuImage* m_image;
    GpuBuffer* m_buffer;
    ImageSubResource m_subResource;
};

class CopyBufferToImage final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    static constexpr CommandType ThisCommandType = CommandType::CopyBufferToImage;

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
    template <class>
    friend class TCommandRecorder;

    static constexpr CommandType ThisCommandType = CommandType::CopyBuffer;

    CopyBuffer(GpuBuffer* srcBuffer, GpuBuffer* dstBuffer, uint32 count)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_srcOffset(0),
          m_dstOffset(0),
          m_count(count)
    {
        AssertDebug(m_srcBuffer && m_dstBuffer);
        AssertDebug(m_srcOffset + m_count <= m_srcBuffer->Size(), "Source buffer copy range out of bounds: {}", m_srcOffset + m_count);
        AssertDebug(m_dstOffset + m_count <= m_dstBuffer->Size(), "Destination buffer copy range out of bounds {}", m_dstOffset + m_count);
    }

    CopyBuffer(GpuBuffer* srcBuffer, GpuBuffer* dstBuffer, uint32 srcOffset, uint32 dstOffset, uint32 count)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_srcOffset(srcOffset),
          m_dstOffset(dstOffset),
          m_count(count)
    {
        AssertDebug(m_srcBuffer && m_dstBuffer);
        AssertDebug(m_srcOffset + m_count <= m_srcBuffer->Size(), "Source buffer copy range out of bounds: {}", m_srcOffset + m_count);
        AssertDebug(m_dstOffset + m_count <= m_dstBuffer->Size(), "Destination buffer copy range out of bounds {}", m_dstOffset + m_count);
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
    template <class>
    friend class TCommandRecorder;

    GenerateMipmaps(Texture* inTexture);

    static constexpr CommandType ThisCommandType = CommandType::GenerateMipmaps;

    Texture* inTexture;
};

class DispatchCompute final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

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

    static constexpr CommandType ThisCommandType = CommandType::DispatchCompute;

private:
    ComputePipeline* m_pipeline;
    Vec3u m_workgroupCount;
};

class TraceRays final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

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

    static constexpr CommandType ThisCommandType = CommandType::TraceRays;

private:
    RayTracingPipeline* m_pipeline;
    Vec3u m_workgroupCount;
};

class SetAsyncShaderLoadingEnabled final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetAsyncShaderLoadingEnabled(bool enabled)
        : enabled(enabled)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetAsyncShaderLoadingEnabled;

private:
    bool enabled;
};

class SetStencilState final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetStencilState(uint8 referenceValue, uint8 compareMask = 0xFF, uint8 writeMask = 0xFF)
        : m_referenceValue(referenceValue),
          m_compareMask(compareMask),
          m_writeMask(writeMask)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetStencilState;

private:
    uint8 m_referenceValue;
    uint8 m_compareMask;
    uint8 m_writeMask;
};

class SetCurrentShader final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetCurrentShader(const ShaderDesc& shaderDesc, bool async = true)
        : shaderDesc(shaderDesc),
          async(async)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetCurrentShader;

private:
    ShaderDesc shaderDesc;
    bool async;
};

class SetCurrentViewport final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetCurrentViewport(const Viewport& viewport)
        : viewport(viewport)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetCurrentViewport;

private:
    Viewport viewport;
};

class SetTopology final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetTopology(Topology topology)
        : topology(topology)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetTopology;

private:
    Topology topology;
};

class SetInputLayout final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetInputLayout(const VertexInputLayoutDesc& inputLayout)
        : inputLayout(inputLayout)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetInputLayout;

private:
    VertexInputLayoutDesc inputLayout;
};

class SetCurrentBlendFunction final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetCurrentBlendFunction(const BlendFunction& blendFunction)
        : blendFunction(blendFunction)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetCurrentBlendFunction;

private:
    BlendFunction blendFunction;
};

class SetDepthWrite final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetDepthWrite(bool depthWrite)
        : depthWrite(depthWrite)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetDepthWrite;

private:
    bool depthWrite;
};

class SetDepthTest final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetDepthTest(bool depthTest)
        : depthTest(depthTest)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetDepthTest;

private:
    bool depthTest;
};

class SetDepthCompareOp final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetDepthCompareOp(DepthCompareOp compareOp)
        : compareOp(compareOp)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetDepthCompareOp;

private:
    DepthCompareOp compareOp;
};

class SetDepthBias final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetDepthBias(int depthBias, float depthBiasSlope)
        : depthBias(depthBias),
          depthBiasSlope(depthBiasSlope)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetDepthBias;

private:
    int depthBias;
    float depthBiasSlope;
};

class SetDepthClamp final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetDepthClamp(bool depthClamp)
        : depthClamp(depthClamp)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetDepthClamp;

private:
    bool depthClamp;
};

class SetStencilTest final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetStencilTest(bool stencilTest)
        : stencilTest(stencilTest)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetStencilTest;

private:
    bool stencilTest;
};

class SetStencilFunction final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetStencilFunction(StencilFunction stencilFunction)
        : stencilFunction(stencilFunction)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetStencilFunction;

private:
    StencilFunction stencilFunction;
};

class SetFillMode final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetFillMode(FillMode fillMode)
        : fillMode(fillMode)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetFillMode;

private:
    FillMode fillMode;
};

class SetFaceCullMode final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    explicit SetFaceCullMode(FaceCullMode faceCullMode)
        : faceCullMode(faceCullMode)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetFaceCullMode;

private:
    FaceCullMode faceCullMode;
};

class SetShaderUniform final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

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

    SetShaderUniform(uint32 uniformIndex, StringHash name, const StructuredBuffer& structuredBuffer, uint32 elementOffset = 0);
    SetShaderUniform(uint32 uniformIndex, StringHash name, const RWStructuredBuffer& rwStructuredBuffer, uint32 elementOffset = 0);
    SetShaderUniform(uint32 uniformIndex, StringHash name, const ByteAddressBuffer& byteAddressBuffer, uint32 byteOffset = 0);

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

    SetShaderUniform(uint32 uniformIndex, StringHash name, TopLevelAS* tlas)
        : uniformIndex(uniformIndex),
          uniform { name, tlas }
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetShaderUniform;

    uint32 uniformIndex;
    ShaderUniform uniform;
    ShaderDataOffset shaderDataOffset;
};

class SetShaderUniforms final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    SetShaderUniforms(const ShaderUniforms& shaderUniforms, uint32 startIndex = 0)
        : shaderUniforms(shaderUniforms),
          startIndex(startIndex)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::SetShaderUniforms;

    uint32 startIndex;
    ShaderUniforms shaderUniforms;
};

class RecordGpuTimestamp final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    RecordGpuTimestamp(EngineStatGpuTimer* timer, bool isStart)
        : m_timer(timer),
          m_isStart(isStart)
    {
    }

    static constexpr CommandType ThisCommandType = CommandType::RecordGpuTimestamp;

private:
    EngineStatGpuTimer* m_timer;
    bool m_isStart;
};

class CommitDrawState final : public CmdBase
{
public:
    template <class>
    friend class TCommandRecorder;

    CommitDrawState() = default;

    static constexpr CommandType ThisCommandType = CommandType::CommitDrawState;
};

class CommandRecorderBase
{
protected:
    using InvokeCmdFnPtr = void (*)(CmdBase*, CommandBuffer*);
    using MoveCmdFnPtr = void (*)(CmdBase*, void* where);

    struct CmdHeader
    {
        union
        {
            struct
            {

                uint64 offset : 48;
                uint64 cmd : 8;
            };

            // Function pointers for custom are held in two headers, second one holds the function pointer directly in memory
            uintptr_t address;
        };

        HYP_FORCE_INLINE bool IsCustom() const
        {
            return cmd == static_cast<uint8>(CommandType::Custom);
        }

        HYP_FORCE_INLINE CommandType GetCommandType() const
        {
            return static_cast<CommandType>(cmd);
        }
    };

    CommandRecorderBase()
        : writeCount(1),
          m_writableState(true),
          m_offset(0),
          m_bufferSize(0),
          m_startPtr(nullptr),
          m_headerCount(0),
          m_headerCapacity(0),
          m_headersPtr(nullptr)
    {
    }
    
    ~CommandRecorderBase() = default;

public:
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

        AssertDebug(m_writableState.LoadVolatile());

        // static_assert(std::is_trivially_copyable_v<TCmd> && std::is_trivially_destructible_v<TCmd>,
        //    "CmdType should be trivially copyable and destructible!");

        constexpr size_t CmdSize = sizeof(TCmd);

        const uint32 alignedOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_bufferSize < alignedOffset + CmdSize)
        {
            m_vfTable.ResizeBuffer(this, MathUtil::Ceil<size_t>(1.5 * (alignedOffset + CmdSize)));
        }

        ubyte* startPtr = m_startPtr + alignedOffset;
        new (startPtr) TCmd(std::forward<CmdType>(cmd));
        
        // add +1 in case it needs a payload
        if (m_headerCount + 2 > m_headerCapacity)
        {
            uint32 newCapacity = MathUtil::Max(16u, MathUtil::NextPowerOf2(m_headerCount + 2));

            m_vfTable.ResizeHeaders(this, newCapacity);
        }

        CmdHeader& header = m_headersPtr[m_headerCount++];
        header.offset = alignedOffset;

        if constexpr (HasCommandTypeV<TCmd>)
        {
            header.cmd = static_cast<uint8>(TCmd::ThisCommandType);
        }
        else
        {
            header.cmd = static_cast<uint8>(CommandType::Custom);

            // Store address in the next header 
            CmdHeader& payloadHeader = m_headersPtr[m_headerCount++];

            InvokeCmdFnPtr fnPtr = &TCmd::InvokeStatic;
            payloadHeader.address = reinterpret_cast<uintptr_t>(fnPtr);
        }

        m_offset = alignedOffset + CmdSize;
    }

    template <class CmdType>
    CommandRecorderBase& operator<<(CmdType&& cmd)
    {
        Add(std::forward<CmdType>(cmd));

        return *this;
    }

    void Done()
    {
        if (!--writeCount)
        {
            m_writableState.Release();
        }
    }

    uint32 writeCount;

protected:
    struct VFTable
    {
        void (*ResizeBuffer)(CommandRecorderBase* pThis, size_t newSize);
        void (*ResizeHeaders)(CommandRecorderBase* pThis, uint32 newCapacity);
    } m_vfTable;

    AtomicFlag m_writableState;

    uint32 m_offset;
    size_t m_bufferSize;
    ubyte* m_startPtr;

    size_t m_headerCount;
    size_t m_headerCapacity;
    CmdHeader* m_headersPtr;
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

    TCommandRecorder()
    {
        m_vfTable.ResizeBuffer = [](CommandRecorderBase* pThis, size_t newSize)
        {
            static_cast<TCommandRecorder*>(pThis)->ResizeBuffer(newSize);
        };

        m_vfTable.ResizeHeaders = [](CommandRecorderBase* pThis, uint32 newCapacity)
        {
            static_cast<TCommandRecorder*>(pThis)->ResizeHeaders(newCapacity);
        };
    }

    TCommandRecorder(const TCommandRecorder& other) = delete;
    TCommandRecorder& operator=(const TCommandRecorder& other) = delete;

    TCommandRecorder(TCommandRecorder&& other) noexcept = delete;
    TCommandRecorder& operator=(TCommandRecorder&& other) noexcept = delete;

    ~TCommandRecorder();

    void Reset(bool freeMemory)
    {
        if (freeMemory)
        {
            if (m_headersPtr != nullptr)
            {
                GetDefaultAllocatorInstance<AllocatorType>()->Free(m_headersPtr);
                m_headersPtr = nullptr;

                m_headerCapacity = 0;
            }

            m_buffer.Clear();
            m_startPtr = nullptr;
            m_bufferSize = 0;
        }

        m_headerCount = 0;

        writeCount = 1;

        m_offset = 0;
        m_writableState.Store(true);
    }

    void Reserve(uint32 numCmdHeaders, uint32 bufferSizeBytes = 0)
    {
        if (numCmdHeaders > m_headerCapacity)
        {
            ResizeHeaders(numCmdHeaders);
        }

        if (bufferSizeBytes > 0 && bufferSizeBytes > m_bufferSize)
        {
            ResizeBuffer(ByteUtil::AlignAs(bufferSizeBytes, 16));
        }
    }

    template <class OtherAllocatorType>
    void Concat(TCommandRecorder<OtherAllocatorType>& other)
    {
        if ((void*)&other == (void*)this)
        {
            return;
        }

        other.m_writableState.Acquire();

        if (m_headerCount + other.m_headerCount > m_headerCapacity)
        {
            ResizeHeaders(m_headerCount + other.m_headerCount);
        }

        // since we guarantee <= 16 byte alignment, we should just align our offset to 16 to make sure everything fits
        const uint32 newStartOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_bufferSize < newStartOffset + other.m_offset)
        {
            ResizeBuffer(MathUtil::Ceil<size_t>(1.5 * (newStartOffset + other.m_offset)));
        }

        // Reconstruct the commands into our memory
        Memory::Copy(m_buffer.Data() + newStartOffset, other.m_buffer.Data(), other.m_offset);

        bool isPayloadHeader = false;

        for (uint32 i = 0; i < other.m_headerCount; ++i)
        {
            const CmdHeader& cmdHeader = other.m_headersPtr[i];
            CmdHeader& newCmdHeader = m_headersPtr[m_headerCount++];
            newCmdHeader = cmdHeader;

            if (isPayloadHeader)
            {
                isPayloadHeader = false;
            }
            else
            {
                newCmdHeader.offset += newStartOffset;

                if (cmdHeader.IsCustom())
                {
                    // The next header is the payload holding the function pointer
                    isPayloadHeader = true;
                }
            }
        }

        m_offset = newStartOffset + other.m_offset;
        other.m_headerCount = 0; // Consume the other headers
        other.m_offset = 0;

        // @NOTE: Keep it in write state
    }

    // Non-virtualized Add()
    
    template <class CmdType>
    void Add(CmdType&& cmd)
    {
        using TCmd = NormalizedType<CmdType>;
        static_assert(alignof(TCmd) <= 16, "CmdType should have alignment <= 16!");

        AssertDebug(m_writableState.LoadVolatile());

        // static_assert(std::is_trivially_copyable_v<TCmd> && std::is_trivially_destructible_v<TCmd>,
        //    "CmdType should be trivially copyable and destructible!");

        constexpr size_t CmdSize = sizeof(TCmd);

        const uint32 alignedOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_bufferSize < alignedOffset + CmdSize)
        {
            ResizeBuffer(MathUtil::Ceil<size_t>(1.5 * (alignedOffset + CmdSize)));
        }

        ubyte* startPtr = m_startPtr + alignedOffset;
        new (startPtr) TCmd(std::forward<CmdType>(cmd));

        // add +1 in case it needs a payload
        if (m_headerCount + 2 > m_headerCapacity)
        {
            uint32 newCapacity = MathUtil::Max(16u, MathUtil::NextPowerOf2(m_headerCount + 2));
            ResizeHeaders(newCapacity);
        }

        CmdHeader& header = m_headersPtr[m_headerCount++];
        header.offset = alignedOffset;

        if constexpr (HasCommandTypeV<TCmd>)
        {
            header.cmd = static_cast<uint8>(TCmd::ThisCommandType);
        }
        else
        {
            header.cmd = static_cast<uint8>(CommandType::Custom);

            // Store address in the next header 
            CmdHeader& payloadHeader = m_headersPtr[m_headerCount++];

            InvokeCmdFnPtr fnPtr = &TCmd::InvokeStatic;
            payloadHeader.address = reinterpret_cast<uintptr_t>(fnPtr);
        }

        m_offset = alignedOffset + CmdSize;
    }

    template <class CmdType>
    TCommandRecorder& operator<<(CmdType&& cmd)
    {
        Add(std::forward<CmdType>(cmd));

        return *this;
    }

    void Execute(CommandBuffer* commandBuffer);

    void Submit();

private:
    void ResizeBuffer(size_t newSize)
    {
        m_buffer.SetSize(ByteUtil::AlignAs(newSize, 16));
        m_bufferSize = m_buffer.Size();
        m_startPtr = m_buffer.Data();
    }

    void ResizeHeaders(uint32 newCapacity)
    {
        auto* allocator = GetDefaultAllocatorInstance<AllocatorType>();

        CmdHeader* prevPtr = m_headersPtr;

        if (newCapacity != 0)
        {
            m_headersPtr = (CmdHeader*)allocator->Allocate(newCapacity * sizeof(CmdHeader), alignof(CmdHeader));

            m_headerCount = newCapacity < m_headerCount ? newCapacity : m_headerCount;

            if (m_headersPtr && prevPtr && m_headerCount != 0)
            {
                Memory::Copy(m_headersPtr, prevPtr, m_headerCount * sizeof(CmdHeader));
            }
        }
        else
        {
            m_headersPtr = nullptr;
        }

        if (prevPtr != nullptr)
        {
            allocator->Free(prevPtr);
        }

        m_headerCapacity = m_headersPtr ? newCapacity : 0;
    }

    memory::ByteBuffer<AllocatorType> m_buffer;
};

template <class AllocatorType>
TCommandRecorder<AllocatorType>::~TCommandRecorder()
{
    Assert(m_headerCount == 0, "CommandRecorder destroyed with pending commands!");

    if (m_headersPtr != nullptr)
    {
        GetDefaultAllocatorInstance<AllocatorType>()->Free(m_headersPtr);
        m_headersPtr = nullptr;
    }
}

using CommandRecorder = TCommandRecorder<RenderAllocator>;

} // namespace Hyperion
