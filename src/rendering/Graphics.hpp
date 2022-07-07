#ifndef HYPERION_V2_GRAPHICS_H
#define HYPERION_V2_GRAPHICS_H

#include <core/Containers.hpp>
#include <animation/Skeleton.hpp>
#include <scene/Spatial.hpp>
#include "Shader.hpp"
#include "Framebuffer.hpp"
#include "RenderPass.hpp"
#include "RenderBucket.hpp"
#include "RenderableAttributes.hpp"
#include <Constants.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <memory>
#include <mutex>
#include <vector>
#include <deque>

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

    UInt GetMultiviewIndex() const                          { return m_multiview_index; }
    void SetMultiviewIndex(UInt multiview_index)            { m_multiview_index = multiview_index; }

    void AddSpatial(Ref<Spatial> &&spatial);
    void RemoveSpatial(Ref<Spatial> &&spatial, bool call_on_removed = true);
    auto &GetSpatials()                                              { return m_spatials; }
    const auto &GetSpatials() const                                  { return m_spatials; }

    void AddFramebuffer(Ref<Framebuffer> &&fbo) { m_fbos.push_back(std::move(fbo)); }
    void RemoveFramebuffer(Framebuffer::ID id);

    auto &GetFramebuffers()             { return m_fbos; } 
    const auto &GetFramebuffers() const { return m_fbos; }
    
    /* Build pipeline */
    void Init(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    struct CachedRenderData {
        UInt          cycles_remaining{max_frames_in_flight + 1};
        Spatial::ID   spatial_id;
        Ref<Material> material;
        Ref<Mesh>     mesh;
        Ref<Skeleton> skeleton;
        Ref<Shader>   shader;
    };

    void PerformEnqueuedSpatialUpdates(Engine *engine, UInt frame_index);
    
    void UpdateEnqueuedSpatialsFlag()
    {
        m_enqueued_spatials_flag.store(
           !m_spatials_pending_addition.empty() || !m_spatials_pending_removal.empty()
        );
    }

    std::unique_ptr<renderer::GraphicsPipeline> m_pipeline;

    Ref<Shader>                       m_shader;
    Ref<RenderPass>                   m_render_pass;
    RenderableAttributeSet            m_renderable_attributes;
    UInt                              m_multiview_index;
    
    std::vector<Ref<Framebuffer>>     m_fbos;

    std::vector<Ref<Spatial>>         m_spatials; // lives in RENDER thread
    std::vector<Ref<Spatial>>         m_spatials_pending_addition; // shared
    std::vector<Ref<Spatial>>         m_spatials_pending_removal; // shared

    std::vector<CachedRenderData>     m_cached_render_data;

    PerFrameData<CommandBuffer>      *m_per_frame_data;

    std::mutex                        m_enqueued_spatials_mutex;
    std::atomic_bool                  m_enqueued_spatials_flag{false};

};

} // namespace hyperion::v2

#endif
