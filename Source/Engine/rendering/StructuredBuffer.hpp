/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <rendering/GpuBuffer.hpp>
#include <rendering/RenderMemory.hpp>

namespace Hyperion {

template <class AllocatorType>
class TCommandRecorder;

class StructuredBuffer final
{
public:
    StructuredBuffer()
        : gpuBuffer(nullptr),
          elementSize(0),
          dirtyRangeStart(SIZE_MAX),
          dirtyRangeEnd(0)
    {
    }

    explicit StructuredBuffer(size_t numElements, size_t elementSize)
        : gpuBuffer(new GpuBuffer(GpuBufferType::STORAGE_BUFFER, numElements * elementSize, 16)),
          elementSize(elementSize),
          dirtyRangeStart(SIZE_MAX),
          dirtyRangeEnd(0)
    {
        cpuBuffer.SetSize(numElements * elementSize);
    }

    StructuredBuffer(const StructuredBuffer& other) = delete;
    StructuredBuffer& operator=(const StructuredBuffer& other) = delete;

    StructuredBuffer(StructuredBuffer&& other) noexcept
        : gpuBuffer(other.gpuBuffer),
          cpuBuffer(std::move(other.cpuBuffer)),
          elementSize(other.elementSize),
          dirtyRangeStart(other.dirtyRangeStart),
          dirtyRangeEnd(other.dirtyRangeEnd)
    {
        other.gpuBuffer = nullptr;
    }

    StructuredBuffer& operator=(StructuredBuffer&& other) noexcept
    {
        if (this != &other)
        {
            if (gpuBuffer)
            {
                gpuBuffer->Release();
                gpuBuffer = nullptr;
            }

            gpuBuffer = other.gpuBuffer;
            other.gpuBuffer = nullptr;

            cpuBuffer = std::move(other.cpuBuffer);
            elementSize = other.elementSize;
            dirtyRangeStart = other.dirtyRangeStart;
            dirtyRangeEnd = other.dirtyRangeEnd;
        }
        
        return *this;
    }

    ~StructuredBuffer()
    {
        if (gpuBuffer)
        {
            gpuBuffer->Release();
            gpuBuffer = nullptr;
        }
    }

    HYP_FORCE_INLINE bool IsDirty() const
    {
        return dirtyRangeStart < dirtyRangeEnd
            && (dirtyRangeEnd - dirtyRangeStart) > 0;
    }

    HYP_FORCE_INLINE void MarkDirty(size_t offset, size_t count)
    {
        dirtyRangeStart = offset < dirtyRangeStart ? offset : dirtyRangeStart;
        dirtyRangeEnd = (offset + count) > dirtyRangeEnd ? (offset + count) : dirtyRangeEnd;
    }

    void Initialize();
    void Shutdown();

    void Write(size_t offset, size_t count, const void* data);

    void Flush();

    GpuBuffer* gpuBuffer;
    TByteBuffer<RenderAllocator> cpuBuffer;

    size_t elementSize;

    size_t dirtyRangeStart;
    size_t dirtyRangeEnd;
};

} // namespace Hyperion
