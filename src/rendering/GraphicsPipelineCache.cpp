/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

// For CompiledShader
#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

#pragma region CachedPipelinesMap

class CachedPipelinesMap : HashMap<RenderableAttributeSet, Array<GraphicsPipelineRef>>
{
public:
    using Base = HashMap<RenderableAttributeSet, Array<GraphicsPipelineRef>>;
    using ReverseMap = SparsePagedArray<RenderableAttributeSet>;

    using Base::Begin;
    using Base::begin;
    using Base::Empty;
    using Base::End;
    using Base::end;
    using Base::operator[];
    using Base::Erase;
    using Base::Find;

    CachedPipelinesMap()
        : Base()
    {
        cleanupIterator = reverseMap.End();
    }

    void Clear()
    {
        Base::Clear();
        reverseMap.Clear();
    }

    ReverseMap reverseMap;
    typename ReverseMap::Iterator cleanupIterator;
};

#pragma endregion CachedPipelinesMap

#pragma region GraphicsPipelineCache

GraphicsPipelineCache::GraphicsPipelineCache()
    : m_cachedPipelines(new CachedPipelinesMap())
{
}

GraphicsPipelineCache::~GraphicsPipelineCache()
{
    for (auto& it : *m_cachedPipelines)
    {
        for (GraphicsPipelineRef& pipeline : it.second)
        {
            SafeRelease(std::move(pipeline));
        }
    }

    m_cachedPipelines->Clear();

    Assert(m_cachedPipelines->Empty(), "Graphics pipeline cache not empty!");
    delete m_cachedPipelines;
}

GraphicsPipelineRef GraphicsPipelineCache::GetOrCreate(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    if (!shader.IsValid())
    {
        HYP_LOG(Rendering, Error, "Shader is null or invalid!");

        return nullptr;
    }

    const DescriptorTableDeclaration* descriptorTableDecl = nullptr;

    DescriptorTableRef table;

    if (descriptorTable)
    {
        table = descriptorTable;
        descriptorTableDecl = table->GetDeclaration();

        Assert(descriptorTableDecl != nullptr);
    }
    else
    {
        descriptorTableDecl = &shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    }

    GraphicsPipelineRef graphicsPipeline = FindGraphicsPipeline(
        shader,
        *descriptorTableDecl,
        framebuffers,
        attributes);

    if (graphicsPipeline.IsValid())
    {
        return graphicsPipeline;
    }

    if (!table)
    {
        table = g_renderBackend->MakeDescriptorTable(descriptorTableDecl);
        if (!table.IsValid())
        {
            HYP_LOG(Rendering, Error, "Failed to create descriptor table for shader: {}", shader->GetDebugName());

            return nullptr;
        }

        DeferCreate(table);
    }

    Proc<void(GraphicsPipelineRef graphicsPipeline)> newCallback([this, attributes](GraphicsPipelineRef graphicsPipeline)
    {
        Mutex::Guard guard(m_mutex);

        HYP_LOG(Rendering, Debug, "Adding graphics pipeline {} (debug name: {}) to cache with hash: {}", (void*)graphicsPipeline.Get(), graphicsPipeline->GetDebugName(), attributes.GetHashCode().Value());

        (*m_cachedPipelines)[attributes].PushBack(graphicsPipeline);
        m_cachedPipelines->reverseMap.Set(graphicsPipeline.header->index, attributes);
    });

    graphicsPipeline = g_renderBackend->MakeGraphicsPipeline(
        shader,
        table,
        framebuffers,
        attributes);

    struct RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)
        : RenderCommand
    {
        GraphicsPipelineRef graphicsPipeline;
        Proc<void(GraphicsPipelineRef)> callback;

        RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)(
            const GraphicsPipelineRef& graphicsPipeline,
            Proc<void(GraphicsPipelineRef)>&& callback)
            : graphicsPipeline(graphicsPipeline),
              callback(std::move(callback))
        {
            Assert(graphicsPipeline.IsValid());
        }

        virtual ~RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)() override
        {
            SafeRelease(std::move(graphicsPipeline));
        }

        virtual RendererResult operator()() override
        {
            HYP_GFX_CHECK(graphicsPipeline->Create());

            if (callback.IsValid())
            {
                // set initial lastFrame index so we don't delete it right away when cleaning up after the frame.
                graphicsPipeline->lastFrame = RenderApi_GetFrameCounter();
                
                callback(graphicsPipeline);
            }

            return RendererResult {};
        }
    };

    PUSH_RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache, graphicsPipeline, std::move(newCallback));

    return graphicsPipeline;
}

GraphicsPipelineRef GraphicsPipelineCache::FindGraphicsPipeline(
    const ShaderRef& shader,
    const DescriptorTableDeclaration& descriptorTableDecl,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    PerformanceClock clock;
    clock.Start();

    Mutex::Guard guard(m_mutex);

    const RenderableAttributeSet key = attributes;

    auto it = m_cachedPipelines->Find(key);

    if (it == m_cachedPipelines->End())
    {
        HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

        return nullptr;
    }

    const HashCode shaderHashCode = shader->GetCompiledShader()->GetHashCode();

    for (const GraphicsPipelineRef& pipeline : it->second)
    {
        if (pipeline->MatchesSignature(
                shader,
                descriptorTableDecl,
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

int GraphicsPipelineCache::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 currFrame = RenderApi_GetFrameCounter();

    Mutex::Guard guard(m_mutex);

    m_cachedPipelines->cleanupIterator = typename CachedPipelinesMap::ReverseMap::Iterator(
        &m_cachedPipelines->reverseMap,
        m_cachedPipelines->cleanupIterator.page,
        m_cachedPipelines->cleanupIterator.elem);

    const typename CachedPipelinesMap::ReverseMap::Iterator startIterator = m_cachedPipelines->cleanupIterator; // the iterator we started at - use it to check that we don't do duplicate checks

    int numCycles = 0;

    while (numCycles < maxIter)
    {
        // Loop around to the beginning of the container when the end is reached.
        if (m_cachedPipelines->cleanupIterator == m_cachedPipelines->reverseMap.End())
        {
            m_cachedPipelines->cleanupIterator = m_cachedPipelines->reverseMap.Begin();

            if (m_cachedPipelines->cleanupIterator == m_cachedPipelines->reverseMap.End())
            {
                break;
            }
        }

        RenderableAttributeSet& renderableAttributes = *m_cachedPipelines->cleanupIterator;

        auto it = m_cachedPipelines->Find(renderableAttributes);

        if (it == m_cachedPipelines->End())
        {
            m_cachedPipelines->Erase(renderableAttributes);

            m_cachedPipelines->cleanupIterator = m_cachedPipelines->reverseMap.Erase(m_cachedPipelines->cleanupIterator);
        }
        else
        {
            SizeType graphicsPipelineIdx = m_cachedPipelines->reverseMap.IndexOf(m_cachedPipelines->cleanupIterator);
            AssertDebug(graphicsPipelineIdx != -1);

            bool iteratorAdvanced = false;

            for (auto graphicsPipelineIt = it->second.Begin(); graphicsPipelineIt != it->second.End() && numCycles < maxIter; ++numCycles)
            {
                AssertDebug(graphicsPipelineIt->header != nullptr);
                
                GraphicsPipelineRef& graphicsPipeline = (*graphicsPipelineIt);

                if (graphicsPipeline->GetHeader_Internal()->index == uint32(graphicsPipelineIdx))
                {
                    // signed as graphics pipelines that haven't been used yet have -1 as their lastFrame value
                    const int64 frameDiff = int64(currFrame) - int64(graphicsPipeline->lastFrame);

                    if (frameDiff >= 10)
                    {
                        HYP_LOG(Rendering, Debug, "Removing graphics pipeline from cache for attributes {} (debug name: {}) as it has not been used in {} frames",
                            renderableAttributes.GetHashCode().Value(),
                            graphicsPipeline->GetDebugName(),
                            frameDiff);

                        SafeRelease(std::move(graphicsPipeline));
                        
                        graphicsPipelineIt = it->second.Erase(graphicsPipelineIt);

                        m_cachedPipelines->cleanupIterator = m_cachedPipelines->reverseMap.Erase(m_cachedPipelines->cleanupIterator);
                        iteratorAdvanced = true;

                        break;
                    }
                }

                ++graphicsPipelineIt;
            }

            if (!iteratorAdvanced)
            {
                ++m_cachedPipelines->cleanupIterator;
            }
        }

        ++numCycles;

        if (m_cachedPipelines->cleanupIterator == startIterator)
        {
            // we checked everything
            break;
        }
    }

    return numCycles;
}

#pragma endregion GraphicsPipelineCache

} // namespace hyperion
