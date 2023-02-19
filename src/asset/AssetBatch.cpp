#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void AssetBatch::LoadAsync(UInt num_batches)
{
    AssertThrowMsg(enqueued_assets != nullptr, "AssetBatch is in invalid state");

    if (enqueued_assets->Empty()) {
        return;
    }

    // partition each proc
    AssertThrow(procs.Size() == enqueued_assets->Size());

    const UInt num_items = UInt(procs.Size());

    num_batches = MathUtil::Max(num_batches, 1u);
    num_batches = MathUtil::Min(num_batches, num_items);

    const UInt items_per_batch = num_items / num_batches;

    for (UInt batch_index = 0; batch_index < num_batches; batch_index++) {
        const UInt offset_index = batch_index * items_per_batch;
        const UInt max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

        AssertThrow(max_index >= offset_index);

        Array<Proc<void, AssetManager *, AssetMap *>> batch_procs;
        batch_procs.Reserve(max_index - offset_index);

        for (UInt i = offset_index; i < max_index; ++i) {
            batch_procs.PushBack(std::move(procs[i]));
        }

        AddTask([asset_manager = asset_manager, batch_procs = std::move(batch_procs), asset_map = enqueued_assets.Get()](...) mutable {
            for (auto &proc : batch_procs) {
                proc(asset_manager, asset_map);
            }
        });
    }

    procs.Clear();

    g_engine->task_system.EnqueueBatch(this);
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
        proc(asset_manager, enqueued_assets.Get());
    }

    procs.Clear();

    AssetMap results = std::move(*enqueued_assets);
    // enqueued_assets is cleared now

    return results;
}

} // namespace hyperion::v2