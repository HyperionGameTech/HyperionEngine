/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/CommandRecorderAllocator.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Framework/DeviceDetails.hpp>

#ifdef HYP_VULKAN
#include <Rendering/Vulkan/VulkanStructs.hpp>
#elif defined(HYP_DX12)
#include <Rendering/DX12/DX12Structs.hpp>
#endif

namespace Hyperion {

class Entity;
class GpuBufferHolderMap;
class PlaceholderData;
class RenderProxyList;
class View;
class DrawCallCollection;
class DescriptorSetLayout;
class PassBase;
class SkyProbe;
class UIPass;
class Material;
class MaterialTextureCache;
class GraphicsPipelineCache;
class ComputePipelineCache;
class RayTracingPipelineCache;
class BindlessStorage;
class RenderCollector;
struct WorldShaderData;
struct Viewport;
class FinalPass;
class World;
class DescriptorSetCache;
struct ShaderInputGroup;
class Texture;
class ApplicationWindow;
class SingleTimeCommands;
class Shader;
class GpuBufferHolderBase;
class StagingBufferPool;
class ShaderManager;
class DeletionQueue;
class BLASCache;
class ShadowMapCache;
class CrashHandler;
class EngineConfig;
class SamplerCache;
struct SamplerDesc;
class RenderGroupCache;
class EngineStatGpuTimer;
class GpuTimerBackendBase;
class CBufferAllocator;
class BufferAllocator;
class ScratchImageAllocator;

struct IRenderProxy;

enum class GpuBufferType : uint8;
enum RenderTargetType : uint8;

namespace Resources {
struct ResourceContainer;
class ResourceBinderBase;

extern ResourceBinderBase* g_meshEntityBinder;
extern ResourceBinderBase* g_meshBinder;
extern ResourceBinderBase* g_cameraBinder;
extern ResourceBinderBase* g_envProbeBinder;
extern ResourceBinderBase* g_reflectionProbeTextureBinder;
extern ResourceBinderBase* g_probeVolumeBinder;
extern ResourceBinderBase* g_lightBinder;
extern ResourceBinderBase* g_lightmapVolumeBinder;
extern ResourceBinderBase* g_particleVolumeBinder;
extern ResourceBinderBase* g_materialBinder;
extern ResourceBinderBase* g_textureBinder;
extern ResourceBinderBase* g_skeletonBinder;
} // namespace Resources

/*! \brief Get the current ring buffer index for the current thread (can be called from the game or render threads).
 *  \note This is thread-safe only if called from the game or render thread. Other threads should not call this function. */
ENGINE_API uint32 GetRingIndex();

/*! \brief Get the global frame counter value that is incremented every frame.
 *  This is used to track the number of frames that have been rendered.
 *  \note This is thread-safe and can be called from any thread as the frame counter is atomic. */
uint32 GetFrameCounter();

/*! \brief Get the current render thread index (valid indices starting at 1) - usable from Render thread or renderer worker threads.
 *  For the main render thread, this index would be 0. For worker threads, it would be 1,2... so on and so forth.
 *  Undefined for sim thread or other threads than the render thread or renderer worker threads. */
uint32 CurrentRenderThreadIndex();

void BeginFrameSim(AtomicFlag* pCancelFlag);
void EndFrameSim();

/*! \brief Get the RenderProxyList for the Sim thread to write to for the current frame, for the given view.
 *  The sim thread adds proxies of entities, lights, envprobes, etc. to this list, which the render thread will
 *  use when rendering the frame.
 *  \note This is only valid to call from the sim thread, or from a task that is initiated by the sim thread. */
RenderProxyList& GetProducerProxyList(View* view);

/*! \brief Get the RenderProxyList for the Render thread to read from for the current frame, for the given view.
 *  \note This is only valid to call from the render thread, or from a task that is initiated by the render thread. */
RenderProxyList& GetConsumerProxyList(View* view);

/*! \brief Get the RenderCollector corresponding to the given View, only usable on the Render thread. */
RenderCollector& GetRenderCollector(View* view);

// Call on render thread or render thread tasks only (consumer threads)
IRenderProxy* GetRenderProxy(const ObjectBase* resource);

/*! \brief Render thread only - update GPU data to match RenderProxy's buffer data for the resource */
void UpdateGpuData(const ObjectBase* resource);

// used on render thread only - set whether the given resource should be forced to rebind on next ApplyUpdates() call
void SetForceRebind(ObjectBase* resource, bool forceRebind = true);

namespace Resources {
// used on render thread only - assigns all render proxy for the given object to the given binding
void SetBinding(ObjectBase* resource, uint32 binding);
// used on render thread only - retrieves the binding set for the given resource (~0u if unset)
uint32 GetBinding(const ObjectBase* resource);
} // namespace Resources

WorldShaderData* GetWorldBufferData();

void CommitActiveWorlds(Span<World*> activeWorlds);
Span<World*> GetActiveWorlds();

Viewport& GetViewport(View* view);

struct NamedPass
{
    enum Name : uint8
    {
        Invalid = UINT8_MAX,

        Deferred = 0,
        UI,
        EnvProbe,
        ProbeVolume,
        ShadowMap,
        ParticleVolume,
        Sprite,
        SSAO,

        Max
    };

    static constexpr const char* StringValues[Max] = {
        "Deferred",
        "UI",
        "EnvProbe",
        "ProbeVolume",
        "ShadowMap",
        "ParticleVolume",
        "Sprite",
        "SSAO",
    };

    NamedPass(Name value)
        : value(value)
    {
    }

    operator uint8() const
    {
        return uint8(value);
    }

    Name value;
};

static constexpr uint8 NumNamedPasses = uint8(NamedPass::Max);

enum PSOType : uint8
{
    PSO_Graphics,
    PSO_Compute,
    PSO_RayTracing
};

struct NamedBuffer
{
    enum : uint8
    {
        Invalid = UINT8_MAX,

        Worlds = 0,
        Cameras,
        Lights,
        Entities,
        Materials,
        Skeletons,
        EnvProbes,
        ProbeVolumes,
        LightmapVolumes,

        Max
    };

    static constexpr const char* StringValues[Max] = {
        "Worlds",
        "Cameras",
        "Lights",
        "Entities",
        "Materials",
        "Skeletons",
        "EnvProbes",
        "ProbeVolumes",
        "LightmapVolumes"
    };
};

static constexpr uint8 NumNamedBuffers = NamedBuffer::Max;

class RenderInterface
{
public:
    struct State
    {
        static constexpr uint32 MaxShaderUniforms = 32;
        static constexpr uint32 MaxBoundDescriptorSets = 8;

        RenderableAttributeSet attributes;
        Viewport viewport;

        ShaderUniform shaderUniforms[MaxShaderUniforms] {};
        uint32 validUniforms = 0;
        uint32 dirtyUniforms = 0;

        uint32 shaderUniformBufferOffsets[MaxShaderUniforms] {};
        uint32 shaderUniformBufferStrides[MaxShaderUniforms] {};

        uint32 dirtyBufferOffsets = 0;

        DescriptorSet* prevBoundDescriptorSets[MaxBoundDescriptorSets] {};

        uint8 stencilReference = 0;
        uint8 stencilCompareMask = 0xFF;
        uint8 stencilWriteMask = 0x0;

        union
        {
            GraphicsPipeline* boundGraphicsPipeline = nullptr;
            ComputePipeline* boundComputePipeline;
            RayTracingPipeline* boundRayTracingPipeline;
        };

        ShaderDesc boundShaderDesc;

        Framebuffer* framebuffer = nullptr;
        Framebuffer* boundFramebuffer = nullptr;

        PSOType boundPsoType = PSO_Graphics;

        void Reset()
        {
            AssertDebug(boundFramebuffer == nullptr);

            attributes = {};

            validUniforms = 0;
            dirtyUniforms = 0;
            dirtyBufferOffsets = 0;

            Memory::Fill(prevBoundDescriptorSets, 0, sizeof(prevBoundDescriptorSets));

            stencilReference = 0;
            stencilCompareMask = 0xFF;
            stencilWriteMask = 0x0;

            framebuffer = nullptr;
            boundFramebuffer = nullptr;

            boundPsoType = PSO_Graphics;

            boundShaderDesc = ShaderDesc {};

            boundGraphicsPipeline = nullptr;
        }
    };

    RenderInterface();

    RenderInterface(const RenderInterface& other) = delete;
    RenderInterface& operator=(const RenderInterface& other) = delete;

    virtual ~RenderInterface();

    virtual RendererResult Initialize();
    virtual void Shutdown();

    void AddPass(NamedPass passName, PassBase* pass);
    void RemovePass(NamedPass passName, PassBase* pass);

    void FlushStructuredBuffers();

    void CommitDrawState(CommandBuffer* commandBuffer)
    {
        CommitPipelineState(PSO_Graphics, commandBuffer);
    }

    void CommitPipelineState(PSOType psoType, CommandBuffer* commandBuffer);

    virtual const IRenderConfig& GetRenderConfig() const = 0;

    virtual Frame* GetCurrentFrame() const = 0;

    virtual void BeginFrame(AtomicFlag* pCancelFlag);
    virtual void EndFrame();

    virtual void WriteCommandBuffer();

    virtual void RecordStartTimestamp(CommandBuffer* cmd, EngineStatGpuTimer* timer) = 0;
    virtual void RecordStopTimestamp(CommandBuffer* cmd, EngineStatGpuTimer* timer) = 0;
    virtual void ResolveGpuFrameResults(uint32 completedFrameIndex) = 0;

    virtual SwapchainRef CreateSwapchain(ApplicationWindow* window, const Vec2u& extent) = 0;

    virtual void PrepareSwapchain(Swapchain* swapchain) = 0;
    virtual void PresentToSwapchain(Swapchain* swapchain) = 0;

    virtual CommandBuffer* GetCurrentCommandBuffer() const = 0;

    virtual CommandBuffer& GetTransientCommandBuffer() = 0;
    virtual void SubmitTransientCommandBuffer(CommandBuffer& commandBuffer) = 0;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) = 0;

    virtual DescriptorTableRef MakeDescriptorTable(const ShaderInputGroup* decl) = 0;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderInstanceRef& shader,
        const FramebufferDesc& framebufferDesc,
        const RenderableAttributeSet& attributes,
        uint8 stencilWriteMask,
        uint8 stencilCompareMask) = 0;

    virtual ComputePipelineRef MakeComputePipeline(const ShaderInstanceRef& shader) = 0;

    virtual RayTracingPipelineRef MakeRayTracingPipeline(const ShaderInstanceRef& shader) = 0;

    virtual GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment = 0) = 0;

    virtual GpuImageRef MakeImage(const TextureDesc& textureDesc) = 0;

    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image) = 0;
    virtual GpuImageViewRef MakeImageView(
        const GpuImageRef& image,
        uint8 mipIndex,
        uint8 numMips,
        uint16 layerIndex,
        uint16 numLayers,
        TextureType viewType = TextureType::Max) = 0;

    virtual SamplerRef MakeSampler(const SamplerDesc& samplerDesc) = 0;

    virtual FramebufferRef MakeFramebuffer(const FramebufferDesc& framebufferDesc) = 0;

    virtual FrameRef MakeFrame(uint32 frameIndex) = 0;

    virtual ShaderInstanceRef MakeShader(const Shader* shader) = 0;

    virtual BottomLevelASRef MakeBottomLevelAS(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) = 0;

    virtual TopLevelASRef MakeTLAS() = 0;

    virtual void PopulateIndirectDrawCommandsBuffer(
        const GpuBuffer* vertexBuffer,
        const GpuBuffer* indexBuffer,
        uint32 instanceOffset,
        Array<IndirectDrawCommand, RHIAllocator>& outBuffer) = 0;

    virtual bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const = 0;
    virtual TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const = 0;

    virtual UniquePtr<SingleTimeCommands> GetSingleTimeCommands() = 0;

    virtual AsyncCompute* CreateAsyncCompute() = 0;
    virtual void SubmitAsyncCompute(AsyncCompute* asyncCompute) = 0;

    void DeferFlushBuffer(RawBuffer* buffer);

    ShaderManager* shaderManager;

    BindlessStorage* bindlessStorage;

    PlaceholderData* placeholderData;

    GpuBufferHolderMap* gpuBufferHolders;

    CBufferAllocator* cbufferAllocator;
    BufferAllocator* bufferAllocator;
    ScratchImageAllocator* scratchImageAllocator;

    DescriptorTableRef globalDescriptorTable;

    Array<PassBase*, RenderAllocator> namedPasses[NumNamedPasses];

    StructuredBuffer namedBuffers[NumNamedBuffers];

    StructuredBuffer blueNoiseBuffer;
    StructuredBuffer sphereSamplesBuffer;

    Handle<Texture> envProbesColorTexture;
    Handle<Texture> envProbesDepthTexture;

    MaterialTextureCache* materialTextureCache;

    GraphicsPipelineCache* graphicsPipelineCache;
    ComputePipelineCache* computePipelineCache;
    RayTracingPipelineCache* rayTracingPipelineCache;

    DescriptorSetCache* descriptorSetCache;

    RenderGroupCache* renderGroupCache;

    TextureViewCache* textureViewCache;

    SamplerCache* samplerCache;

    BLASCache* blasCache;

    ShadowMapCache* shadowMapCache;

    FinalPass* finalPass;

    Span<World*> worldsToRender;

    State state;

    StagingBufferPool* stagingBufferPool;

    CommandRecorderAllocator commandRecorderAllocator;

    Array<RawBuffer*, RenderAllocator> deferredFlushBuffers;

    DeviceDetails deviceDetails;

    Resources::ResourceContainer* resources;

protected:
    virtual void NewFrameIndex()
    {
    }
    virtual void PrepareFrame(Frame* frame) = 0;

    virtual void ReleaseTransientMemory() = 0;

    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();

    void CreateEnvProbesColorTexture();
    void CreateEnvProbesDepthTexture();

    GpuTimerBackendBase* m_gpuTimerBackend;

private:
    virtual void InitDeviceDetails(DeviceDetails& deviceDetails) = 0;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12RenderInterface.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
