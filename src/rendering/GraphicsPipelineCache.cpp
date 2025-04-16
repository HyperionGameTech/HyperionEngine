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

#include <core/algorithm/Map.hpp>

// For CompiledShader
#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateGraphicsPipeline) : renderer::RenderCommand
{
    ShaderRef                               shader;
    DescriptorTableRef                      descriptor_table;
    Array<FramebufferRef>                   framebuffers;
    RenderableAttributeSet                  attributes;
    Proc<void(const GraphicsPipelineRef &)> callback;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table,
        const Array<FramebufferRef> &framebuffers,
        const RenderableAttributeSet &attributes,
        Proc<void(const GraphicsPipelineRef &)> &&callback
    ) : shader(shader),
        descriptor_table(descriptor_table),
        framebuffers(framebuffers),
        attributes(attributes),
        callback(std::move(callback))
    {
    }

    virtual ~RENDER_COMMAND(CreateGraphicsPipeline)() override = default;

    virtual RendererResult operator()() override
    {
        GraphicsPipelineRef pipeline;

        HYP_DEFER({
            if (callback.IsValid()) {
                callback(pipeline);
            }
        });

        pipeline = g_rendering_api->MakeGraphicsPipeline(
            shader,
            descriptor_table,
            framebuffers,
            attributes
        );

        HYPERION_BUBBLE_ERRORS(pipeline->Create());

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region GraphicsPipelineCache

GraphicsPipelineCache::GraphicsPipelineCache() = default;

GraphicsPipelineCache::~GraphicsPipelineCache()
{
    AssertThrowMsg(m_cached_pipelines.Empty(), "Graphics pipeline cache not empty!");
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

    for (auto &it : m_cached_pipelines) {
        for (GraphicsPipelineRef &pipeline : it.second) {
            SafeRelease(std::move(pipeline));
        }
    }

    m_cached_pipelines.Clear();
}

Task<GraphicsPipelineRef> GraphicsPipelineCache::GetOrCreate(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    const Array<FramebufferRef> &framebuffers,
    const RenderableAttributeSet &attributes
)
{
    HYP_SCOPE;

    Task<GraphicsPipelineRef> task;

    GraphicsPipelineRef graphics_pipeline = FindGraphicsPipeline(
        shader,
        descriptor_table,
        framebuffers,
        attributes
    );

    if (graphics_pipeline.IsValid()) {
        task.Fulfill(graphics_pipeline);

        return task;
    }

    Proc<void(const GraphicsPipelineRef &)> new_callback = [this, attributes, task_executor = task.Initialize()](const GraphicsPipelineRef &graphics_pipeline)
    {
        {
            Mutex::Guard guard(m_mutex);

            m_cached_pipelines[attributes].PushBack(graphics_pipeline);
        }

        task_executor->Fulfill(graphics_pipeline);
    };

    PUSH_RENDER_COMMAND(
        CreateGraphicsPipeline,
        shader,
        descriptor_table,
        framebuffers,
        attributes,
        std::move(new_callback)
    );

    return task;
}

GraphicsPipelineRef GraphicsPipelineCache::FindGraphicsPipeline(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    const Array<FramebufferRef> &framebuffers,
    const RenderableAttributeSet &attributes
)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    const RenderableAttributeSet key = attributes;

    auto it = m_cached_pipelines.Find(key);

    if (it == m_cached_pipelines.End()) {
        return nullptr;
    }

    const HashCode shader_hash_code = shader->GetCompiledShader()->GetHashCode();
    const HashCode descriptor_table_hash_code = descriptor_table->GetDeclaration().GetHashCode();

    for (const GraphicsPipelineRef &pipeline : it->second) {
        if (pipeline->MatchesSignature(
            shader,
            descriptor_table,
            Map(framebuffers, [](const FramebufferRef &framebuffer) { return static_cast<const FramebufferBase *>(framebuffer.Get()); }),
            attributes
        ))
        {
            return pipeline;
        }
    }

    return nullptr;
}

#pragma endregion GraphicsPipelineCache

} // namespace hyperion
