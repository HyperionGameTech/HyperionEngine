/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>

#include <core/object/Handle.hpp>

#include <rendering/Buffers.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <rendering/util/ResourceBinder.hpp>

#include <util/ResourceTracker.hpp>

namespace hyperion {

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
class MaterialDescriptorSetManager;
class GraphicsPipelineCache;
class BindlessStorage;
class RenderCollector;
struct WorldShaderData;
struct RenderStats;
struct RenderStatsCounts;
struct Viewport;

HYP_API extern SizeType GetNumDescendants(TypeId typeId);
HYP_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

// Call at start of engine before render / game thread start ticking.
// Allocates containers declared in RenderGlobalState.cpp via DECLARE_RENDER_DATA_CONTAINER
void RenderApi_Init();
void RenderApi_Shutdown();

uint32 RenderApi_GetFrameIndex();
uint32 RenderApi_GetFrameCounter();

void RenderApi_BeginFrame_GameThread();
void RenderApi_EndFrame_GameThread();

void RenderApi_BeginFrame_RenderThread();
void RenderApi_EndFrame_RenderThread();

/*! \brief Get the RenderProxyList for the Game thread to write to for the current frame, for the given view.
 *  The game thread adds proxies of entities, lights, envprobes, etc. to this list, which the render thread will
 *  use when rendering the frame.
 *  \note This is only valid to call from the game thread, or from a task that is initiated by the game thread. */
RenderProxyList& RenderApi_GetProducerProxyList(View* view);

/*! \brief Get the RenderProxyList for the Render thread to read from for the current frame, for the given view.
 *  \note This is only valid to call from the render thread, or from a task that is initiated by the render thread. */
RenderProxyList& RenderApi_GetConsumerProxyList(View* view);

/*! \brief Get the RenderCollector corresponding to the given View, only usable on the Render thread. */
RenderCollector& RenderApi_GetRenderCollector(View* view);

// Call on render thread or render thread tasks only (consumer threads)
IRenderProxy* RenderApi_GetRenderProxy(ObjIdBase resourceId);

/*! \brief Render thread only - update GPU data to match RenderProxy's buffer data for the resource with the given ID */
void RenderApi_UpdateGpuData(ObjIdBase resourceId);

// used on render thread only - assigns all render proxy for the given object to the given binding
void RenderApi_AssignResourceBinding(HypObjectBase* resource, uint32 binding);
// used on render thread only - retrieves the binding set for the given resource (~0u if unset)
uint32 RenderApi_RetrieveResourceBinding(const HypObjectBase* resource);
uint32 RenderApi_RetrieveResourceBinding(ObjIdBase resourceId);

WorldShaderData* RenderApi_GetWorldBufferData();

Viewport& RenderApi_GetViewport(View* view);

RenderStats* RenderApi_GetRenderStats();
void RenderApi_AddRenderStats(const RenderStatsCounts& counts);
void RenderApi_SuppressRenderStats();
void RenderApi_UnsuppressRenderStats();

struct ResourceBindings;

HYP_ENUM()
enum GlobalRenderBuffer : uint8
{
    GRB_INVALID = uint8(-1),

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

    GRT_ENV_PROBE = 0, //!< Global renderer instances for different EnvProbe classes
    GRT_ENV_GRID,      //!< Global renderer instance for EnvGrids
    GRT_SHADOW_MAP,    //!< Shadow map renderers, e.g. PointLightShadowRenderer, DirectionalLightShadowRenderer
    GRT_UI,            //!< Globally registered UIRenderer instances to be used by FinalPass to draw the UI onto the backbuffer.

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

class RenderGlobalState
{
    friend class ResourceBinderBase;

public:
    RenderGlobalState();
    RenderGlobalState(const RenderGlobalState& other) = delete;
    RenderGlobalState& operator=(const RenderGlobalState& other) = delete;
    ~RenderGlobalState();

    void UpdateBuffers(FrameBase* frame);

    void AddRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);
    void RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);

    BindlessStorage* bindlessStorage;

    UniquePtr<ShadowMapAllocator> shadowMapAllocator;
    UniquePtr<PlaceholderData> placeholderData;

    UniquePtr<GpuBufferHolderMap> gpuBufferHolders;

    DescriptorTableRef globalDescriptorTable;

    RendererBase* mainRenderer;
    Array<RendererBase*> globalRenderers[GRT_MAX];

    GlobalGpuBuffers gpuBuffers;
    ResourceBindings* resourceBindings;

    MaterialDescriptorSetManager* materialDescriptorSetManager;

    GraphicsPipelineCache* graphicsPipelineCache;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();

    void SetDefaultDescriptorSetElements(uint32 frameIndex);
};

} // namespace hyperion
