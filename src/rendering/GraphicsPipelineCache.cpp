/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/DeferredScope.hpp>

// For CompiledShader
#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateGraphicsPipeline) : renderer::RenderCommand
{
    GraphicsPipelineRef                     pipeline;
    RenderPassRef                           render_pass;
    Array<FramebufferRef>                   framebuffers;
    RenderableAttributeSet                  attributes;
    Proc<void(const GraphicsPipelineRef &)> callback;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        const GraphicsPipelineRef &pipeline,
        const RenderPassRef &render_pass,
        const Array<FramebufferRef> &framebuffers,
        const RenderableAttributeSet &attributes,
        Proc<void(const GraphicsPipelineRef &)> &&callback
    ) : pipeline(pipeline),
        render_pass(render_pass),
        framebuffers(framebuffers),
        attributes(attributes),
        callback(std::move(callback))
    {
        AssertThrow(pipeline.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateGraphicsPipeline)() override = default;

    virtual RendererResult operator()() override
    {
        HYP_DEFER({
            if (callback.IsValid()) {
                callback(pipeline);
            }
        });

        AssertThrow(pipeline->GetDescriptorTable().IsValid());

        for (auto &it : pipeline->GetDescriptorTable()->GetSets()) {
            for (const auto &set : it) {
                AssertThrow(set.IsValid());
                AssertThrowMsg(set->GetPlatformImpl().GetVkDescriptorSetLayout() != VK_NULL_HANDLE,
                    "Invalid descriptor set for %s", set.GetName().LookupString());
            }
        }

        pipeline->SetVertexAttributes(attributes.GetMeshAttributes().vertex_attributes);
        pipeline->SetTopology(attributes.GetMeshAttributes().topology);
        pipeline->SetCullMode(attributes.GetMaterialAttributes().cull_faces);
        pipeline->SetFillMode(attributes.GetMaterialAttributes().fill_mode);
        pipeline->SetBlendFunction(attributes.GetMaterialAttributes().blend_function);
        pipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencil_function);
        pipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_TEST));
        pipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_WRITE));
        pipeline->SetRenderPass(render_pass);
        pipeline->SetFramebuffers(framebuffers);

        HYPERION_BUBBLE_ERRORS(pipeline->Create(g_engine->GetGPUDevice()));

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

GraphicsPipelineRef GraphicsPipelineCache::GetOrCreate(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    const RenderPassRef &render_pass,
    const Array<FramebufferRef> &framebuffers,
    const RenderableAttributeSet &attributes,
    Proc<void(const GraphicsPipelineRef &)> &&on_ready_callback
)
{
    HYP_SCOPE;

    GraphicsPipelineRef graphics_pipeline = FindGraphicsPipeline(
        shader,
        descriptor_table,
        render_pass,
        framebuffers,
        attributes
    );

    if (graphics_pipeline.IsValid()) {
        if (on_ready_callback.IsValid()) {
            on_ready_callback(graphics_pipeline);
        }

        return graphics_pipeline;
    }

    if (descriptor_table.IsValid()) {
        graphics_pipeline = MakeRenderObject<GraphicsPipeline>(ShaderRef::unset, descriptor_table);
    } else {
        graphics_pipeline = MakeRenderObject<GraphicsPipeline>();
    }

    if (shader.IsValid()) {
        graphics_pipeline->SetShader(shader);
    }

    Proc<void(const GraphicsPipelineRef &)> new_callback = [this, attributes, on_ready_callback = std::move(on_ready_callback)](const GraphicsPipelineRef &graphics_pipeline)
    {
        {
            Mutex::Guard guard(m_mutex);

            m_cached_pipelines[attributes].PushBack(graphics_pipeline);
        }

        if (on_ready_callback.IsValid()) {
            on_ready_callback(graphics_pipeline);
        }
    };

    PUSH_RENDER_COMMAND(
        CreateGraphicsPipeline,
        graphics_pipeline,
        render_pass,
        framebuffers,
        attributes,
        std::move(new_callback)
    );

    return graphics_pipeline;
}

GraphicsPipelineRef GraphicsPipelineCache::FindGraphicsPipeline(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    const RenderPassRef &render_pass,
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
        if (pipeline->GetShader()->GetCompiledShader()->GetHashCode() == shader_hash_code
            && pipeline->GetDescriptorTable()->GetDeclaration().GetHashCode() == descriptor_table_hash_code
            && pipeline->GetRenderPass() == render_pass
            && pipeline->GetFramebuffers() == framebuffers)
        {
            return pipeline;
        }
    }

    return nullptr;
}

#pragma endregion GraphicsPipelineCache

} // namespace hyperion
