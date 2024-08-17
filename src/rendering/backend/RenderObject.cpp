/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Platform;

template <>
renderer::platform::Device<Platform::CURRENT> *RenderObjectDeleter<Platform::CURRENT>::GetDevice()
{
    return g_engine->GetGPUDevice();
}

template <>
void RenderObjectDeleter<Platform::CURRENT>::Initialize()
{
    // Command buffer should be deleted first so that no
    // buffers that will be deleted are used in the command buffers
    (void)GetQueue<CommandBuffer>();
}

template <>
void RenderObjectDeleter<Platform::CURRENT>::Iterate()
{
    HYP_NAMED_SCOPE("Destroy enqueued rendering resources");

    DeletionQueueBase **queue = queues.Data();

    while (*queue) {
        (*queue)->Iterate();
        ++queue;
    }
}

template <>
void RenderObjectDeleter<Platform::CURRENT>::RemoveAllNow(bool force)
{
    HYP_NAMED_SCOPE("Force delete all rendering resources");

    FixedArray<AtomicVar<int32> *, queues.Size()> queue_num_items { };

    { // init atomic vars
        DeletionQueueBase **queue = queues.Data();
        for (uint queue_index = 0; *queue; ++queue_index, ++queue) {
            queue_num_items[queue_index] = &(*queue)->num_items;
        }
    }

    // Loop until all queues are empty
    while (queue_num_items.Any([](AtomicVar<int32> *count) { return count != nullptr && count->Get(MemoryOrder::ACQUIRE) > 0; })) {
        for (DeletionQueueBase **queue = queues.Data(); *queue; ++queue) {
            (*queue)->RemoveAllNow(force);
        }
    }
}

} // namespace hyperion