/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_INSTANCE_BATCH_HOLDER_MAP_HPP
#define HYPERION_ENTITY_INSTANCE_BATCH_HOLDER_MAP_HPP

#include <core/Defines.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/TypeMap.hpp>

#include <rendering/Shader.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion {

using renderer::GPUBufferType;

class EntityInstanceBatchHolderMap
{
public:
    HYP_FORCE_INLINE const TypeMap<UniquePtr<GPUBufferHolderBase>> &GetItems() const
        { return m_entity_instance_batch_holders; }

    template <class EntityInstanceBatchType>
    GPUBufferHolder<EntityInstanceBatchType, GPUBufferType::STORAGE_BUFFER> *GetOrCreate(uint32 count)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_entity_instance_batch_holders.Find<EntityInstanceBatchType>();

        if (it == m_entity_instance_batch_holders.End()) {
            HYP_MT_CHECK_WRITE(m_data_race_detector);

            it = m_entity_instance_batch_holders.Set<EntityInstanceBatchType>(MakeUnique<GPUBufferHolder<EntityInstanceBatchType, GPUBufferType::STORAGE_BUFFER>>(count)).first;
        }

        return static_cast<GPUBufferHolder<EntityInstanceBatchType, GPUBufferType::STORAGE_BUFFER> *>(it->second.Get());
    }

private:
    TypeMap<UniquePtr<GPUBufferHolderBase>> m_entity_instance_batch_holders;

#ifdef HYP_ENABLE_MT_CHECK
    DataRaceDetector                        m_data_race_detector;
#endif
};

} // namespace hyperion

#endif