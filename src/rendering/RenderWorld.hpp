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
#include <core/IDGenerator.hpp>

#include <Types.hpp>

namespace hyperion {

class World;
class Scene;
class RenderEnvironment;
class CameraRenderResource;
class SceneRenderResource;
class ShadowMapRenderResource;

class WorldRenderResource final : public RenderResourceBase
{
public:
    WorldRenderResource(World *world);
    virtual ~WorldRenderResource() override;

    HYP_FORCE_INLINE World *GetWorld() const
        { return m_world; }

    HYP_FORCE_INLINE const Array<TResourceHandle<SceneRenderResource>> &GetBoundScenes() const
        { return m_bound_scenes; }

    HYP_FORCE_INLINE const Handle<RenderEnvironment> &GetEnvironment() const
        { return m_environment; }

    HYP_FORCE_INLINE const ImageRef &GetShadowMapsTextureArrayImage() const
        { return m_shadow_maps_texture_array_image; }

    HYP_FORCE_INLINE const ImageViewRef &GetShadowMapsTextureArrayImageView() const
        { return m_shadow_maps_texture_array_image_view; }

    void AddScene(const Handle<Scene> &scene);
    Task<bool> RemoveScene(const WeakHandle<Scene> &scene_weak);

    void AddShadowMapRenderResource(const TResourceHandle<ShadowMapRenderResource> &shadow_map_resource_handle);
    void RemoveShadowMapRenderResource(const ShadowMapRenderResource *shadow_map_render_resource);

    const EngineRenderStats &GetRenderStats() const;
    void SetRenderStats(const EngineRenderStats &render_stats);

    void PreRender(renderer::FrameBase *frame);
    void PostRender(renderer::FrameBase *frame);
    void Render(renderer::FrameBase *frame);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void CreateShadowMapsTextureArray();

    World                                                       *m_world;
    Array<TResourceHandle<SceneRenderResource>>                 m_bound_scenes;
    Handle<RenderEnvironment>                                   m_environment;

    ImageRef                                                    m_shadow_maps_texture_array_image;
    ImageViewRef                                                m_shadow_maps_texture_array_image_view;
    Array<TResourceHandle<ShadowMapRenderResource>>             m_shadow_map_resource_handles;

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
