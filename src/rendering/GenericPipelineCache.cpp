/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/GenericPipelineCache.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/raytracing/RenderRaytracingPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>

namespace Hyperion {

#pragma region GenericPipelineCache

template <class PipelineType>
auto GenericPipelineCache<PipelineType>::GetOrCreate(const ShaderDefinition& shaderDefinition) -> PipelineRefType
{
    HYP_SCOPE;

    const HashCode key = shaderDefinition.GetHashCode();

    // First try to find existing
    {
        TSharedLock guard(m_mutex);

        auto it = m_keyToIndex.Find(key);
        if (it != m_keyToIndex.End())
        {
            CachedPipeline& cached = m_pipelines.Get(it->second);
            
            if (cached.pipeline.IsValid())
            {
                cached.pipeline->lastFrame = RenderApi::GetFrameCounter();
                return cached.pipeline;
            }
        }
    }

    // Not found, create new
    TUniqueLock guard(m_mutex);

    auto it = m_keyToIndex.Find(key);
    if (it != m_keyToIndex.End())
    {
        CachedPipeline& cached = m_pipelines.Get(it->second);
        
        if (cached.pipeline.IsValid())
        {
            cached.pipeline->lastFrame = RenderApi::GetFrameCounter();
            return cached.pipeline;
        }
    }

    PipelineRefType pipeline = MakePipeline(shaderDefinition);

    if (!pipeline.IsValid())
    {
        return PipelineRefType::Null();
    }

    CheckResult(pipeline->Create());

    const uint32 index = m_idGenerator.Next() - 1;
    
    CachedPipeline cached {};
    cached.pipeline = pipeline;
    cached.key = key;
    
    m_pipelines.Set(index, std::move(cached));
    m_keyToIndex[key] = index;

    struct CreatePipelineCommand : RenderCommand
    {
        PipelineType* pipeline;

        CreatePipelineCommand(PipelineType* pipeline)
            : pipeline(pipeline)
        {
        }

        virtual ~CreatePipelineCommand() override = default;

        virtual RendererResult operator()() override
        {
            CheckResultOrReturn(pipeline->Create());
            
            pipeline->lastFrame = RenderApi::GetFrameCounter();

            return RendererResult {};
        }
    };

    PUSH_RENDER_COMMAND(CreatePipelineCommand, pipeline.Get());

    return pipeline;
}

template <class PipelineType>
auto GenericPipelineCache<PipelineType>::Find(const ShaderDefinition& shaderDefinition) const -> PipelineRefType
{
    HYP_SCOPE;

    TSharedLock guard(m_mutex);

    const HashCode key = shaderDefinition.GetHashCode();

    auto it = m_keyToIndex.Find(key);
    if (it != m_keyToIndex.End())
    {
        const CachedPipeline& cached = m_pipelines.Get(it->second);
        
        if (cached.pipeline.IsValid() && cached.pipeline->IsCreated())
        {
            return cached.pipeline;
        }
    }

    return PipelineRefType::Null();
}

template <class PipelineType>
int GenericPipelineCache<PipelineType>::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;

    TUniqueLock guard(m_mutex);

    int numIterations = 0;
    const uint32 frameCounter = RenderApi::GetFrameCounter();

    if (m_cleanupIterator == m_pipelines.End())
    {
        m_cleanupIterator = m_pipelines.Begin();
    }

    while (numIterations < maxIter && m_cleanupIterator != m_pipelines.End())
    {
        const SizeType index = m_pipelines.IndexOf(m_cleanupIterator);
        CachedPipeline& cached = *m_cleanupIterator;

        ++m_cleanupIterator;
        ++numIterations;

        if (cached.pipeline->lastFrame == uint32(-1))
        {
            continue;
        }

        if (frameCounter - cached.pipeline->lastFrame > m_discardFrames)
        {
            m_keyToIndex.Erase(cached.key);
            
            SafeDelete(std::move(cached.pipeline));

            m_pipelines.EraseAt(index);
            
            m_idGenerator.ReleaseId(index + 1);
        }
    }

    return numIterations;
}

template <class PipelineType>
void GenericPipelineCache<PipelineType>::Clear()
{
    TUniqueLock guard(m_mutex);

    for (CachedPipeline& cached : m_pipelines)
    {
        if (cached.pipeline.IsValid())
        {
            SafeDelete(std::move(cached.pipeline));
        }
    }

    m_pipelines.Clear();
    m_keyToIndex.Clear();
    m_cleanupIterator = m_pipelines.End();
}

// explicit template instantiations
template class GenericPipelineCache<ComputePipeline>;
template class GenericPipelineCache<RaytracingPipeline>;

#pragma endregion GenericPipelineCache

#pragma region ComputePipelineCache

ComputePipelineRef ComputePipelineCache::MakePipeline(const ShaderDefinition& shaderDefinition)
{
    ShaderRef shader = g_shaderManager->GetOrCreate(shaderDefinition);
    
    if (!shader.IsValid())
    {
        return ComputePipelineRef::Null();
    }

    return g_renderBackend->MakeComputePipeline(shader, DescriptorTableRef::Null());
}

#pragma endregion ComputePipelineCache

#pragma region RaytracingPipelineCache

RaytracingPipelineRef RaytracingPipelineCache::MakePipeline(const ShaderDefinition& shaderDefinition)
{
    ShaderRef shader = g_shaderManager->GetOrCreate(shaderDefinition);
    
    if (!shader.IsValid())
    {
        return RaytracingPipelineRef::Null();
    }

    return g_renderBackend->MakeRaytracingPipeline(shader, DescriptorTableRef::Null());
}

#pragma endregion RaytracingPipelineCache

} // namespace Hyperion
