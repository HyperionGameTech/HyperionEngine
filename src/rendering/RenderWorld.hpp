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
struct ShadowMapAtlasElement;

enum class ShadowMapType : uint32;
enum class ShadowMapFilterMode : uint32;

struct ShadowMapAtlas
{
    struct SkylineNode
    {
        Vec2i offset;
        Vec2i dimensions;
    };

    uint32                                          atlas_index;
    Vec2u                                           atlas_dimensions;
    Array<ShadowMapAtlasElement, DynamicAllocator>  elements;
    Array<SkylineNode>                              free_spaces;

    ShadowMapAtlas(uint32 atlas_index, const Vec2u &atlas_dimensions);

    bool AddElement(const Vec2u &dimensions, ShadowMapAtlasElement &out_element);
    bool RemoveElement(const ShadowMapAtlasElement &element);

    void Clear();

    bool CalculateFitOffset(uint32 index, const Vec2u &dimensions, Vec2u &out_offset) const;
    void AddSkylineNode(uint32 before_index, const Vec2u &dimensions, const Vec2u &offset);
    void MergeSkyline();
};

class ShadowMapManager
{
public:
    ShadowMapManager();
    ~ShadowMapManager();

    HYP_FORCE_INLINE const ImageRef &GetImage() const
        { return m_image; }

    HYP_FORCE_INLINE const ImageViewRef &GetImageView() const
        { return m_image_view; }

    void Initialize();
    void Destroy();

    ShadowMapRenderResource *AllocateShadowMap(ShadowMapType shadow_map_type, ShadowMapFilterMode filter_mode, const Vec2u &dimensions);
    bool FreeShadowMap(ShadowMapRenderResource *shadow_map_render_resource);

private:
    Vec2u                       m_atlas_dimensions;
    Array<ShadowMapAtlas>       m_atlases;

    ImageRef                    m_image;
    ImageViewRef                m_image_view;
};

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

    HYP_FORCE_INLINE ShadowMapManager *GetShadowMapManager() const
        { return m_shadow_map_manager.Get(); }

    void AddScene(const Handle<Scene> &scene);
    Task<bool> RemoveScene(const WeakHandle<Scene> &scene_weak);

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

    UniquePtr<ShadowMapManager>                                 m_shadow_map_manager;

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
