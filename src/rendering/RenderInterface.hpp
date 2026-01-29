/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>

#include <core/reflection/Handle.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/RenderConfig.hpp>

namespace Hyperion {

class Entity;
class ShadowMapAllocator;
class GpuBufferHolderMap;
class PlaceholderData;
class RenderProxyList;
class View;
class DrawCallCollection;
class DescriptorSetLayout;
class RendererBase;
class IRenderProxy;
class EnvProbeRenderer;
class EnvProbe;
class ReflectionProbe;
class SkyProbe;
class RenderGlobalState;
class RenderResourceLock;
class UIRenderer;
class Material;
class MaterialTextureCache;
class GraphicsPipelineCache;
class ComputePipelineCache;
class RayTracingPipelineCache;
class BindlessStorage;
class RenderCollector;
struct WorldShaderData;
struct RenderStats;
struct RenderStatsCounts;
struct Viewport;
class FinalPass;
class ResourceBinderBase;
class World;
class ConstantsAllocator;
class RenderGroup;
class DescriptorSetCache;
struct DescriptorTableDeclaration;
class Texture;
class ApplicationWindow;
class SingleTimeCommands;
struct CompiledShader;

enum class GpuBufferType : uint8;
enum RenderTargetType : uint8;


extern ResourceBinderBase* g_meshEntityBinder;
extern ResourceBinderBase* g_meshBinder;
extern ResourceBinderBase* g_cameraBinder;
extern ResourceBinderBase* g_envProbeBinder;
extern ResourceBinderBase* g_reflectionProbeTextureBinder;
extern ResourceBinderBase* g_envGridBinder;
extern ResourceBinderBase* g_lightBinder;
extern ResourceBinderBase* g_lightmapVolumeBinder;
extern ResourceBinderBase* g_particleVolumeBinder;
extern ResourceBinderBase* g_materialBinder;
extern ResourceBinderBase* g_textureBinder;
extern ResourceBinderBase* g_skeletonBinder;

/*! \brief Get the current ring buffer index for the current thread (can be called from the game or render threads).
 *  \note This is thread-safe only if called from the game or render thread. Other threads should not call this function. */
HYP_API uint32 GetRingIndex();

/*! \brief Get the global frame counter value that is incremented every frame.
 *  This is used to track the number of frames that have been rendered.
 *  \note This is thread-safe and can be called from any thread as the frame counter is atomic. */
HYP_API uint32 GetFrameCounter();

void BeginFrameSim();
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
/*! For debugging: Get all active render collectors for the current frame. Only usable on the Render thread. */
Array<Pair<View*, RenderCollector*>> GetAllRenderCollectors();

// Call on render thread or render thread tasks only (consumer threads)
IRenderProxy* GetRenderProxy(const ObjectBase* resource);

/*! \brief Render thread only - update GPU data to match RenderProxy's buffer data for the resource */
void UpdateGpuData(const ObjectBase* resource);

// used on render thread only - assigns all render proxy for the given object to the given binding
void AssignResourceBinding(ObjectBase* resource, uint32 binding);
// used on render thread only - retrieves the binding set for the given resource (~0u if unset)
uint32 RetrieveResourceBinding(const ObjectBase* resource);
// used on render thread only - set whether the given resource should be forced to rebind on next ApplyUpdates() call
void SetForceRebind(ObjectBase* resource, bool forceRebind = true);

WorldShaderData* GetWorldBufferData();

Viewport& GetViewport(View* view);

HYP_ENUM()
enum GlobalRenderBuffer : uint8
{
    GRB_INVALID = UINT8_MAX,

    GRB_WORLDS = 0,
    GRB_CAMERAS,
    GRB_LIGHTS,
    GRB_ENTITIES,
    GRB_MATERIALS,
    GRB_SKELETONS,
    GRB_ENV_PROBES,
    GRB_ENV_GRIDS,
    GRB_LIGHTMAP_VOLUMES,

    GRB_MAX
};

HYP_ENUM()
enum GlobalRendererType : uint32
{
    GRT_NONE = ~0u, //!< Not a global renderer type

    GRT_MAIN = 0,        //!< Main world renderer (DeferredRenderer)
    GRT_UI,              //!< Globally registered UIRenderer instances to be used by FinalPass to draw the UI onto the backbuffer.
    GRT_ENV_PROBE,       //!< Global renderer instances for different EnvProbe classes
    GRT_ENV_GRID,        //!< Global renderer instance for EnvGrids
    GRT_SHADOW_MAP,      //!< Shadow map renderers, e.g. PointLightShadowRenderer, DirectionalLightShadowRenderer
    GRT_PARTICLE_VOLUME, //!< Global renderer instance for ParticleVolumes

    GRT_MAX
};

enum PSOType : uint8
{
    PSO_Graphics,
    PSO_Compute,
    PSO_RayTracing
};

struct GlobalGpuBuffers
{
    GpuBufferHolderBase* buffers[GRB_MAX];

    HYP_FORCE_INLINE GpuBufferHolderBase* operator[](GlobalRenderBuffer buf) const
    {
        return buffers[buf];
    }
};

class RenderInterface
{
    friend class ResourceBinderBase;

public:
    struct State
    {
        static constexpr uint32 MaxShaderUniforms = 32;
        static constexpr uint32 MaxBoundDescriptorSets = 8;

        RenderableAttributeSet attributes;
        Viewport viewport;
        RenderTargetDesc renderTargetDesc;

        ShaderUniform shaderUniforms[MaxShaderUniforms] {};
        uint32 validUniforms = 0;
        uint32 dirtyUniforms = 0;
        
        uint32 shaderUniformBufferOffsets[MaxShaderUniforms] {};
        uint32 dirtyBufferOffsets = 0;

        DescriptorSet* prevBoundDescriptorSets[MaxBoundDescriptorSets] {};

        uint8 stencilReference = 0;
        uint8 stencilCompareMask = 0xFF;
        uint8 stencilWriteMask = 0x0;
        
        union
        {
            GraphicsPipeline* prevGraphicsPipeline = nullptr;
            ComputePipeline* prevComputePipeline;
            RayTracingPipeline* prevRayTracingPipeline;
        };

        PSOType prevPsoType = PSO_Graphics;
        
        void Reset()
        {
            attributes = {};
            validUniforms = 0;
            dirtyUniforms = 0;
            dirtyBufferOffsets = 0;
            renderTargetDesc = {};
            
            Memory::Fill(prevBoundDescriptorSets, 0, sizeof(prevBoundDescriptorSets));

            stencilReference = 0;
            stencilCompareMask = 0xFF;
            stencilWriteMask = 0x0;
            
            prevPsoType = PSO_Graphics;
            prevGraphicsPipeline = nullptr;
        }
    };

    RenderInterface();
    
    RenderInterface(const RenderInterface& other) = delete;
    RenderInterface& operator=(const RenderInterface& other) = delete;

    virtual ~RenderInterface();
    
    virtual RendererResult Initialize();
    virtual RendererResult Shutdown();

    void AddRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);
    void RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);

    void UpdateBuffers(Frame* frame);

    void CommitDrawState(CommandBuffer* commandBuffer)
    {
        CommitPipelineState(PSO_Graphics, commandBuffer);
    }
    
    void CommitPipelineState(PSOType psoType, CommandBuffer* commandBuffer);

    virtual const IRenderConfig& GetRenderConfig() const = 0;

    virtual Frame* GetCurrentFrame() const = 0;

    virtual Frame* PrepareNextFrame() = 0;

    virtual void BeginFrame();
    virtual void EndFrame();

    virtual SwapchainRef CreateSwapchain(ApplicationWindow* window) = 0;

    virtual void PrepareSwapchain(Swapchain* swapchain) = 0;
    virtual void SubmitCommandBuffers(Swapchain* swapchain) = 0;
    virtual void PresentToSwapchain(Swapchain* swapchain) = 0;

    virtual CommandBuffer* GetCurrentCommandBuffer() const = 0;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) = 0;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) = 0;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef& shader,
        const RenderTargetDesc& renderTargetDesc,
        const RenderableAttributeSet& attributes) = 0;

    virtual ComputePipelineRef MakeComputePipeline(const ShaderRef& shader) = 0;

    virtual RayTracingPipelineRef MakeRayTracingPipeline(const ShaderRef& shader) = 0;

    virtual GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment = 0) = 0;

    virtual GpuImageRef MakeImage(const TextureDesc& textureDesc) = 0;

    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image) = 0;
    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers) = 0;

    virtual SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) = 0;

    virtual FramebufferRef MakeFramebuffer(const RenderTargetDesc& renderTargetDesc) = 0;

    virtual FrameRef MakeFrame(uint32 frameIndex) = 0;

    virtual ShaderRef MakeShader(const CompiledShader* compiledShader) = 0;

    virtual GpuBlasRef MakeGpuBlas(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) = 0;

    virtual GpuTlasRef MakeTLAS() = 0;

    virtual void PopulateIndirectDrawCommandsBuffer(
        const GpuBufferRef& vertexBuffer,
        const GpuBufferRef& indexBuffer,
        uint32 instanceOffset,
        TByteBuffer<RenderAllocator>& outByteBuffer) = 0;

    virtual TextureFormat GetDefaultFormat(DefaultImageFormat type) const = 0;

    virtual bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const = 0;
    virtual TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const = 0;

    virtual UniquePtr<SingleTimeCommands> GetSingleTimeCommands() = 0;

    virtual AsyncCompute* CreateAsyncCompute() = 0;
    virtual void SubmitAsyncCompute(AsyncCompute* asyncCompute) = 0;

    virtual void ReleaseTransientMemory() = 0;
    virtual void NextFrame() = 0;

    BindlessStorage* bindlessStorage;

    ShadowMapAllocator* shadowMapAllocator;
    PlaceholderData* placeholderData;

    GpuBufferHolderMap* gpuBufferHolders;
    ConstantsAllocator* constantsAllocator;

    DescriptorTableRef globalDescriptorTable;

    Array<RendererBase*> globalRenderers[GRT_MAX];

    GlobalGpuBuffers gpuBuffers;
    
    GpuBufferRef blueNoiseBuffer;
    GpuBufferRef sphereSamplesBuffer;

    Handle<Texture> envProbesTexture;

    MaterialTextureCache* materialTextureCache;

    GraphicsPipelineCache* graphicsPipelineCache;
    ComputePipelineCache* computePipelineCache;
    RayTracingPipelineCache* rayTracingPipelineCache;

    FinalPass* finalPass;

    TextureViewCache* textureViewCache;

    Array<World*> renderWorlds[RingBufferDepth];

    State state;
    
    DescriptorSetCache* descriptorSetCache;

    struct ResourceContainer* resources;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();
    void CreateEnvProbesTexture();
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12RenderInterface.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif