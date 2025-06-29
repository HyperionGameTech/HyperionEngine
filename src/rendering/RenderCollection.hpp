/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COLLECTION_HPP
#define HYPERION_RENDER_COLLECTION_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/object/ObjId.hpp>

#include <core/math/Transform.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/EngineRenderStats.hpp>
#include <rendering/IndirectDraw.hpp>

#include <rendering/rhi/CmdList.hpp>

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
class LightmapVolume;
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
    CmdList base_command_list;

    FixedArray<CmdList, max_batches> command_lists {};
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
    HashMap<ObjId<Entity>, RenderProxy*> render_proxies;
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

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE SizeType NumDrawCallsCollected() const
    {
        SizeType num_draw_calls = 0;

        for (const auto& mappings : mappings_by_bucket)
        {
            for (const KeyValuePair<RenderableAttributeSet, DrawCallCollectionMapping>& it : mappings)
            {
                const DrawCallCollectionMapping& mapping = it.second;

                num_draw_calls += mapping.draw_call_collection.draw_calls.Size()
                    + mapping.draw_call_collection.instanced_draw_calls.Size();
            }
        }

        return num_draw_calls;
    }
#endif

    void Clear();
    void RemoveEmptyRenderGroups();

    /*! \brief Counts the number of render groups in the list. */
    uint32 NumRenderGroups() const;

    /*! \brief Builds RenderGroups for proxies, based on renderable attributes */
    void BuildRenderGroups(View* view);

    ParallelRenderingState* AcquireNextParallelRenderingState();
    void CommitParallelRenderingState(CmdList& out_command_list);

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

    ResourceTracker<ObjId<Entity>, RenderProxy> meshes;
    ResourceTracker<ObjId<EnvProbe>, EnvProbe*> env_probes;
    ResourceTracker<ObjId<Light>, Light*> lights;
    ResourceTracker<ObjId<EnvGrid>, EnvGrid*> env_grids;
    ResourceTracker<ObjId<LightmapVolume>, LightmapVolume*> lightmap_volumes;

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
    static void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& render_setup, RenderProxyList& render_proxy_list, uint32 bucket_bits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    static void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& render_setup, RenderProxyList& render_proxy_list, const FramebufferRef& framebuffer, uint32 bucket_bits);
};

} // namespace hyperion

#endif