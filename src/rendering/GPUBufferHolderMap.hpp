/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GPU_BUFFER_HOLDER_MAP_HPP
#define HYPERION_GPU_BUFFER_HOLDER_MAP_HPP

#include <core/Defines.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/TypeMap.hpp>

#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion {

using renderer::GPUBufferType;

class GPUBufferHolderBase;

template <class StructType, GPUBufferType BufferType>
class GPUBufferHolder;

class GPUBufferHolderMap
{
public:
    HYP_FORCE_INLINE const TypeMap<UniquePtr<GPUBufferHolderBase>> &GetItems() const
        { return m_holders; }

    template <class T, GPUBufferType BufferType = GPUBufferType::STORAGE_BUFFER>
    GPUBufferHolder<T, BufferType> *GetOrCreate(uint32 count)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_holders.Find<T>();

        if (it == m_holders.End()) {
            HYP_MT_CHECK_WRITE(m_data_race_detector);

            it = m_holders.Set<T>(MakeUnique<GPUBufferHolder<T, BufferType>>(count)).first;
        }

        return static_cast<GPUBufferHolder<T, BufferType> *>(it->second.Get());
    }

private:
    TypeMap<UniquePtr<GPUBufferHolderBase>> m_holders;
    
    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif