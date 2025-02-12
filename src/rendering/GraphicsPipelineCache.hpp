/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GRAPHICS_PIPELINE_CACHE_HPP
#define HYPERION_GRAPHICS_PIPELINE_CACHE_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/Span.hpp>

#include <core/functional/Proc.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>

namespace hyperion {
    
class RenderableAttributeSet;

class GraphicsPipelineCache
{
public:
    GraphicsPipelineCache();
    GraphicsPipelineCache(const GraphicsPipelineCache &)            = delete;
    GraphicsPipelineCache &operator=(const GraphicsPipelineCache &) = delete;
    GraphicsPipelineCache(GraphicsPipelineCache &&)                 = delete;
    GraphicsPipelineCache &operator=(GraphicsPipelineCache &&)      = delete;
    ~GraphicsPipelineCache();

    void Initialize();
    void Destroy();

    GraphicsPipelineRef GetOrCreate(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table,
        const RenderPassRef &render_pass,
        const Array<FramebufferRef> &framebuffers,
        const RenderableAttributeSet &attributes,
        Proc<void, const GraphicsPipelineRef &> &&on_ready_callback = { }
    );

private:
    GraphicsPipelineRef FindGraphicsPipeline(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table,
        const RenderPassRef &render_pass,
        const Array<FramebufferRef> &framebuffers,
        const RenderableAttributeSet &attributes
    );

    HashMap<RenderableAttributeSet, Array<GraphicsPipelineRef>> m_cached_pipelines;
    Mutex                                                       m_mutex;
};

} // namespace hyperion

#endif
