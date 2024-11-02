/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_entity_instance_batches_HPP
#define HYPERION_entity_instance_batches_HPP

#include <core/Defines.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/Queue.hpp>
#include <core/containers/LinkedList.hpp>

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

class EntityInstanceBatchList
{
public:
    static constexpr uint32 elements_per_block = 2048;

    struct Block
    {
        FixedArray<EntityInstanceBatch, elements_per_block>                             entity_instance_batches;
        FixedArray<uint8, elements_per_block>                                           dirty_states;
        AtomicVar<uint32>                                                               count { 0 };
        IDGenerator                                                                     id_generator;
    };

    HYP_API EntityInstanceBatchList();
    HYP_API ~EntityInstanceBatchList();

    HYP_API EntityInstanceBatch &GetEntityInstanceBatch(uint32 index);

    HYP_FORCE_INLINE const EntityInstanceBatch &GetEntityInstanceBatch(uint32 index) const
        { return const_cast<EntityInstanceBatchList *>(this)->GetEntityInstanceBatch(index); }

    HYP_FORCE_INLINE const GPUBufferRef &GetGPUBuffer(uint32 frame_index) const
        { return m_gpu_buffers[frame_index]; }

    // HYP_API const GPUBufferRef &GetGPUBuffer(uint32 index, uint32 frame_index) const;
    HYP_API uint32 AcquireIndex();
    HYP_API void ReleaseIndex(uint32 &index);

    HYP_API void MarkDirty(uint32 index);
    void UpdateBuffers(renderer::Device *device, uint32 frame_index);

private:
    mutable Mutex                                   m_mutex;
    LinkedList<Block>                               m_blocks;
    DataRaceDetector                                m_data_race_detector;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_gpu_buffers;
};

} // namespace hyperion

#endif