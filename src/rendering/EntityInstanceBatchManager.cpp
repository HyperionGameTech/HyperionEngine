/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/EntityInstanceBatchManager.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region EntityInstanceBatchList

static bool IsBlockEmpty(EntityInstanceBatchList::Block &block)
{
    return block.count.Get(MemoryOrder::ACQUIRE) != 0;
}

static bool IsBlockFull(EntityInstanceBatchList::Block &block)
{
    return block.count.Get(MemoryOrder::ACQUIRE) == EntityInstanceBatchList::elements_per_block;
}

EntityInstanceBatchList::EntityInstanceBatchList()
{
    // Make sure one is always there
    m_blocks.EmplaceBack();

    for (GPUBufferRef &buffer : m_gpu_buffers) {
        buffer = MakeRenderObject<renderer::GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
        DeferCreate(buffer, g_engine->GetGPUDevice(), sizeof(EntityInstanceBatch) * elements_per_block);
    }
}

EntityInstanceBatchList::~EntityInstanceBatchList()
{
    for (GPUBufferRef &buffer : m_gpu_buffers) {
        SafeRelease(std::move(buffer));
    }
}

EntityInstanceBatch &EntityInstanceBatchList::GetEntityInstanceBatch(uint32 index)
{
    AssertThrow(index != 0);

    const uint32 block_index = (index - 1) / elements_per_block;

    if (block_index == 0) {
        return m_blocks.Front().entity_instance_batches[(index - 1) % elements_per_block];
    }

    Mutex::Guard guard(m_mutex);
    HYP_MT_CHECK_READ(m_data_race_detector);

    AssertThrow(block_index < m_blocks.Size());

    uint32 i = 0;

    for (Block &block : m_blocks) {
        if (i == block_index) {
            return block.entity_instance_batches[(index - 1) % elements_per_block];
        }

        ++i;
    }

    HYP_FAIL("Element out of bounds: %u", index);
}

uint32 EntityInstanceBatchList::AcquireIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    uint32 index = 0;
    uint32 block_index = 0;

    Block *block_ptr = &m_blocks.Front();

    // temp
    Mutex::Guard guard(m_mutex);
    HYP_MT_CHECK_READ(m_data_race_detector);

    if (IsBlockFull(*block_ptr)) {
        // Mutex::Guard guard(m_mutex);
        // HYP_MT_CHECK_READ(m_data_race_detector);

        block_ptr = nullptr;

        for (Block &block : m_blocks) {
            if (!IsBlockFull(block)) {
                block_ptr = &block;

                break;
            }

            ++block_index;
        }

        if (!block_ptr) {
            // Add new block at end
            block_ptr = &m_blocks.EmplaceBack();
        }
    }

    // @fixme What if another thread grabs the last slot from a block from while we're here?
    const uint32 id = block_ptr->id_generator.NextID();
    AssertThrow(id != 0);

    index = ((id - 1) + (block_index * elements_per_block)) + 1;
    block_ptr->count.Increment(1, MemoryOrder::RELEASE);

    // const uint32 relative_index = (index - 1) % elements_per_block;

// #ifdef HYP_DEBUG_MODE
//     AssertThrow(block_ptr->gpu_buffers[relative_index].Every([](const GPUBufferRef &ref) { return ref == nullptr; }));
// #endif

    // for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
    //     GPUBufferRef &buffer = block_ptr->gpu_buffers[relative_index][frame_index];
    //     AssertThrow(buffer == nullptr);

    //     // @TODO Change to uniform buffer
    //     buffer = MakeRenderObject<renderer::GPUBuffer>(GPUBufferType::STORAGE_BUFFER);

    //     HYPERION_ASSERT_RESULT(buffer->Create(
    //         g_engine->GetGPUDevice(),
    //         sizeof(EntityInstanceBatch)
    //     ));
    // }

    return index;
}

void EntityInstanceBatchList::ReleaseIndex(uint32 &index)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    HYP_DEFER({
        index = 0;
    });

    const uint32 block_index = (index - 1) / elements_per_block;

    // temp
    Mutex::Guard guard(m_mutex);
    HYP_MT_CHECK_READ(m_data_race_detector);

    Block *block_ptr = nullptr;

    if (block_index == 0) {
        block_ptr = &m_blocks.Front();
    } else {
        // Have to lock m_mutex for element

        // Mutex::Guard guard(m_mutex);
        // HYP_MT_CHECK_READ(m_data_race_detector);

        uint32 i = 0;

        for (Block &block : m_blocks) {
            if (i == block_index) {
                block_ptr = &block;

                break;
            }

            ++i;
        }
    }

    AssertThrow(block_ptr != nullptr);

    const uint32 relative_index = (index - 1) % elements_per_block;

    // SafeRelease(std::move(block_ptr->gpu_buffers[relative_index]));
    block_ptr->dirty_states[relative_index] = 0;
    Memory::MemSet(&block_ptr->entity_instance_batches[relative_index], 0, sizeof(EntityInstanceBatch));

    block_ptr->id_generator.FreeID(relative_index + 1);

    block_ptr->count.Decrement(1, MemoryOrder::RELEASE);

    // // See if we can remove this block from the end.
    // // Can't remove if the block is in the middle (IDs will refer to the wrong m_blocks) or if it is the first.
    // if (block_ptr == &m_blocks.Front()) {
    //     return;
    // }

    // Mutex::Guard guard(m_mutex);
    // HYP_MT_CHECK_RW(m_data_race_detector);

    // while (block_ptr == &m_blocks.Back() && block_ptr != &m_blocks.Front() && IsBlockEmpty(*block_ptr)) {
    //     // Remove the empty block from the end.
    //     m_blocks.PopBack();

    //     block_ptr = &m_blocks.Back();
    // }
}

void EntityInstanceBatchList::MarkDirty(uint32 index)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    if (index == 0) {
        return;
    }

    const uint32 block_index = (index - 1) / elements_per_block;

    if (block_index == 0) {
        m_blocks.Front().dirty_states[(index - 1) % elements_per_block] = 0xFF;

        return;
    }

    Mutex::Guard guard(m_mutex);

    HYP_MT_CHECK_RW(m_data_race_detector);

    uint32 i = 0;

    for (Block &block : m_blocks) {
        if (i == block_index) {
            block.dirty_states[(index - 1) % elements_per_block] = 0xFF;

            return;
        }

        ++i;
    }

    HYP_FAIL("Element out of bounds: %u", index);
}

void EntityInstanceBatchList::UpdateBuffers(renderer::Device *device, uint32 frame_index)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    HYP_MT_CHECK_RW(m_data_race_detector);

    uint32 num_empty_blocks_at_end = 0;
    uint32 block_index = 0;

    Array<Pair<uint32, Span<EntityInstanceBatch>>> spans;

    for (Block &block : m_blocks) {
        if (block.count.Get(MemoryOrder::ACQUIRE) == 0) {
            ++num_empty_blocks_at_end;
            ++block_index;

            continue;
        } else {
            num_empty_blocks_at_end = 0;
        }

        Span<EntityInstanceBatch> current_span;

        for (uint32 i = 0; i < elements_per_block; i++) {
            if (!(block.dirty_states[i] & (1u << frame_index))) {
                continue;
            }

            if (!current_span.first) {
                current_span.first = &block.entity_instance_batches[i];
            }

            current_span.last = &block.entity_instance_batches[i] + 1;

            block.dirty_states[i] &= ~(1u << frame_index);
        }

        if (current_span) {
            spans.EmplaceBack(((current_span.first - &block.entity_instance_batches[0]) + block_index) * elements_per_block, current_span);
        }

        ++block_index;
    }

    // remove empty m_blocks found while iterating
    while (num_empty_blocks_at_end != 0) {
        if (m_blocks.Size() == 1) {
            // Don't remove the first block
            break;
        }

        m_blocks.PopBack();

        --num_empty_blocks_at_end;
    }

    const SizeType new_buffer_size = m_blocks.Size() * elements_per_block;

    const GPUBufferRef &buffer = m_gpu_buffers[frame_index];
    AssertThrow(buffer.IsValid());

    bool size_changed = false;

    HYPERION_ASSERT_RESULT(buffer->EnsureCapacity(
        g_engine->GetGPUDevice(),
        new_buffer_size,
        &size_changed
    ));

    if (size_changed) {
        // Copy all EntityInstanceBatch data if size has changed
        uint32 block_index = 0;

        for (Block &block : m_blocks) {
            buffer->Copy(
                device,
                block_index * elements_per_block * sizeof(EntityInstanceBatch),
                elements_per_block * sizeof(EntityInstanceBatch),
                block.entity_instance_batches.Data()
            );

            ++block_index;
        }
    } else if (spans.Any()) {
        AssertThrow(buffer->IsCreated());

        for (const Pair<uint32, Span<EntityInstanceBatch>> &it : spans) {
            const uint32 offset = it.first;
            const Span<EntityInstanceBatch> &span = it.second;

            const SizeType size = span.Size();

            buffer->Copy(
                device,
                offset * sizeof(EntityInstanceBatch),
                size * sizeof(EntityInstanceBatch),
                span.Data()
            );
        }
    }

    // const SizeType allocated_size = AllocatedSize();

    // for (SizeType i = 0; i < allocated_size; i++) {
    //     if (m_dirty_states[i] & (1u << frame_index)) {
    //         const GPUBufferRef &buffer = m_gpu_buffers[i][frame_index];
    //         AssertThrow(buffer.IsValid());

    //         if (!buffer->IsCreated()) {
    //             HYPERION_ASSERT_RESULT(buffer->Create(
    //                 device,
    //                 sizeof(EntityInstanceBatch)
    //             ));
    //         }

    //         buffer->Copy(
    //             device,
    //             0,
    //             sizeof(EntityInstanceBatch),
    //             &m_entity_instance_batches[i]
    //         );

    //         m_dirty_states[i] &= ~(1u << frame_index);
    //     }
    // }
}

#pragma endregion EntityInstanceBatchList

} // namespace hyperion