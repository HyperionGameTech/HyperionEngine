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

using renderer::PushConstantData;

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

struct HYP_API RenderProxyList
{
    RenderProxyList();

    RenderProxyList(const RenderProxyList& other) = delete;
    RenderProxyList& operator=(const RenderProxyList& other) = delete;

    RenderProxyList(RenderProxyList&& other) noexcept = delete;
    RenderProxyList& operator=(RenderProxyList&& other) noexcept = delete;

    ~RenderProxyList();

    void ClearProxyGroups();
    void RemoveEmptyProxyGroups();

    uint32 NumRenderGroups() const;

    ParallelRenderingState* AcquireNextParallelRenderingState();
    void CommitParallelRenderingState(RHICommandList& out_command_list);

    FixedArray<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>, Bucket::BUCKET_MAX> mappings_by_bucket;
    RenderProxyTracker render_proxy_tracker;
    ParallelRenderingState* parallel_rendering_state_head;
    ParallelRenderingState* parallel_rendering_state_tail;
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