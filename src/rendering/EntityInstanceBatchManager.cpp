/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/EntityInstanceBatchManager.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

EntityInstanceBatchManager::EntityInstanceBatchManager()
    : m_index_counter(0)
{
}

EntityInstanceBatchManager::~EntityInstanceBatchManager()
{
    for (auto &it : m_gpu_buffers) {
        SafeRelease(std::move(it));
    }
}

uint32 EntityInstanceBatchManager::AcquireIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    Mutex::Guard guard(m_mutex);

    if (m_free_indices.Any()) {
        uint32 index = m_free_indices.PopBack();

#ifdef HYP_DEBUG_MODE // sanity check
        AssertThrow(index - 1 < AllocatedSize());
#endif

        m_entity_instance_batches[index - 1] = EntityInstanceBatch { };

        for (GPUBufferRef &gpu_buffer : m_gpu_buffers[index - 1]) {
            gpu_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
        }

        m_dirty_states[index - 1] = uint8(-1);

        return index;
    }

    HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Adding new entity instance batch from thread {}, size changing from {} to {}\tCurrently dynamic? {}",
        Threads::CurrentThreadID().name,
        m_entity_instance_batches.Size(),
        m_entity_instance_batches.Size() + 1,
        m_entity_instance_batches.GetArrayStorage().IsDynamic());

    if (m_entity_instance_batches.GetArrayStorage().IsDynamic()) {
        AssertThrow(m_entity_instance_batches.GetArrayStorage().m_buffer != nullptr);
    }

    m_entity_instance_batches.EmplaceBack();

    if (m_entity_instance_batches.GetArrayStorage().IsDynamic()) {
        AssertThrow(m_entity_instance_batches.GetArrayStorage().m_buffer != nullptr);
    }
    
    for (GPUBufferRef &gpu_buffer : m_gpu_buffers.EmplaceBack()) {
        gpu_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    }

    m_dirty_states.EmplaceBack(0);

    return ++m_index_counter;
}

void EntityInstanceBatchManager::ReleaseIndex(uint32 &index)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    HYP_DEFER({
        index = 0;
    });

    Mutex::Guard guard(m_mutex);

    AssertThrow(index - 1 < AllocatedSize());

    m_dirty_states[index - 1] = 0; // prevent update after release
    SafeRelease(std::move(m_gpu_buffers[index - 1]));

    if (index == m_gpu_buffers.Size()) {
        // number of elements to pop off end
        uint32 resize_count = 0;

        while (m_gpu_buffers[--m_index_counter].Every([](const GPUBufferRef &ref) -> bool { return ref == nullptr; })) {
            m_free_indices.Erase(m_index_counter);
            ++resize_count;
        }

        if (resize_count != 0) {
            HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Resize m_entity_instance_batches from {} to {}\tCurrently dynamic? {}",
                m_entity_instance_batches.Size(),
                m_entity_instance_batches.Size() - resize_count,
                m_entity_instance_batches.GetArrayStorage().IsDynamic());

            m_gpu_buffers.Resize(m_gpu_buffers.Size() - resize_count);
            m_entity_instance_batches.Resize(m_entity_instance_batches.Size() - resize_count);
            m_dirty_states.Resize(m_dirty_states.Size() - resize_count);
        }

        return;
    }

    m_free_indices.PushBack(index);
}

void EntityInstanceBatchManager::MarkDirty(uint32 index)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    if (index == 0) {
        return;
    }

    // TEMP: @TODO: use lock-free linked list with blocks for these.
    // use atomic var for allocated size.
    Mutex::Guard guard(m_mutex);

    AssertThrow(index - 1 < AllocatedSize());

    m_dirty_states[index - 1] = uint8(-1);
}

void EntityInstanceBatchManager::UpdateBuffers(renderer::Device *device, uint32 frame_index)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const SizeType allocated_size = AllocatedSize();

    for (SizeType i = 0; i < allocated_size; i++) {
        if (m_dirty_states[i] & (1u << frame_index)) {
            const GPUBufferRef &buffer = m_gpu_buffers[i][frame_index];
            AssertThrow(buffer.IsValid());

            if (!buffer->IsCreated()) {
                HYPERION_ASSERT_RESULT(buffer->Create(
                    device,
                    sizeof(EntityInstanceBatch)
                ));
            }

            buffer->Copy(
                device,
                0,
                sizeof(EntityInstanceBatch),
                &m_entity_instance_batches[i]
            );

            m_dirty_states[i] &= ~(1u << frame_index);
        }
    }
}

} // namespace hyperion