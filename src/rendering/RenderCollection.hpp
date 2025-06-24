/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COLLECTION_HPP
#define HYPERION_RENDER_COLLECTION_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/ID.hpp>

#include <core/math/Transform.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/EngineRenderStats.hpp>
#include <rendering/IndirectDraw.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Scene;
class Camera;
class Entity;
class RenderGroup;
class RenderCamera;
class RenderView;
struct RenderSetup;
class IndirectRenderer;
class RenderLight;
class RenderLightmapVolume;
class RenderEnvGrid;
class RenderEnvProbe;
class ReflectionProbe;
enum LightType : uint32;
enum EnvProbeType : uint32;

struct ParallelRenderingState
{
    static constexpr uint32 max_batches = num_async_rendering_command_buffers;

    TaskBatch* task_batch = nullptr;

    uint32 num_batches = 0;

    // Non-async rendering command list - used for binding state at the start of the pass before async stuff
    RHICommandList base_command_list;

    FixedArray<RHICommandList, max_batches> command_lists {};
    FixedArray<EngineRenderStatsCounts, max_batches> render_stats_counts {};

    // Temporary storage for data that will be executed in parallel during the frame
    Array<Span<const DrawCall>, FixedAllocator<max_batches>> draw_calls;
    Array<Span<const InstancedDrawCall>, FixedAllocator<max_batches>> instanced_draw_calls;
    Array<Proc<void(Span<const DrawCall>, uint32, uint32)>, FixedAllocator<1>> draw_call_procs;
    Array<Proc<void(Span<const InstancedDrawCall>, uint32, uint32)>, FixedAllocator<1>> instanced_draw_call_procs;

    ParallelRenderingState* next = nullptr;
};

// Utility struct that maps attribute sets -> draw call collections that have been written to already and had render groups created.
struct DrawCallCollectionMapping
{
    Handle<RenderGroup> render_group;
    HashMap<ID<Entity>, RenderProxy*> render_proxies;
    DrawCallCollection draw_call_collection;
    IndirectRenderer* indirect_renderer = nullptr;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return render_group.IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return render_group.IsValid();
    }
};

/*! \brief A collection of renderable objects and setup for a specific View, proxied so the render thread can work with it. */
struct HYP_API RenderProxyList
{
    RenderProxyList();

    RenderProxyList(const RenderProxyList& other) = delete;
    RenderProxyList& operator=(const RenderProxyList& other) = delete;

    RenderProxyList(RenderProxyList&& other) noexcept = delete;
    RenderProxyList& operator=(RenderProxyList&& other) noexcept = delete;

    ~RenderProxyList();

    /*! \brief Get the currently bound Lights with the given LightType.
     *  \note Only call from render thread or from task on a task thread that is initiated by the render thread.
     *  \param type The type of light to get. */
    HYP_FORCE_INLINE const Array<RenderLight*>& GetLights(LightType type) const
    {
        AssertDebug(type < lights.Size());

        return lights[type];
    }

    HYP_FORCE_INLINE SizeType NumLights() const
    {
        SizeType num_lights = 0;

        for (const auto& it : lights)
        {
            num_lights += it.Size();
        }

        return num_lights;
    }

    HYP_FORCE_INLINE const Array<RenderEnvGrid*>& GetEnvGrids() const
    {
        return env_grids;
    }

    HYP_FORCE_INLINE const Array<RenderEnvProbe*>& GetEnvProbes(EnvProbeType type) const
    {
        AssertDebug(type < env_probes.Size());

        return env_probes[type];
    }

    HYP_FORCE_INLINE SizeType NumEnvProbes() const
    {
        SizeType num_env_probes = 0;

        for (const auto& it : env_probes)
        {
            num_env_probes += it.Size();
        }

        return num_env_probes;
    }

    void Clear();
    void RemoveEmptyRenderGroups();

    /*! \brief Counts the number of render groups in the list. */
    uint32 NumRenderGroups() const;

    /*! \brief Creates RenderGroups and adds/removes RenderProxies based on the renderable attributes of the render proxies.
     *  If override_attributes is provided, it will be used instead of the renderable attributes of the render proxies. */
    void BuildRenderGroups(RenderView* render_view, const RenderableAttributeSet* override_attributes = nullptr);

    ParallelRenderingState* AcquireNextParallelRenderingState();
    void CommitParallelRenderingState(RHICommandList& out_command_list);

    // State for tracking transitions from writing (game thread) to reading (render thread).
    enum CollectionState : uint8
    {
        CS_WRITING, //!< Currently being written to. set when the frame starts on the game thread.
        CS_WRITTEN, //!< Written to, but not yet read from. set when the frame finishes on the game thread.
        CS_READING, //!< Currently ready to be read. set when the frame starts on the render thread.
        CS_DONE     //!< Finished reading. set when the frame finishes on the render thread.
    };

    CollectionState state : 2 = CS_DONE;

    Viewport viewport;
    int priority;

    ResourceTracker<ID<Entity>, RenderProxy> meshes;
    ResourceTracker<ID<Light>, RenderLight*> tracked_lights;
    ResourceTracker<ID<EnvProbe>, RenderEnvProbe*> tracked_env_probes;
    ResourceTracker<ID<EnvGrid>, RenderEnvGrid*> tracked_env_grids;
    ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*> tracked_lightmap_volumes;

    Array<Array<RenderLight*>> lights;
    Array<Array<RenderEnvProbe*>> env_probes;
    Array<RenderEnvGrid*> env_grids;
    Array<RenderLightmapVolume*> lightmap_volumes;

    ParallelRenderingState* parallel_rendering_state_head;
    ParallelRenderingState* parallel_rendering_state_tail;

    FixedArray<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>, RB_MAX> mappings_by_bucket;

#ifdef HYP_ENABLE_MT_CHECK
    HYP_DECLARE_MT_CHECK(data_race_detector);
#endif
};

class RenderCollector
{
public:
    static void CollectDrawCalls(RenderProxyList& render_proxy_list, uint32 bucket_bits);

    static void PerformOcclusionCulling(FrameBase* frame, const RenderSetup& render_setup, RenderProxyList& render_proxy_list, uint32 bucket_bits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    static void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& render_setup, RenderProxyList& render_proxy_list, uint32 bucket_bits, PushConstantData push_constant = {});

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    static void ExecuteDrawCalls(
        FrameBase* frame,
        const RenderSetup& render_setup,
        RenderProxyList& render_proxy_list,
        const FramebufferRef& framebuffer,
        uint32 bucket_bits,
        PushConstantData push_constant = {});
};

} // namespace hyperion

#endif