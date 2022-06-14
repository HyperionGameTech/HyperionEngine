#ifndef HYPERION_V2_GRAPHICS_H
#define HYPERION_V2_GRAPHICS_H

#include <core/containers.h>
#include <core/observer.h>
#include "shader.h"
#include "../scene/spatial.h"
#include "framebuffer.h"
#include "render_pass.h"
#include "render_bucket.h"
#include "renderable_attributes.h"

#include <rendering/backend/renderer_graphics_pipeline.h>
#include <rendering/backend/renderer_frame.h>

#include <memory>
#include <mutex>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;
using renderer::DescriptorSetBinding;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::VertexAttributeSet;
using renderer::PerFrameData;
using renderer::Topology;
using renderer::FillMode;
using renderer::FaceCullMode;
using renderer::StencilState;
using renderer::Frame;

class Engine;

class GraphicsPipeline : public EngineComponentBase<STUB_CLASS(GraphicsPipeline)> {
    friend class Engine;
    friend class Spatial;

public:
    GraphicsPipeline(
        Ref<Shader> &&shader,
        Ref<RenderPass> &&render_pass,
        const RenderableAttributeSet &renderable_attributes
    );

    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    renderer::GraphicsPipeline *GetPipeline() const         { return m_pipeline.get(); }
    Shader *GetShader() const                               { return m_shader.ptr; }
    
    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }
    
    Topology GetTopology() const                            { return m_renderable_attributes.topology; }
    void SetTopology(Topology topology)                     { m_renderable_attributes.topology = topology; }
                                                            
    auto GetFillMode() const                                { return m_renderable_attributes.fill_mode; }
    void SetFillMode(FillMode fill_mode)                    { m_renderable_attributes.fill_mode = fill_mode; }
                                                            
    auto GetCullMode() const                                { return m_renderable_attributes.cull_faces; }
    void SetFaceCullMode(FaceCullMode cull_mode)            { m_renderable_attributes.cull_faces = cull_mode; }
                                                            
    bool GetDepthTest() const                               { return m_renderable_attributes.depth_test; }
    void SetDepthTest(bool depth_test)                      { m_renderable_attributes.depth_test = depth_test; }
                                                            
    bool GetDepthWrite() const                              { return m_renderable_attributes.depth_write; }
    void SetDepthWrite(bool depth_write)                    { m_renderable_attributes.depth_write = depth_write; }
                                                            
    bool GetBlendEnabled() const                            { return m_renderable_attributes.alpha_blending; }
    void SetBlendEnabled(bool blend_enabled)                { m_renderable_attributes.alpha_blending = blend_enabled; }
                                                            
    const StencilState &GetStencilMode() const              { return m_renderable_attributes.stencil_state; }
    void SetStencilState(const StencilState &stencil_state) { m_renderable_attributes.stencil_state = stencil_state; }

    void AddSpatial(Spatial *spatial);
    void RemoveSpatial(Spatial::ID id);
    auto &GetSpatials()                                              { return m_spatials; }
    const auto &GetSpatials() const                                  { return m_spatials; }
    ObserverNotifier<Spatial *> &GetSpatialNotifier()                { return m_spatial_notifier; }
    const ObserverNotifier<Spatial *> &GetSpatialNotifier() const    { return m_spatial_notifier; }

    void AddFramebuffer(Ref<Framebuffer> &&fbo) { m_fbos.push_back(std::move(fbo)); }
    void RemoveFramebuffer(Framebuffer::ID id);

    auto &GetFramebuffers()             { return m_fbos; } 
    const auto &GetFramebuffers() const { return m_fbos; }
    
    /* Build pipeline */
    void Init(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    static bool BucketSupportsCulling(Bucket bucket);
    
    bool RemoveFromSpatialList(
        Spatial::ID id,
        std::vector<Spatial *> &spatials,
        bool call_on_removed,
        bool dispatch_item_removed,
        bool remove_immediately
    );

    /* Called from Spatial - remove the pointer */
    void OnSpatialRemoved(Spatial *spatial);

    void PerformEnqueuedSpatialUpdates(Engine *engine);
    
    void UpdateEnqueuedSpatialsFlag()
    {
        m_enqueued_spatials_flag.store(
           !m_spatials_pending_addition.empty() || !m_spatials_pending_removal.empty()
        );
    }

    std::unique_ptr<renderer::GraphicsPipeline> m_pipeline;

    Ref<Shader>                    m_shader;
    Ref<RenderPass>                m_render_pass;
    RenderableAttributeSet         m_renderable_attributes;
    
    std::vector<Ref<Framebuffer>>  m_fbos;

    std::vector<Spatial *>      m_spatials;
    std::vector<Spatial *>      m_spatials_pending_addition;
    std::vector<Spatial *>      m_spatials_pending_removal;
    ObserverNotifier<Spatial *> m_spatial_notifier;

    PerFrameData<CommandBuffer>   *m_per_frame_data;

    std::mutex                     m_enqueued_spatials_mutex;
    std::atomic_bool               m_enqueued_spatials_flag{false};

};

} // namespace hyperion::v2

#endif
