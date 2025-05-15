/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_VIEW_HPP
#define HYPERION_RENDERING_VIEW_HPP

#include <core/memory/UniquePtr.hpp>
#include <core/Handle.hpp>

#include <rendering/Renderer.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <HashCode.hpp>

namespace hyperion {

class View;
class DeferredRenderer;
class WorldRenderResource;
class SceneRenderResource;
class CameraRenderResource;
class LightRenderResource;
class GBuffer;
class EnvGridPass;
class ReflectionsPass;
class FullScreenPass;
class DepthPyramidRenderer;
class TemporalAA;
class PostProcessing;
class DeferredPass;
class HBAO;
class SSGI;
class DOFBlur;
class Texture;
enum class LightType : uint32;

class ViewRenderResource : public RenderResourceBase
{
public:
    ViewRenderResource(View *view);
    ViewRenderResource(const ViewRenderResource &other)             = delete;
    ViewRenderResource &operator=(const ViewRenderResource &other)  = delete;
    virtual ~ViewRenderResource() override;

    HYP_FORCE_INLINE View *GetView() const
        { return m_view; }

    HYP_FORCE_INLINE const RendererConfig &GetRendererConfig() const
        { return m_renderer_config; }

    HYP_FORCE_INLINE const Viewport &GetViewport() const
        { return m_viewport; }

    void SetViewport(const Viewport &viewport);

    HYP_FORCE_INLINE int GetPriority() const
        { return m_priority; }

    void SetPriority(int priority);

    void AddLight(const TResourceHandle<LightRenderResource> &light);
    void RemoveLight(LightRenderResource *light);

    /*! \brief Get the currently bound Lights with the given LightType.
     *  \note Only call from render thread or from task on a task thread that is initiated by the render thread.
     *  \param type The type of light to get. */
    HYP_FORCE_INLINE const Array<TResourceHandle<LightRenderResource>> &GetLights(LightType type) const
    {
        AssertDebug(uint32(type) < m_light_render_resource_handles.Size());

        return m_light_render_resource_handles[uint32(type)];
    }

    void SetLights(Array<TResourceHandle<LightRenderResource>> &&lights);

    HYP_FORCE_INLINE SizeType NumLights() const
    {
        SizeType num_lights = 0;

        for (const auto &lights : m_light_render_resource_handles) {
            num_lights += lights.Size();
        }

        return num_lights;
    }

    HYP_FORCE_INLINE const TResourceHandle<SceneRenderResource> &GetScene() const
        { return m_scene_render_resource_handle; }

    HYP_FORCE_INLINE void SetScene(const TResourceHandle<SceneRenderResource> &scene)
        { m_scene_render_resource_handle = scene; }

    HYP_FORCE_INLINE const TResourceHandle<CameraRenderResource> &GetCamera() const
        { return m_camera_render_resource_handle; }

    HYP_FORCE_INLINE void SetCamera(const TResourceHandle<CameraRenderResource> &camera)
        { m_camera_render_resource_handle = camera; }

    HYP_FORCE_INLINE const DescriptorSetRef &GetFinalPassDescriptorSet() const
        { return m_final_pass_descriptor_set; }

    HYP_FORCE_INLINE GBuffer *GetGBuffer() const
        { return m_gbuffer.Get(); }

    HYP_FORCE_INLINE ReflectionsPass *GetReflectionsPass() const
        { return m_reflections_pass.Get(); }

    HYP_FORCE_INLINE FullScreenPass *GetCombinePass() const
        { return m_combine_pass.Get(); }

    HYP_FORCE_INLINE PostProcessing *GetPostProcessing() const
        { return m_post_processing.Get(); }

    HYP_FORCE_INLINE DepthPyramidRenderer *GetDepthPyramidRenderer() const
        { return m_depth_pyramid_renderer.Get(); }

    HYP_FORCE_INLINE const Handle<Texture> &GetMipChain() const
        { return m_mip_chain; }

    HYP_FORCE_INLINE TemporalAA *GetTemporalAA() const
        { return m_temporal_aa.Get(); }

    HYP_FORCE_INLINE const FixedArray<DescriptorSetRef, max_frames_in_flight> &GetDescriptorSets() const
        { return m_descriptor_sets; }

    HYP_FORCE_INLINE RenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollector() const
        { return m_render_collector; }

    /*! \brief Update the render collector on the render thread to reflect the current state */
    RenderCollector::CollectionResult UpdateRenderCollector();

    virtual void PreFrameUpdate(FrameBase *frame);

    virtual void Render(FrameBase *frame, WorldRenderResource *world_render_resource);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    void CreateRenderer();
    void DestroyRenderer();

    void CreateCombinePass();
    void CreateDescriptorSets();
    void CreateFinalPassDescriptorSet();

    void CollectDrawCalls(FrameBase *frame);
    void ExecuteDrawCalls(FrameBase *frame, uint64 bucket_mask);

    void GenerateMipChain(FrameBase *frame, const ImageRef &image);

    View                                                *m_view;

    RendererConfig                                      m_renderer_config;

    Viewport                                            m_viewport;

    int                                                 m_priority;

    // Descriptor set used when rendering the View using FinalPass.
    DescriptorSetRef                                    m_final_pass_descriptor_set;

    TResourceHandle<SceneRenderResource>                m_scene_render_resource_handle;
    TResourceHandle<CameraRenderResource>               m_camera_render_resource_handle;

    Array<Array<TResourceHandle<LightRenderResource>>>  m_light_render_resource_handles;

    RenderCollector                                     m_render_collector;

    UniquePtr<GBuffer>                                  m_gbuffer;

    UniquePtr<DeferredPass>                             m_indirect_pass;
    UniquePtr<DeferredPass>                             m_direct_pass;

    UniquePtr<EnvGridPass>                              m_env_grid_radiance_pass;
    UniquePtr<EnvGridPass>                              m_env_grid_irradiance_pass;

    UniquePtr<ReflectionsPass>                          m_reflections_pass;

    UniquePtr<PostProcessing>                           m_post_processing;
    UniquePtr<HBAO>                                     m_hbao;
    UniquePtr<TemporalAA>                               m_temporal_aa;
    UniquePtr<SSGI>                                     m_ssgi;

    UniquePtr<FullScreenPass>                           m_combine_pass;

    UniquePtr<DepthPyramidRenderer>                     m_depth_pyramid_renderer;

    UniquePtr<DOFBlur>                                  m_dof_blur;

    Handle<Texture>                                     m_mip_chain;
    
    CullData                                            m_cull_data;

    FixedArray<DescriptorSetRef, max_frames_in_flight>  m_descriptor_sets;
};

} // namespace hyperion

#endif

