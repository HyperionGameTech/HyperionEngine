/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_GLOBAL_STATE_HPP
#define HYPERION_RENDER_GLOBAL_STATE_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/Handle.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>

#include <rendering/util/ResourceBinder.hpp>

namespace hyperion {

class Engine;
class Entity;
class ShadowMapAllocator;
class GpuBufferHolderMap;
class PlaceholderData;
class RenderProxyList;
class RenderView;
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

HYP_API extern SizeType GetNumDescendants(TypeId type_id);
HYP_API extern int GetSubclassIndex(TypeId base_type_id, TypeId subclass_type_id);

// Call at start of engine before render / game thread start ticking.
// Allocates containers declared in RenderGlobalState.cpp via DECLARE_RENDER_DATA_CONTAINER
HYP_API extern void RenderApi_InitResourceContainers();

HYP_API extern uint32 RenderApi_GetFrameIndex_RenderThread();
HYP_API extern uint32 RenderApi_GetFrameIndex_GameThread();

HYP_API extern void RenderApi_BeginFrame_GameThread();
HYP_API extern void RenderApi_EndFrame_GameThread();

HYP_API extern void RenderApi_BeginFrame_RenderThread();
HYP_API extern void RenderApi_EndFrame_RenderThread();

/*! \brief Get the RenderProxyList for the Game thread to write to for the current frame, for the given view.
 *  The game thread adds proxies of entities, lights, envprobes, etc. to this list, which the render thread will
 *  use when rendering the frame.
 *  \note This is only valid to call from the game thread, or from a task that is initiated by the game thread. */
HYP_API extern RenderProxyList& RenderApi_GetProducerProxyList(View* view);

/*! \brief Get the RenderProxyList for the Render thread to read from for the current frame, for the given view.
 *  \note This is only valid to call from the render thread, or from a task that is initiated by the render thread. */
HYP_API extern RenderProxyList& RenderApi_GetConsumerProxyList(View* view);

// Call on game (producer) thread
HYP_API extern void RenderApi_AddRef(HypObjectBase* resource);
HYP_API extern void RenderApi_ReleaseRef(IdBase id);

HYP_API extern IRenderProxy* RenderApi_AllocRenderProxy(IdBase id);
HYP_API extern void RenderApi_UpdateRenderProxy(IdBase id);

// Call on render thread or render thread tasks only (consumer)
HYP_API extern IRenderProxy* RenderApi_GetRenderProxy(IdBase id);

struct ResourceBindings;

enum GlobalRenderBuffer : uint8
{
    GRB_WORLDS = 0,
    GRB_CAMERAS,
    GRB_LIGHTS,
    GRB_ENTITIES,
    GRB_MATERIALS,
    GRB_SKELETONS,
    GRB_SHADOW_MAPS,
    GRB_ENV_PROBES,
    GRB_ENV_GRIDS,
    GRB_LIGHTMAP_VOLUMES,

    GRB_MAX
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

    void Create();
    void Destroy();
    void UpdateBuffers(FrameBase* frame);

    BindlessStorage BindlessTextures;

    UniquePtr<class ShadowMapAllocator> ShadowMapAllocator;
    UniquePtr<class GpuBufferHolderMap> GpuBufferHolderMap;
    UniquePtr<PlaceholderData> PlaceholderData;

    DescriptorTableRef GlobalDescriptorTable;

    RendererBase* Renderer;
    EnvProbeRenderer** EnvProbeRenderers;
    class EnvGridRenderer* EnvGridRenderer;

    GlobalGpuBuffers gpu_buffers;
    ResourceBindings* resource_bindings;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();

    void SetDefaultDescriptorSetElements(uint32 frame_index);
};

} // namespace hyperion

#endif