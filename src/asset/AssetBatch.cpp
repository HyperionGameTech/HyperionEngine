#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void AssetBatch::LoadAsync(UInt num_batches)
{
    if (enqueued_assets.Empty()) {
        return;
    }

    // partition each proc
    AssertThrow(procs.Size() == enqueued_assets.Size());

    const UInt num_items = static_cast<UInt>(procs.Size());

    if (num_items == 0) {
        return;
    }

    num_batches = MathUtil::Min(num_batches, num_items);
    num_batches = MathUtil::Max(num_batches, 1u);

    const UInt items_per_batch = num_items / num_batches;

    for (UInt batch_index = 0; batch_index < num_batches; batch_index++) {
        AddTask([&procs = this->procs, batch_index, items_per_batch, num_items](...) {
            const UInt offset_index = batch_index * items_per_batch;
            const UInt max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

            for (UInt i = offset_index; i < max_index; ++i) {
                procs[i]();
            }
        });
    }

    asset_manager.m_engine->task_system.EnqueueBatch(this);
}

AssetMap AssetBatch::AwaitResults()
{
    AwaitCompletion();

    auto results = std::move(enqueued_assets);
    procs.Clear();

    // enqueued_assets is cleared now

    return results;
}

AssetMap AssetBatch::ForceLoad()
{
    for (auto &proc : procs) {
        proc();
    }

    auto results = std::move(enqueued_assets);
    procs.Clear();

    // enqueued_assets is cleared now

    return results;
}

} // namespace hyperion::v2