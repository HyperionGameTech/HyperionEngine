/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>

#include <core/profiling/PerformanceClock.hpp>

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
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    if (!shader.IsValid())
    {
        HYP_LOG(Rendering, Error, "Shader is null or invalid!");

        return nullptr;
    }

    const DescriptorTableDeclaration* descriptor_table_decl = nullptr;

    DescriptorTableRef table;

    if (descriptor_table)
    {
        table = descriptor_table;
        descriptor_table_decl = table->GetDeclaration();

        AssertThrow(descriptor_table_decl != nullptr);
    }
    else
    {
        descriptor_table_decl = &shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    }

    GraphicsPipelineRef graphics_pipeline = FindGraphicsPipeline(
        shader,
        *descriptor_table_decl,
        framebuffers,
        attributes);

    if (graphics_pipeline.IsValid())
    {
        return graphics_pipeline;
    }

    if (!table)
    {
        table = g_render_backend->MakeDescriptorTable(descriptor_table_decl);
        if (!table.IsValid())
        {
            HYP_LOG(Rendering, Error, "Failed to create descriptor table for shader: {}", shader->GetDebugName());

            return nullptr;
        }

        DeferCreate(table);
    }

    Proc<void(const GraphicsPipelineRef&)> new_callback = [this, attributes](const GraphicsPipelineRef& graphics_pipeline)
    {
        Mutex::Guard guard(m_mutex);

        HYP_LOG(Rendering, Info, "Adding graphics pipeline to cache ({})", attributes.GetHashCode().Value());

        (*m_cached_pipelines)[attributes].PushBack(graphics_pipeline);
    };

    graphics_pipeline = g_render_backend->MakeGraphicsPipeline(
        shader,
        table,
        framebuffers,
        attributes);

    struct RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)
        : RenderCommand
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
    const DescriptorTableDeclaration& descriptor_table_decl,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    PerformanceClock clock;
    clock.Start();

    Mutex::Guard guard(m_mutex);

    const RenderableAttributeSet key = attributes;

    auto it = m_cached_pipelines->Find(key);

    if (it == m_cached_pipelines->End())
    {
        HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

        return nullptr;
    }

    const HashCode shader_hash_code = shader->GetCompiledShader()->GetHashCode();

    for (const GraphicsPipelineRef& pipeline : it->second)
    {
        if (pipeline->MatchesSignature(
                shader,
                descriptor_table_decl,
                Map(framebuffers, [](const FramebufferRef& framebuffer)
                    {
                        return static_cast<const FramebufferBase*>(framebuffer.Get());
                    }),
                attributes))
        {
            HYP_LOG(Rendering, Info, "GraphicsPipelineCache cache hit ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

            return pipeline;
        }
    }

    HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

    return nullptr;
}

#pragma endregion GraphicsPipelineCache

} // namespace hyperion
