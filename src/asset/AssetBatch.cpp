/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region AssetBatch

void AssetBatch::LoadAsync(uint num_batches)
{
    HYP_SCOPE;

    AssertThrowMsg(m_enqueued_assets != nullptr, "AssetBatch is in invalid state");

    if (m_enqueued_assets->Empty()) {
        return;
    }

    // partition each proc
    AssertThrow(m_procs.Size() == m_enqueued_assets->Size());

    const uint num_items = uint(m_procs.Size());

    num_batches = MathUtil::Max(num_batches, 1u);
    num_batches = MathUtil::Min(num_batches, num_items);

    const uint items_per_batch = num_items / num_batches;

    for (uint batch_index = 0; batch_index < num_batches; batch_index++) {
        const uint offset_index = batch_index * items_per_batch;

        const uint max_index = MathUtil::Min(offset_index + items_per_batch, num_items);
        AssertThrow(max_index >= offset_index);

        Array<UniquePtr<ProcessAssetFunctorBase>> batch_procs;
        batch_procs.Reserve(max_index - offset_index);

        for (uint i = offset_index; i < max_index; ++i) {
            AssertThrow(i < m_procs.Size());
            AssertThrow(m_procs[i] != nullptr);
            batch_procs.PushBack(std::move(m_procs[i]));
        }

        AddTask([asset_manager = m_asset_manager, batch_procs = std::move(batch_procs), asset_map = m_enqueued_assets.Get()]() mutable -> void
        {
            HYP_NAMED_SCOPE("Processing assets in batch");

            for (auto &proc : batch_procs) {
                proc->operator()(asset_manager, asset_map);
            }
        });
    }

    m_procs.Clear();

    TaskSystem::GetInstance().EnqueueBatch(this);

    m_asset_manager->AddPendingBatch(RefCountedPtrFromThis());
}

AssetMap AssetBatch::AwaitResults()
{
    AssertThrowMsg(m_enqueued_assets != nullptr, "AssetBatch is in invalid state");

    AwaitCompletion();

    return std::move(*m_enqueued_assets);
}

AssetMap AssetBatch::ForceLoad()
{
    AssertThrowMsg(m_enqueued_assets != nullptr, "AssetBatch is in invalid state");

    for (auto &proc : m_procs) {
        (*proc)(m_asset_manager, m_enqueued_assets.Get());
    }

    m_procs.Clear();

    return std::move(*m_enqueued_assets);
}

void AssetBatch::Add(const String &key, const String &path)
{
    AssertThrowMsg(
        IsCompleted(),
        "Cannot add assets while loading!"
    );

    AssertThrowMsg(
        m_enqueued_assets != nullptr,
        "AssetBatch is in invalid state"
    );

    if (!m_enqueued_assets->Emplace(key).second) {
        return;
    }

    UniquePtr<ProcessAssetFunctorBase> functor_ptr = m_asset_manager->CreateProcessAssetFunctor(
        key,
        path,
        &m_callbacks
    );

    AssertThrowMsg(
        functor_ptr != nullptr,
        "Failed to create ProcessAssetFunctor - perhaps the asset type is not registered or the path is invalid"
    );

    m_procs.PushBack(std::move(functor_ptr));
}

#pragma endregion AssetBatch

#pragma region AssetManager

UniquePtr<ProcessAssetFunctorBase> AssetManager::CreateProcessAssetFunctor(TypeID loader_type_id, const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr)
{
    auto it = m_functor_factories.Find(loader_type_id);
    AssertThrow(it != m_functor_factories.End());

    return it->second(key, path, callbacks_ptr);
}

#pragma endregion AssetManager

} // namespace hyperion