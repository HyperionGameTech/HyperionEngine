/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_GLOBAL_STATE_HPP
#define HYPERION_RENDER_GLOBAL_STATE_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/Handle.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <rendering/util/ResourceBinder.hpp>

namespace hyperion {

class Engine;
class Entity;
class ShadowMapAllocator;
class GPUBufferHolderMap;
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

HYP_API extern SizeType GetNumDescendants(TypeID type_id);
HYP_API extern int GetSubclassIndex(TypeID base_type_id, TypeID subclass_type_id);

// Call at start of engine before render / game thread start ticking.
// Allocates containers declared in RenderGlobalState.cpp via DECLARE_RENDER_DATA_CONTAINER
HYP_API extern void RendererAPI_InitResourceContainers();

HYP_API extern uint32 RendererAPI_GetFrameIndex_RenderThread();
HYP_API extern uint32 RendererAPI_GetFrameIndex_GameThread();

HYP_API extern void RendererAPI_BeginFrame_GameThread();
HYP_API extern void RendererAPI_EndFrame_GameThread();

HYP_API extern void RendererAPI_BeginFrame_RenderThread();
HYP_API extern void RendererAPI_EndFrame_RenderThread();

/*! \brief Get the RenderProxyList for the Game thread to write to for the current frame, for the given view.
 *  The game thread adds proxies of entities, lights, envprobes, etc. to this list, which the render thread will
 *  use when rendering the frame.
 *  \note This is only valid to call from the game thread, or from a task that is initiated by the game thread. */
HYP_API extern RenderProxyList& RendererAPI_GetProducerProxyList(View* view);

/*! \brief Get the RenderProxyList for the Render thread to read from for the current frame, for the given view.
 *  \note This is only valid to call from the render thread, or from a task that is initiated by the render thread. */
HYP_API extern RenderProxyList& RendererAPI_GetConsumerProxyList(View* view);

// Call on game (producer) thread
HYP_API extern void RendererAPI_AddRef(HypObjectBase* resource);
HYP_API extern void RendererAPI_ReleaseRef(IDBase id);

HYP_API extern IRenderProxy* RendererAPI_AllocRenderProxy(IDBase id);
HYP_API extern void RendererAPI_UpdateRenderProxy(IDBase id);
HYP_API extern void RendererAPI_UpdateRenderProxy(IDBase id, IRenderProxy* proxy);

// Call on render thread or render thread tasks only (consumer)
HYP_API extern IRenderProxy* RendererAPI_GetRenderProxy(IDBase id);

class RenderGlobalState
{
    friend class ResourceBinderBase;

    static void OnEnvProbeBindingChanged(EnvProbe* env_probe, uint32 prev, uint32 next);

public:
    static constexpr uint32 max_binders = 16;

    RenderGlobalState();
    RenderGlobalState(const RenderGlobalState& other) = delete;
    RenderGlobalState& operator=(const RenderGlobalState& other) = delete;
    ~RenderGlobalState();

    void Create();
    void Destroy();
    void UpdateBuffers(FrameBase* frame);

    GPUBufferHolderBase* Worlds;
    GPUBufferHolderBase* Cameras;
    GPUBufferHolderBase* Lights;
    GPUBufferHolderBase* Entities;
    GPUBufferHolderBase* Materials;
    GPUBufferHolderBase* Skeletons;
    GPUBufferHolderBase* ShadowMaps;
    GPUBufferHolderBase* EnvProbes;
    GPUBufferHolderBase* EnvGrids;
    GPUBufferHolderBase* LightmapVolumes;

    BindlessStorage BindlessTextures;

    UniquePtr<class ShadowMapAllocator> ShadowMapAllocator;
    UniquePtr<class GPUBufferHolderMap> GPUBufferHolderMap;
    UniquePtr<PlaceholderData> PlaceholderData;

    DescriptorTableRef GlobalDescriptorTable;

    RendererBase* Renderer;
    EnvProbeRenderer** EnvProbeRenderers;

    ResourceBinderBase* ResourceBinders[max_binders] { nullptr };
    ResourceBinder<EnvProbe, &OnEnvProbeBindingChanged> EnvProbeBinder;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();

    void SetDefaultDescriptorSetElements(uint32 frame_index);
};

} // namespace hyperion

#endif