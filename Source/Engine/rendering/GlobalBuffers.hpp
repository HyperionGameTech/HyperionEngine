/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/TypeMap.hpp>

#include <rendering/GpuBuffer.hpp>

namespace Hyperion {

class GpuBufferHolderBase;

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder;

class GpuBufferHolderMap
{
public:
    GpuBufferHolderMap() = default;

    GpuBufferHolderMap(const GpuBufferHolderMap& other) = delete;
    GpuBufferHolderMap& operator=(const GpuBufferHolderMap& other) = delete;

    ~GpuBufferHolderMap();

    HYP_FORCE_INLINE const TypeMap<GpuBufferHolderBase*>& GetItems() const
    {
        return m_holders;
    }

    template <class T, GpuBufferType BufferType = GpuBufferType::STORAGE_BUFFER>
    GpuBufferHolder<T, BufferType>* GetOrCreate(uint32 initialCount, bool cpuAccessible)
    {
        auto it = m_holders.Find<T>();

        if (it == m_holders.End())
        {
            HYP_MT_CHECK_WRITE(m_dataRaceDetector);

            it = m_holders.Set<T>(PoolNew<GpuBufferHolder<T, BufferType>>(*g_renderPool, initialCount, cpuAccessible)).first;
        }

        return static_cast<GpuBufferHolder<T, BufferType>*>(it->second);
    }

    void DeleteAll();

private:
    TypeMap<GpuBufferHolderBase*> m_holders;
};

} // namespace Hyperion
