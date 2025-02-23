/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion {
namespace renderer {

StagingBuffer *StagingBufferPool::Context::Acquire(SizeType required_size)
{
    if (required_size == 0) {
        DebugLog(LogType::Warn, "Attempt to acquire staging buffer of 0 size\n");

        return nullptr;
    }
    
    const SizeType new_size = MathUtil::NextPowerOf2(required_size);

    StagingBuffer *staging_buffer = m_pool->FindStagingBuffer(required_size - 1);

    if (staging_buffer != nullptr && !m_used.Contains(staging_buffer)) {
#ifdef HYP_LOG_MEMORY_OPERATIONS
        DebugLog(
            LogType::Debug,
            "Requested staging buffer of size %llu, reusing existing staging buffer of size %llu\n",
            required_size,
            staging_buffer->Size()
        );
#endif
    } else {
#ifdef HYP_LOG_MEMORY_OPERATIONS
        DebugLog(
            LogType::Debug,
            "Staging buffer of size >= %llu not found, creating one of size %llu at time %lld\n",
            required_size,
            new_size,
            std::time(nullptr)
        );
#endif
        staging_buffer = CreateStagingBuffer(new_size);
    }

    m_used.Insert(staging_buffer);

    return staging_buffer;
}

StagingBuffer *StagingBufferPool::Context::CreateStagingBuffer(SizeType size)
{
    const std::time_t current_time = std::time(nullptr);

    DebugLog(
        LogType::Debug,
        "Creating staging buffer of size %llu at time %lld\n",
        size,
        current_time
    );

    auto staging_buffer = std::make_unique<StagingBuffer>();

    if (!staging_buffer->Create(m_device, size)) {
        return nullptr;
    }

    m_staging_buffers.PushBack(StagingBufferRecord {
        .size       = size,
        .buffer     = std::move(staging_buffer),
        .last_used  = current_time
    });

    return m_staging_buffers.Back().buffer.get();
}

StagingBuffer *StagingBufferPool::FindStagingBuffer(SizeType size)
{
    /* do a binary search to find one with the closest size (never less than required) */
    const auto bound = std::upper_bound(
        m_staging_buffers.Begin(),
        m_staging_buffers.End(),
        size,
        [](SizeType sz, const auto &it)
        {
            return sz < it.size;
        }
    );

    if (bound != m_staging_buffers.End()) {
        /* Update the time so that frequently used buffers will stay in the pool longer */
        bound->last_used = std::time(nullptr);

        return bound->buffer.get();
    }

    return nullptr;
}

RendererResult StagingBufferPool::Use(Device *device, UseFunction &&fn)
{
    RendererResult result;

    Context context(this, device);

    HYPERION_PASS_ERRORS(fn(context), result);

    for (auto &record : context.m_staging_buffers) {
        const auto bound = std::upper_bound(
            m_staging_buffers.Begin(),
            m_staging_buffers.End(),
            record,
            [](const auto &record, const auto &it) {
                return record.size < it.size;
            }
        );

        m_staging_buffers.Insert(bound, std::move(record));
    }
    
    if (++use_calls % gc_threshold == 0) {
        HYPERION_PASS_ERRORS(GC(device), result);
    }

    return result;
}

RendererResult StagingBufferPool::GC(Device *device)
{
    const auto current_time = std::time(nullptr);

    DebugLog(LogType::Debug, "Clean up staging buffers from pool\n");
    
    RendererResult result;
    SizeType num_destroyed = 0;

    for (auto it = m_staging_buffers.Begin(); it != m_staging_buffers.End();) {
        if (current_time - it->last_used > hold_time) {
            ++num_destroyed;
            
            HYPERION_PASS_ERRORS(it->buffer->Destroy(device), result);

            it = m_staging_buffers.Erase(it);
        } else {
            ++it;
        }
    }

    if (num_destroyed != 0) {
        DebugLog(LogType::Debug, "Removed %llu staging buffers from pool\n", num_destroyed);
    }

    return result;
}

RendererResult StagingBufferPool::Destroy(Device *device)
{
    RendererResult result;

    for (auto &record : m_staging_buffers) {
        HYPERION_PASS_ERRORS(record.buffer->Destroy(device), result);
    }

    m_staging_buffers.Clear();

    use_calls = 0;

    return result;
}

} // namespace renderer
} // namespace hyperion