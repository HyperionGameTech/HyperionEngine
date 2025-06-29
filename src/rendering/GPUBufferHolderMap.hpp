/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GPU_BUFFER_HOLDER_MAP_HPP
#define HYPERION_GPU_BUFFER_HOLDER_MAP_HPP

#include <core/Defines.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/TypeMap.hpp>

#include <rendering/backend/RendererGpuBuffer.hpp>

namespace hyperion {

class GpuBufferHolderBase;

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder;

class GpuBufferHolderMap
{
public:
    HYP_FORCE_INLINE const TypeMap<UniquePtr<GpuBufferHolderBase>>& GetItems() const
    {
        return m_holders;
    }

    template <class T, GpuBufferType BufferType = GpuBufferType::SSBO>
    GpuBufferHolder<T, BufferType>* GetOrCreate(uint32 initial_count = 0)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_holders.Find<T>();

        if (it == m_holders.End())
        {
            HYP_MT_CHECK_WRITE(m_data_race_detector);

            it = m_holders.Set<T>(MakeUnique<GpuBufferHolder<T, BufferType>>(initial_count)).first;
        }

        return static_cast<GpuBufferHolder<T, BufferType>*>(it->second.Get());
    }

private:
    TypeMap<UniquePtr<GpuBufferHolderBase>> m_holders;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif