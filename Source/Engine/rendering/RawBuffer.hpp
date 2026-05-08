/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <rendering/GpuBuffer.hpp>
#include <rendering/RenderMemory.hpp>

namespace Hyperion {

template <class AllocatorType>
class TCommandRecorder;

class RawBuffer
{
protected:
    RawBuffer(GpuBufferType bufferType, size_t numElements, size_t elementSize, size_t alignment = 16)
        : gpuBuffer(new GpuBuffer(bufferType, GetAlignedBufferSize(numElements, elementSize, alignment), alignment)),
          elementSize(elementSize),
          dirtyRangeStart(SIZE_MAX),
          dirtyRangeEnd(0)
    {
        cpuBuffer.SetSize(gpuBuffer->Size());
    }

public:
    RawBuffer()
        : gpuBuffer(nullptr),
          elementSize(0),
          dirtyRangeStart(SIZE_MAX),
          dirtyRangeEnd(0)
    {
    }

    RawBuffer(const RawBuffer& other) = delete;
    RawBuffer& operator=(const RawBuffer& other) = delete;

    RawBuffer(RawBuffer&& other) noexcept
        : gpuBuffer(other.gpuBuffer),
          cpuBuffer(std::move(other.cpuBuffer)),
          elementSize(other.elementSize),
          dirtyRangeStart(other.dirtyRangeStart),
          dirtyRangeEnd(other.dirtyRangeEnd)
    {
        other.gpuBuffer = nullptr;
    }

    RawBuffer& operator=(RawBuffer&& other) noexcept
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

    ~RawBuffer()
    {
        Shutdown();
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

    void FlushInto(CommandBuffer& cmdBuffer);
    void Flush();

    GpuBuffer* gpuBuffer;
    TByteBuffer<RenderAllocator> cpuBuffer;

    size_t elementSize;

    size_t dirtyRangeStart;
    size_t dirtyRangeEnd;

protected:
    static constexpr inline size_t GetAlignedBufferSize(size_t numElements, size_t elementSize, size_t alignment)
    {
        size_t totalSize = elementSize != 0 ? numElements * elementSize : numElements;
        return alignment != 0 ? ByteUtil::AlignAs(totalSize, alignment) : totalSize;
    }
};

class StructuredBuffer : public RawBuffer
{
protected:
    StructuredBuffer(GpuBufferType bufferType, size_t numElements, size_t elementSize)
        : RawBuffer(bufferType, numElements, elementSize)
    {
    }

public:
    static constexpr GpuBufferType BufferType = GpuBufferType::StructuredBuffer;

    StructuredBuffer() = default;

    explicit StructuredBuffer(size_t numElements, size_t elementSize)
        : RawBuffer(GpuBufferType::StructuredBuffer, numElements, elementSize)
    {
    }

    StructuredBuffer(const StructuredBuffer& other) = delete;
    StructuredBuffer& operator=(const StructuredBuffer& other) = delete;

    StructuredBuffer(StructuredBuffer&& other) noexcept
        : RawBuffer(static_cast<RawBuffer&&>(other))
    {
    }

    StructuredBuffer& operator=(StructuredBuffer&& other) noexcept
    {
        return static_cast<StructuredBuffer&>(static_cast<RawBuffer&>(*this) = static_cast<RawBuffer&&>(other));
    }
};

class RWStructuredBuffer : public StructuredBuffer
{
public:
    static constexpr GpuBufferType BufferType = GpuBufferType::RWStructuredBuffer;

    RWStructuredBuffer() = default;

    RWStructuredBuffer(size_t numElements, size_t elementSize)
        : StructuredBuffer(GpuBufferType::RWStructuredBuffer, numElements, elementSize)
    {
    }

    RWStructuredBuffer(const RWStructuredBuffer& other) = delete;
    RWStructuredBuffer& operator=(const RWStructuredBuffer& other) = delete;

    RWStructuredBuffer(RWStructuredBuffer&& other) noexcept
        : StructuredBuffer(static_cast<StructuredBuffer&&>(other))
    {
    }

    RWStructuredBuffer& operator=(RWStructuredBuffer&& other) noexcept
    {
        return static_cast<RWStructuredBuffer&>(static_cast<StructuredBuffer&>(*this) = static_cast<StructuredBuffer&&>(other));
    }
};

class ByteAddressBuffer : public RawBuffer
{
public:
    static constexpr GpuBufferType BufferType = GpuBufferType::ByteAddressBuffer;

    ByteAddressBuffer() = default;

    ByteAddressBuffer(size_t totalSizeBytes)
        : RawBuffer(GpuBufferType::ByteAddressBuffer, totalSizeBytes, 0, 4)
    {
    }

    ByteAddressBuffer(const ByteAddressBuffer& other) = delete;
    ByteAddressBuffer& operator=(const ByteAddressBuffer& other) = delete;

    ByteAddressBuffer(ByteAddressBuffer&& other) noexcept
        : RawBuffer(static_cast<RawBuffer&&>(other))
    {
    }

    ByteAddressBuffer& operator=(ByteAddressBuffer&& other) noexcept
    {
        return static_cast<ByteAddressBuffer&>(static_cast<RawBuffer&>(*this) = static_cast<RawBuffer&&>(other));
    }
};

} // namespace Hyperion
