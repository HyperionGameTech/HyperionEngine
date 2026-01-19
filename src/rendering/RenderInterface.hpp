/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>

#include <core/reflection/Handle.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/GpuBuffer.hpp>

namespace Hyperion {

class Entity;
class ShadowMapAllocator;
class GpuBufferHolderMap;
class PlaceholderData;
class RenderProxyList;
class View;
class DrawCallCollection;
class RendererBase;
class IRenderProxy;
class EnvProbeRenderer;
class EnvProbe;
class ReflectionProbe;
class SkyProbe;
class RenderGlobalState;
class RenderResourceLock;
class UIRenderer;
class MaterialTextureCache;
class GraphicsPipelineCache;
class BindlessStorage;
class RenderCollector;
struct WorldShaderData;
struct RenderStats;
struct RenderStatsCounts;
struct Viewport;
class FinalPass;
class ResourceBinderBase;
class ShaderPropertyCache;
class World;
class ConstantsAllocator;
class RenderGroup;
class DescriptorSetCache;
class Texture;

namespace RenderApi {

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

// Call at start of engine before render / sim thread start ticking.
// Allocates containers declared in RenderGlobalState.cpp via DECLARE_RENDER_DATA_CONTAINER
void Init();
void Shutdown();

/*! \brief Check if rendering subsystem has been initialized. Thread safe. */
bool IsInit();

/*! \brief Get the current ring buffer index for the current thread (can be called from the game or render threads).
 *  \note This is thread-safe only if called from the game or render thread. Other threads should not call this function. */
HYP_API uint32 GetRingIndex();

/*! \brief Get the global frame counter value that is incremented every frame.
 *  This is used to track the number of frames that have been rendered.
 *  \note This is thread-safe and can be called from any thread as the frame counter is atomic. */
HYP_API uint32 GetFrameCounter();

void BeginFrameSim();
void EndFrameSim();

void BeginFrameRender();
void EndFrameRender();

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

} // namespace RenderApi

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

        GraphicsPipeline* prevGraphicsPipeline = nullptr;
        DescriptorSet* prevBoundDescriptorSets[MaxBoundDescriptorSets] {};

        uint8 stencilReference = 0;
        uint8 stencilCompareMask = 0xFF;
        uint8 stencilWriteMask = 0x0;
        
        void Reset()
        {
            attributes = {};
            validUniforms = 0;
            dirtyUniforms = 0;
            dirtyBufferOffsets = 0;
            renderTargetDesc = {};
            prevGraphicsPipeline = nullptr;
            Memory::MemSet(prevBoundDescriptorSets, 0, sizeof(prevBoundDescriptorSets));
            stencilReference = 0;
            stencilCompareMask = 0xFF;
            stencilWriteMask = 0x0;
        }
    };

    RenderInterface();
    
    RenderInterface(const RenderInterface& other) = delete;
    RenderInterface& operator=(const RenderInterface& other) = delete;

    ~RenderInterface();

    void AddRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);
    void RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);

    void UpdateBuffers(Frame* frame);

    void CommitDrawState();

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

    FinalPass* finalPass;

    ShaderPropertyCache* shaderPropertyCache;

    TextureViewCache* textureViewCache;

    Array<World*> renderWorlds[RingBufferDepth];

    State state;
    
    DescriptorSetCache* descriptorSetCache;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();
    void CreateEnvProbesTexture();

    void SetDefaultDescriptorSetElements(uint32 frameIndex);
};

} // namespace Hyperion
