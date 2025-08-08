/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

#pragma region AssetBatch

AssetBatch::AssetBatch(const Handle<AssetManager>& assetManager, const String& identifier)
    : TaskBatch(),
      m_assetManager(assetManager),
      m_identifier(identifier),
      m_assetMap(MakeUnique<AssetMap>())
{
    Assert(assetManager != nullptr);
}

AssetBatch::AssetBatch(const Handle<AssetManager>& assetManager)
    : AssetBatch(assetManager, String::empty)
{
}

void AssetBatch::LoadAsync(uint32 numBatches)
{
    HYP_SCOPE;

    Assert(m_assetMap != nullptr, "AssetBatch is in invalid state");
    Assert(m_assetManager != nullptr, "AssetBatch is in invalid state");

    // Set pool to use the asset manager's thread pool if one hasn't been set to override it.
    if (!TaskBatch::pool)
    {
        if (TaskThreadPool* threadPool = m_assetManager->GetThreadPool())
        {
            pool = threadPool;
        }
    }

    m_results.Clear();

    if (m_assetMap->Empty())
    {
        return;
    }

    // partition each proc
    Assert(m_procs.Size() == m_assetMap->Size());

    const uint32 numItems = uint32(m_procs.Size());

    numBatches = MathUtil::Max(numBatches, 1u);
    numBatches = MathUtil::Min(numBatches, numItems);

    const uint32 itemsPerBatch = numItems / numBatches;

    m_results.Resize(numBatches);

    for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        const uint32 offsetIndex = batchIndex * itemsPerBatch;

        const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);
        Assert(maxIndex >= offsetIndex);

        Array<UniquePtr<ProcessAssetFunctorBase>, DynamicAllocator> batchProcs;
        batchProcs.Reserve(maxIndex - offsetIndex);

        for (uint32 i = offsetIndex; i < maxIndex; ++i)
        {
            Assert(i < m_procs.Size());
            Assert(m_procs[i] != nullptr);
            batchProcs.PushBack(std::move(m_procs[i]));
        }

        AddTask([this, batchProcs = std::move(batchProcs), batchIndex]() mutable -> void
            {
                HYP_NAMED_SCOPE("Processing assets in batch");

                Array<TResult<void, AssetLoadError>> batchResults;
                batchResults.Reserve(batchProcs.Size());

                for (auto& proc : batchProcs)
                {
                    batchResults.PushBack((*proc)(*m_assetManager, *m_assetMap));
                }

                m_results[batchIndex] = std::move(batchResults);
            });
    }

    m_procs.Clear();

    TaskSystem::GetInstance().EnqueueBatch(this);

    m_assetManager->AddPendingBatch(RefCountedPtrFromThis());
}

AssetMap AssetBatch::AwaitResults()
{
    Assert(m_assetMap != nullptr, "AssetBatch is in invalid state");

    AwaitCompletion();

    // @TODO Handle m_results

    return std::move(*m_assetMap);
}

AssetMap AssetBatch::ForceLoad()
{
    Assert(m_assetMap != nullptr, "AssetBatch is in invalid state");

    for (auto& proc : m_procs)
    {
        (*proc)(*m_assetManager, *m_assetMap);
    }

    m_procs.Clear();

    return std::move(*m_assetMap);
}

void AssetBatch::Add(const String& key, const String& path)
{
    Assert(IsCompleted(), "Cannot add assets while loading!");
    Assert(m_assetMap != nullptr, "AssetBatch is in invalid state");
    
    UniquePtr<ProcessAssetFunctorBase> functorPtr = m_assetManager->CreateProcessAssetFunctor(
        m_identifier, key, path, &m_callbacks);
    
    if (!functorPtr)
    {
        HYP_LOG(Assets, Warning, "Cannot handle asset with path {} -- perhaps the asset type is not registered or the path is invalid",
            path);
        
        return;
    }

    if (!m_assetMap->Emplace(key).second)
    {
        return;
    }

    m_procs.PushBack(std::move(functorPtr));
}

#pragma endregion AssetBatch

#pragma region AssetManager

UniquePtr<ProcessAssetFunctorBase> AssetManager::CreateProcessAssetFunctor(TypeId loaderTypeId, const String& batchIdentifier, const String& key, const String& path, AssetBatchCallbacks* callbacksPtr)
{
    auto it = m_functorFactories.Find(loaderTypeId);
    Assert(it != m_functorFactories.End());

    return it->second(batchIdentifier, key, path, callbacksPtr);
}

#pragma endregion AssetManager

} // namespace hyperion
