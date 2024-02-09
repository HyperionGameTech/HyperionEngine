#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void AssetBatch::LoadAsync(uint num_batches)
{
    AssertThrowMsg(enqueued_assets != nullptr, "AssetBatch is in invalid state");

    if (enqueued_assets->Empty()) {
        return;
    }

    // partition each proc
    AssertThrow(procs.Size() == enqueued_assets->Size());

    const uint num_items = uint(procs.Size());

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
            AssertThrow(i < procs.Size());
            AssertThrow(procs[i] != nullptr);
            batch_procs.PushBack(std::move(procs[i]));
        }

        AddTask([asset_manager = asset_manager, batch_procs = std::move(batch_procs), asset_map = enqueued_assets.Get()](...) mutable
        {
            for (auto &proc : batch_procs) {
                proc->operator()(asset_manager, asset_map);
            }
        });
    }

    procs.Clear();

    TaskSystem::GetInstance().EnqueueBatch(this);
}

AssetMap AssetBatch::AwaitResults()
{
    AssertThrowMsg(enqueued_assets != nullptr, "AssetBatch is in invalid state");

    AwaitCompletion();

    AssetMap results = std::move(*enqueued_assets);
    // enqueued_assets is cleared now

    return results;
}

AssetMap AssetBatch::ForceLoad()
{
    AssertThrowMsg(enqueued_assets != nullptr, "AssetBatch is in invalid state");

    for (auto &proc : procs) {
        (*proc)(asset_manager, enqueued_assets.Get());
    }

    procs.Clear();

    AssetMap results = std::move(*enqueued_assets);
    // enqueued_assets is cleared now

    return results;
}

void AssetBatch::Add(const String &key, const String &path)
{
    AssertThrowMsg(
        IsCompleted(),
        "Cannot add assets while loading!"
    );

    AssertThrowMsg(
        enqueued_assets != nullptr,
        "AssetBatch is in invalid state"
    );

    enqueued_assets->Set(key, EnqueuedAsset {
        .path = path
    });

    UniquePtr<ProcessAssetFunctorBase> functor_ptr = asset_manager->CreateProcessAssetFunctor(
        key,
        path,
        &callbacks
    );

    AssertThrowMsg(
        functor_ptr != nullptr,
        "Failed to create ProcessAssetFunctor - perhaps the asset type is not registered or the path is invalid"
    );

    procs.PushBack(std::move(functor_ptr));
}

static RC<AssetBatch> ScriptCreateAssetBatch(void *)
{
    return g_asset_manager->CreateBatch();
}

} // namespace hyperion::v2