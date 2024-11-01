/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_INSTANCE_BATCH_MANAGER_HPP
#define HYPERION_ENTITY_INSTANCE_BATCH_MANAGER_HPP

#include <core/Defines.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/Queue.hpp>

#include <core/utilities/Range.hpp>
#include <core/utilities/Pair.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::CURRENT>;

} // namespace hyperion::renderer

namespace hyperion {

class HYP_API EntityInstanceBatchManager
{
public:
    EntityInstanceBatchManager();
    ~EntityInstanceBatchManager();

    HYP_FORCE_INLINE SizeType AllocatedSize() const
        { return m_entity_instance_batches.Size(); }

    HYP_FORCE_INLINE EntityInstanceBatch &GetEntityInstanceBatch(uint32 index)
        { return m_entity_instance_batches[index]; }

    HYP_FORCE_INLINE const EntityInstanceBatch &GetEntityInstanceBatch(uint32 index) const
        { return m_entity_instance_batches[index]; }

    HYP_FORCE_INLINE const GPUBufferRef &GetGPUBuffer(uint32 index, uint32 frame_index) const
        { return m_gpu_buffers[index][frame_index]; }

    uint32 AcquireIndex();
    void ReleaseIndex(uint32 &index);

    void MarkDirty(uint32 index);
    void UpdateBuffers(renderer::Device *device, uint32 frame_index);

private:
    uint32                                                  m_index_counter;
    Array<uint32>                                           m_free_indices;
    Array<EntityInstanceBatch>                              m_entity_instance_batches;
    Array<FixedArray<GPUBufferRef, max_frames_in_flight>>   m_gpu_buffers;
    Array<uint8>                                            m_dirty_states;
    mutable Mutex                                           m_mutex;
};

} // namespace hyperion

#endif