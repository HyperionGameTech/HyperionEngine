/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GRAPHICS_PIPELINE_CACHE_HPP
#define HYPERION_GRAPHICS_PIPELINE_CACHE_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Task.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>

namespace hyperion {

class RenderableAttributeSet;
class CachedPipelinesMap;

class GraphicsPipelineCache
{
public:
    GraphicsPipelineCache();
    GraphicsPipelineCache(const GraphicsPipelineCache&) = delete;
    GraphicsPipelineCache& operator=(const GraphicsPipelineCache&) = delete;
    GraphicsPipelineCache(GraphicsPipelineCache&&) = delete;
    GraphicsPipelineCache& operator=(GraphicsPipelineCache&&) = delete;
    ~GraphicsPipelineCache();

    void Initialize();
    void Destroy();

    GraphicsPipelineRef GetOrCreate(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptor_table,
        const Array<FramebufferRef>& framebuffers,
        const RenderableAttributeSet& attributes);

private:
    GraphicsPipelineRef FindGraphicsPipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptor_table,
        const Array<FramebufferRef>& framebuffers,
        const RenderableAttributeSet& attributes);

    CachedPipelinesMap* m_cached_pipelines;
    Mutex m_mutex;
};

} // namespace hyperion

#endif
