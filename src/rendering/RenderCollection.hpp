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

using renderer::PushConstantData;

class EntityDrawCollection
{
public:
    void ClearProxyGroups();
    void RemoveEmptyProxyGroups();

    /*! \note To be used from render thread only */
    HYP_FORCE_INLINE FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, Bucket::BUCKET_MAX>& GetProxyGroups()
    {
        return m_proxy_groups;
    }

    /*! \note To be used from render thread only */
    HYP_FORCE_INLINE const FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, Bucket::BUCKET_MAX>& GetProxyGroups() const
    {
        return m_proxy_groups;
    }

    /*! \brief Get the Render thread side RenderProxyTracker.
     */
    HYP_FORCE_INLINE RenderProxyTracker& GetRenderProxyTracker()
    {
        return m_render_proxy_tracker;
    }

    /*! \brief Get the Render thread side RenderProxyTracker.
     */
    HYP_FORCE_INLINE const RenderProxyTracker& GetRenderProxyTracker() const
    {
        return m_render_proxy_tracker;
    }

    uint32 NumRenderGroups() const;

private:
    FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, Bucket::BUCKET_MAX> m_proxy_groups;
    RenderProxyTracker m_render_proxy_tracker;
};

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

class RenderCollector
{
public:
    RenderCollector();
    RenderCollector(const RenderCollector& other) = delete;
    RenderCollector& operator=(const RenderCollector& other) = delete;
    RenderCollector(RenderCollector&& other) noexcept = default;
    RenderCollector& operator=(RenderCollector&& other) noexcept = default;
    ~RenderCollector();

    HYP_FORCE_INLINE const RC<EntityDrawCollection>& GetDrawCollection() const
    {
        return m_draw_collection;
    }

    HYP_FORCE_INLINE const Optional<RenderableAttributeSet>& GetOverrideAttributes() const
    {
        return m_override_attributes;
    }

    HYP_FORCE_INLINE void SetOverrideAttributes(const Optional<RenderableAttributeSet>& override_attributes)
    {
        m_override_attributes = override_attributes;
    }

    void CollectDrawCalls(
        FrameBase* frame,
        const RenderSetup& render_setup,
        const Bitset& bucket_bits);

    void ExecuteDrawCalls(
        FrameBase* frame,
        const RenderSetup& render_setup,
        const Bitset& bucket_bits,
        PushConstantData push_constant = {}) const;

    void ExecuteDrawCalls(
        FrameBase* frame,
        const RenderSetup& render_setup,
        const FramebufferRef& framebuffer,
        const Bitset& bucket_bits,
        PushConstantData push_constant = {}) const;

protected:
    RC<EntityDrawCollection> m_draw_collection;
    Optional<RenderableAttributeSet> m_override_attributes;
    mutable ParallelRenderingState* m_parallel_rendering_state;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif