/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region AssetBatch

void AssetBatch::LoadAsync(uint32 num_batches)
{
    HYP_SCOPE;

    AssertThrowMsg(m_asset_map != nullptr, "AssetBatch is in invalid state");

    m_results.Clear();

    if (m_asset_map->Empty())
    {
        return;
    }

    // partition each proc
    AssertThrow(m_procs.Size() == m_asset_map->Size());

    const uint32 num_items = uint32(m_procs.Size());

    num_batches = MathUtil::Max(num_batches, 1u);
    num_batches = MathUtil::Min(num_batches, num_items);

    const uint32 items_per_batch = num_items / num_batches;

    m_results.Resize(num_batches);

    for (uint32 batch_index = 0; batch_index < num_batches; batch_index++)
    {
        const uint32 offset_index = batch_index * items_per_batch;

        const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);
        AssertThrow(max_index >= offset_index);

        Array<UniquePtr<ProcessAssetFunctorBase>> batch_procs;
        batch_procs.Reserve(max_index - offset_index);

        for (uint32 i = offset_index; i < max_index; ++i)
        {
            AssertThrow(i < m_procs.Size());
            AssertThrow(m_procs[i] != nullptr);
            batch_procs.PushBack(std::move(m_procs[i]));
        }

        AddTask([this, batch_procs = std::move(batch_procs), batch_index]() mutable -> void
            {
                HYP_NAMED_SCOPE("Processing assets in batch");

                Array<TResult<void, AssetLoadError>> batch_results;
                batch_results.Reserve(batch_procs.Size());

                for (auto& proc : batch_procs)
                {
                    batch_results.PushBack((*proc)(*m_asset_manager, *m_asset_map));
                }

                m_results[batch_index] = std::move(batch_results);
            });
    }

    m_procs.Clear();

    TaskSystem::GetInstance().EnqueueBatch(this);

    m_asset_manager->AddPendingBatch(RefCountedPtrFromThis());
}

AssetMap AssetBatch::AwaitResults()
{
    AssertThrowMsg(m_asset_map != nullptr, "AssetBatch is in invalid state");

    AwaitCompletion();

    // @TODO Handle m_results

    return std::move(*m_asset_map);
}

AssetMap AssetBatch::ForceLoad()
{
    AssertThrowMsg(m_asset_map != nullptr, "AssetBatch is in invalid state");

    for (auto& proc : m_procs)
    {
        (*proc)(*m_asset_manager, *m_asset_map);
    }

    m_procs.Clear();

    return std::move(*m_asset_map);
}

void AssetBatch::Add(const String& key, const String& path)
{
    AssertThrowMsg(
        IsCompleted(),
        "Cannot add assets while loading!");

    AssertThrowMsg(m_asset_map != nullptr, "AssetBatch is in invalid state");

    if (!m_asset_map->Emplace(key).second)
    {
        return;
    }

    UniquePtr<ProcessAssetFunctorBase> functor_ptr = m_asset_manager->CreateProcessAssetFunctor(
        key,
        path,
        &m_callbacks);

    AssertThrowMsg(
        functor_ptr != nullptr,
        "Failed to create ProcessAssetFunctor - perhaps the asset type is not registered or the path is invalid");

    m_procs.PushBack(std::move(functor_ptr));
}

#pragma endregion AssetBatch

#pragma region AssetManager

UniquePtr<ProcessAssetFunctorBase> AssetManager::CreateProcessAssetFunctor(TypeID loader_type_id, const String& key, const String& path, AssetBatchCallbacks* callbacks_ptr)
{
    auto it = m_functor_factories.Find(loader_type_id);
    AssertThrow(it != m_functor_factories.End());

    return it->second(key, path, callbacks_ptr);
}

#pragma endregion AssetManager

} // namespace hyperion