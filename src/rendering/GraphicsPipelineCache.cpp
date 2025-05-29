/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/algorithm/Map.hpp>

// For CompiledShader
#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region CachedPipelinesMap

struct CachedPipelinesMap : HashMap<RenderableAttributeSet, Array<GraphicsPipelineRef>>
{
};

#pragma endregion CachedPipelinesMap

#pragma region GraphicsPipelineCache

GraphicsPipelineCache::GraphicsPipelineCache()
    : m_cached_pipelines(new CachedPipelinesMap())
{
}

GraphicsPipelineCache::~GraphicsPipelineCache()
{
    AssertThrowMsg(m_cached_pipelines->Empty(), "Graphics pipeline cache not empty!");
    delete m_cached_pipelines;
}

void GraphicsPipelineCache::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
}

void GraphicsPipelineCache::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    Mutex::Guard guard(m_mutex);

    for (auto& it : *m_cached_pipelines)
    {
        for (GraphicsPipelineRef& pipeline : it.second)
        {
            SafeRelease(std::move(pipeline));
        }
    }

    m_cached_pipelines->Clear();
}

GraphicsPipelineRef GraphicsPipelineCache::GetOrCreate(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptor_table,
    const Array<FramebufferRef>& framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    GraphicsPipelineRef graphics_pipeline = FindGraphicsPipeline(
        shader,
        descriptor_table,
        framebuffers,
        attributes);

    if (graphics_pipeline.IsValid())
    {
        return graphics_pipeline;
    }

    Proc<void(const GraphicsPipelineRef&)> new_callback = [this, attributes](const GraphicsPipelineRef& graphics_pipeline)
    {
        Mutex::Guard guard(m_mutex);

        (*m_cached_pipelines)[attributes].PushBack(graphics_pipeline);
    };

    graphics_pipeline = g_rendering_api->MakeGraphicsPipeline(
        shader,
        descriptor_table,
        framebuffers,
        attributes);

    struct RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)
        : renderer::RenderCommand
    {
        GraphicsPipelineRef graphics_pipeline;
        Proc<void(const GraphicsPipelineRef&)> callback;

        RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)(
            const GraphicsPipelineRef& graphics_pipeline,
            Proc<void(const GraphicsPipelineRef&)>&& callback)
            : graphics_pipeline(graphics_pipeline),
              callback(std::move(callback))
        {
            AssertThrow(graphics_pipeline.IsValid());
        }

        virtual ~RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)() override
        {
            SafeRelease(std::move(graphics_pipeline));
        }

        virtual RendererResult operator()() override
        {
            HYPERION_BUBBLE_ERRORS(graphics_pipeline->Create());

            if (callback.IsValid())
            {
                callback(graphics_pipeline);
            }

            return RendererResult {};
        }
    };

    PUSH_RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache, graphics_pipeline, std::move(new_callback));

    return graphics_pipeline;
}

GraphicsPipelineRef GraphicsPipelineCache::FindGraphicsPipeline(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptor_table,
    const Array<FramebufferRef>& framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    const RenderableAttributeSet key = attributes;

    auto it = m_cached_pipelines->Find(key);

    if (it == m_cached_pipelines->End())
    {
        return nullptr;
    }

    if (!descriptor_table->GetDeclaration())
    {
        HYP_LOG(Rendering, Error, "Descriptor table declaration is null for shader: {}", shader->GetCompiledShader()->GetName());

        return nullptr;
    }

    const HashCode shader_hash_code = shader->GetCompiledShader()->GetHashCode();

    const HashCode descriptor_table_hash_code = descriptor_table->GetDeclaration()->GetHashCode();

    for (const GraphicsPipelineRef& pipeline : it->second)
    {
        if (pipeline->MatchesSignature(
                shader,
                descriptor_table,
                Map(framebuffers, [](const FramebufferRef& framebuffer)
                    {
                        return static_cast<const FramebufferBase*>(framebuffer.Get());
                    }),
                attributes))
        {
            return pipeline;
        }
    }

    return nullptr;
}

#pragma endregion GraphicsPipelineCache

} // namespace hyperion
