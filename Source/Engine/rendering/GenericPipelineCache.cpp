/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/GenericPipelineCache.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RayTracingPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/threading/Threads.hpp>

namespace Hyperion {

static constexpr HashCode ComputeHashKey(Name shaderName, const ShaderPropertySet& properties)
{
    return shaderName.GetHashCode()
        .Combine(properties.GetHashCode());
}

#pragma region GenericPipelineCache

template <class PipelineType>
auto GenericPipelineCache<PipelineType>::GetOrCreate(Name shaderName, const ShaderPropertySet& properties) -> PipelineType*
{
    HYP_SCOPE;

    const HashCode key = ComputeHashKey(shaderName, properties);

    // First try to find existing
    {
        TSharedLock guard(m_mutex);

        auto it = m_keyToIndex.Find(key);
        if (it != m_keyToIndex.End())
        {
            CachedPipeline& cached = m_pipelines.Get(it->second);
            
            if (cached.pipeline.IsValid())
            {
                cached.pipeline->lastFrame = GetFrameCounter();
                return cached.pipeline;
            }
        }
    }

    // Not found, create new
    TSharedLock sharedLock(m_mutex);

    auto it = m_keyToIndex.Find(key);
    if (it != m_keyToIndex.End())
    {
        CachedPipeline& cached = m_pipelines.Get(it->second);
        
        if (cached.pipeline.IsValid())
        {
            cached.pipeline->lastFrame = GetFrameCounter();
            return cached.pipeline;
        }
    }

    // unlock for MakePipeline - don't want to deadlock if the thread we wait on calls ExpirePipelinesForShader().
    sharedLock.Reset();

    PipelineRefType pipeline = MakePipeline(shaderName, properties);

    if (!pipeline.IsValid())
    {
        return nullptr;
    }
    
    pipeline->lastFrame = GetFrameCounter();
    CheckResult(pipeline->Create());

    const uint32 index = m_idGenerator.Next() - 1;
    
    CachedPipeline cached {};
    cached.pipeline = pipeline;
    cached.key = key;

    // re-lock
    TUniqueLock uniqueLock(m_mutex);
    
    m_pipelines.Set(index, std::move(cached));
    m_keyToIndex[key] = index;

    return pipeline;
}

template <class PipelineType>
auto GenericPipelineCache<PipelineType>::Find(Name shaderName, const ShaderPropertySet& properties) const -> PipelineType*
{
    HYP_SCOPE;

    TSharedLock guard(m_mutex);

    const HashCode key = ComputeHashKey(shaderName, properties);

    auto it = m_keyToIndex.Find(key);
    if (it != m_keyToIndex.End())
    {
        const CachedPipeline& cached = m_pipelines.Get(it->second);
        
        if (cached.pipeline.IsValid() && cached.pipeline->IsCreated())
        {
            return cached.pipeline;
        }
    }

    return nullptr;
}

template <class PipelineType>
void GenericPipelineCache<PipelineType>::ExpirePipelinesForShader(const Shader* shader)
{
    if (!shader)
    {
        return;
    }

    TUniqueLock guard(m_mutex);

    // find all pipelines that use this shader and remove them, so they will be recreated with the new shader instance when requested again.
    for (auto it = m_keyToIndex.Begin(); it != m_keyToIndex.End();)
    {
        const HashCode key = it->first;

        // check if this pipeline corresponds to the shader we are expiring
        if (key == ComputeHashKey(shader->baseName, shader->properties))
        {
            const uint32 index = it->second;
            CachedPipeline& cached = m_pipelines.Get(index);

            if (cached.pipeline.IsValid())
            {
                EnqueueDeletion(std::move(cached.pipeline));
            }

            m_idGenerator.ReleaseId(index + 1);
            it = m_keyToIndex.Erase(it);

            m_pipelines.EraseAt(index);
        }
        else
        {
            ++it;
        }
    }
}

template <class PipelineType>
int GenericPipelineCache<PipelineType>::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;

    TUniqueLock guard(m_mutex);

    int numIterations = 0;
    const uint32 frameCounter = GetFrameCounter();

    m_cleanupIterator = typename PipelineStorage::Iterator(
        &m_pipelines,
        m_cleanupIterator.page,
        m_cleanupIterator.elem);

    if (m_cleanupIterator == m_pipelines.End())
    {
        m_cleanupIterator = m_pipelines.Begin();
    }

    while (numIterations < maxIter && m_cleanupIterator != m_pipelines.End())
    {
        CachedPipeline& cached = *m_cleanupIterator;

        ++numIterations;

        if (cached.pipeline->lastFrame == uint32(-1))
        {
            continue;
        }

        if (frameCounter - cached.pipeline->lastFrame > m_discardFrames)
        {
            auto keyToIndexIt = m_keyToIndex.Find(cached.key);
            Assert(keyToIndexIt != m_keyToIndex.End());
            
            m_idGenerator.ReleaseId(keyToIndexIt->second + 1);

            m_keyToIndex.Erase(keyToIndexIt);
            
            EnqueueDeletion(std::move(cached.pipeline));

            m_cleanupIterator = m_pipelines.Erase(m_cleanupIterator);

            continue;
        }
        
        ++m_cleanupIterator;
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
            EnqueueDeletion(std::move(cached.pipeline));
        }
    }

    m_pipelines.Clear();
    m_keyToIndex.Clear();
    m_cleanupIterator = m_pipelines.End();
}

// explicit template instantiations
template class GenericPipelineCache<ComputePipeline>;
template class GenericPipelineCache<RayTracingPipeline>;

#pragma endregion GenericPipelineCache

#pragma region ComputePipelineCache

ComputePipelineRef ComputePipelineCache::MakePipeline(Name shaderName, const ShaderPropertySet& properties)
{
    ShaderInstanceRef shader = g_renderInterface->shaderManager->GetOrCreate(shaderName, properties, {});
    
    if (!shader.IsValid())
    {
        return ComputePipelineRef::Null();
    }

    return g_renderInterface->MakeComputePipeline(shader);
}

#pragma endregion ComputePipelineCache

#pragma region RayTracingPipelineCache

RayTracingPipelineRef RayTracingPipelineCache::MakePipeline(Name shaderName, const ShaderPropertySet& properties)
{
    ShaderInstanceRef shader = g_renderInterface->shaderManager->GetOrCreate(shaderName, properties, {});
    
    if (!shader.IsValid())
    {
        return RayTracingPipelineRef::Null();
    }

    return g_renderInterface->MakeRayTracingPipeline(shader);
}

#pragma endregion RayTracingPipelineCache

} // namespace Hyperion
