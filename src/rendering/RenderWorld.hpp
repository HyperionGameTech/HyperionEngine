/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_WORLD_HPP
#define HYPERION_RENDERING_WORLD_HPP

#include <core/math/Vector4.hpp>
#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/EngineRenderStats.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class World;
class Scene;
class RenderEnvironment;
class CameraRenderResource;
class SceneRenderResource;

class RenderCollectorContainer
{
public:
    static constexpr uint32 max_scenes = 128;

    RenderCollectorContainer()
        : m_num_render_collectors { 0u }
    {
    }

    RenderCollectorContainer(const RenderCollectorContainer &other)                 = delete;
    RenderCollectorContainer &operator=(RenderCollectorContainer &other)            = delete;
    RenderCollectorContainer(RenderCollectorContainer &&other) noexcept             = delete;
    RenderCollectorContainer &operator=(RenderCollectorContainer &&other) noexcept  = delete;
    ~RenderCollectorContainer()                                                     = default;

    uint32 NumRenderCollectors() const
        { return m_num_render_collectors.Get(MemoryOrder::ACQUIRE); }

    void AddScene(ID<Scene> scene_id);
    void RemoveScene(ID<Scene> scene_id);

    RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id)
        { return m_render_collectors_by_id_index[scene_id.ToIndex()]; }

    const RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id) const
        { return m_render_collectors_by_id_index[scene_id.ToIndex()]; }

    RenderCollector *GetRenderCollectorAtIndex(uint32 index) const
        { return m_render_collectors[index]; }

private:
    FixedArray<RenderCollector *, max_scenes>   m_render_collectors;
    FixedArray<RenderCollector, max_scenes>     m_render_collectors_by_id_index;
    AtomicVar<uint32>                           m_num_render_collectors;
};

class WorldRenderResource final : public RenderResourceBase
{
public:
    WorldRenderResource(World *world);
    virtual ~WorldRenderResource() override;

    HYP_FORCE_INLINE World *GetWorld() const
        { return m_world; }

    HYP_FORCE_INLINE RenderCollectorContainer &GetRenderCollectorContainer()
        { return m_render_collector_container; }

    HYP_FORCE_INLINE const RenderCollectorContainer &GetRenderCollectorContainer() const
        { return m_render_collector_container; }

    HYP_FORCE_INLINE RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id)
        { return m_render_collector_container.GetRenderCollectorForScene(scene_id); }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id) const
        { return m_render_collector_container.GetRenderCollectorForScene(scene_id); }

    HYP_FORCE_INLINE const Handle<RenderEnvironment> &GetEnvironment() const
        { return m_environment; }

    void AddScene(const Handle<Scene> &scene);
    Task<bool> RemoveScene(ID<Scene> scene_id);

    const EngineRenderStats &GetRenderStats() const;
    void SetRenderStats(const EngineRenderStats &render_stats);

    void PreRender(renderer::Frame *frame);
    void PostRender(renderer::Frame *frame);
    void Render(renderer::Frame *frame);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    World                                                       *m_world;
    Array<TResourceHandle<SceneRenderResource>>                 m_bound_scenes;
    RenderCollectorContainer                                    m_render_collector_container;
    Handle<RenderEnvironment>                                   m_environment;
    FixedArray<EngineRenderStats, ThreadType::THREAD_TYPE_MAX>  m_render_stats;
};

template <>
struct ResourceMemoryPoolInitInfo<WorldRenderResource> : MemoryPoolInitInfo
{
    static constexpr uint32 num_elements_per_block = 16;
    static constexpr uint32 num_initial_elements = 16;
};

} // namespace hyperion

#endif
