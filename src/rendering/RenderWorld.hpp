/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_WORLD_HPP
#define HYPERION_RENDERING_WORLD_HPP

#include <core/math/Vector4.hpp>
#include <core/math/Matrix4.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/Id.hpp>
#include <core/Handle.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/EngineRenderStats.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <util/AtlasPacker.hpp>

#include <Types.hpp>

namespace hyperion {

class World;
class Scene;
class RenderEnvironment;
class RenderCamera;
class RenderScene;
class RenderShadowMap;
class ShadowMapAllocator;
class FinalPass;
class RenderView;
struct ViewInfo;

struct WorldShaderData
{
    Vec4f fog_params;

    float game_time;
    uint32 frame_counter;
    uint32 _pad0;
    uint32 _pad1;
};

class RenderWorld final : public RenderResourceBase
{
public:
    RenderWorld(World* world);
    virtual ~RenderWorld() override;

    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_FORCE_INLINE const Array<TResourceHandle<RenderView>>& GetViews() const
    {
        return m_render_views;
    }

    void AddView(TResourceHandle<RenderView>&& render_view);
    void RemoveView(RenderView* render_view);

    void AddScene(TResourceHandle<RenderScene>&& render_scene);
    void RemoveScene(RenderScene* render_scene);

    const EngineRenderStats& GetRenderStats() const;
    void SetRenderStats(const EngineRenderStats& render_stats);

    HYP_FORCE_INLINE RenderEnvironment* GetEnvironment() const
    {
        return m_render_environment.Get();
    }

    void SetBufferData(const WorldShaderData& buffer_data);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const WorldShaderData& GetBufferData() const
    {
        return m_buffer_data;
    }

    void PreRender(FrameBase* frame);
    void Render(FrameBase* frame);
    void PostRender(FrameBase* frame);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    void UpdateBufferData();
    void CreateShadowMapsTextureArray();

    World* m_world;

    Array<TResourceHandle<RenderView>> m_render_views;
    Array<TResourceHandle<RenderScene>> m_render_scenes;

    UniquePtr<RenderEnvironment> m_render_environment;

    FixedArray<EngineRenderStats, ThreadType::THREAD_TYPE_MAX> m_render_stats;

    WorldShaderData m_buffer_data;
};

template <>
struct ResourceMemoryPoolInitInfo<RenderWorld> : MemoryPoolInitInfo<RenderWorld>
{
    static constexpr uint32 num_elements_per_block = 8;
    static constexpr uint32 num_initial_elements = 8;
};

} // namespace hyperion

#endif
