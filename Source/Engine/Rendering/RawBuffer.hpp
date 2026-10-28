/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

template <class AllocatorType>
class TCommandRecorder;

enum class RawBufferFlags : uint8
{
    None = 0x0,             //!< Writes go to a shadow buffer, copied to the GPU via a staging buffer, not coherent.

    CpuWriteCoherent = 0x1, //!< Buffer remains mapped for writing directly on the cpu. Cannot be dynamically resized.
    CpuReadCoherent = 0x2,  //!< Buffer is mapped for reading on the cpu.
    GpuOnly = 0x4,

    Default = None
};

HYP_MAKE_ENUM_FLAGS(RawBufferFlags);

struct RawBufferDirtyRange
{
    size_t start;
    size_t end;
};

/// A wrapper interface for a GPU-accessible buffer.
class RawBuffer
{
public:
    static constexpr uint32 MaxDirtyRanges = 8;
    static constexpr size_t DirtyRangeMergeGap = 1024;

protected:
protected:
    RawBuffer(
        GpuBufferType bufferType,
        size_t numElements,
        size_t elementSize,
        size_t alignment = 16,
        EnumFlags<RawBufferFlags> flags = RawBufferFlags::Default)
        : gpuBuffer(new GpuBuffer(bufferType, GetAlignedBufferSize(numElements, elementSize, alignment), alignment)),
          elementSize(elementSize),
          dirtyRangeStart(SIZE_MAX),
          dirtyRangeEnd(0),
          numDirtyRanges(0),
          flags(flags)
    {
        if (!(flags & (RawBufferFlags::GpuOnly | RawBufferFlags::CpuWriteCoherent)))
        {
            // CPU shadow buffer
            cpuBuffer.SetSize(gpuBuffer->Size());
        }

        if (flags & (RawBufferFlags::CpuReadCoherent | RawBufferFlags::CpuWriteCoherent))
        {
            gpuBuffer->SetIsCpuAccessible(true);
        }
    }

public:
    using CpuBuffer = memory::ByteBuffer<RenderAllocator>;

    RawBuffer()
        : gpuBuffer(nullptr),
          elementSize(0),
          dirtyRangeStart(SIZE_MAX),
          dirtyRangeEnd(0),
          numDirtyRanges(0),
          flags(RawBufferFlags::Default)
    {
    }

    RawBuffer(const RawBuffer& other) = delete;
    RawBuffer& operator=(const RawBuffer& other) = delete;

    RawBuffer(RawBuffer&& other) noexcept
        : gpuBuffer(other.gpuBuffer),
          cpuBuffer(std::move(other.cpuBuffer)),
          elementSize(other.elementSize),
          dirtyRangeStart(other.dirtyRangeStart),
          dirtyRangeEnd(other.dirtyRangeEnd),
          numDirtyRanges(other.numDirtyRanges),
          flags(other.flags)
    {
        Memory::Copy(dirtyRanges, other.dirtyRanges, sizeof(dirtyRanges));

        other.gpuBuffer = nullptr;
        other.ClearDirty();
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
            numDirtyRanges = other.numDirtyRanges;
            Memory::Copy(dirtyRanges, other.dirtyRanges, sizeof(dirtyRanges));
            flags = other.flags;

            other.ClearDirty();
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
        if (count == 0)
        {
            return;
        }

        const size_t end = offset + count;

        dirtyRangeStart = offset < dirtyRangeStart ? offset : dirtyRangeStart;
        dirtyRangeEnd = end > dirtyRangeEnd ? end : dirtyRangeEnd;

        AddDirtyRange(offset, end);
    }

    HYP_FORCE_INLINE void ClearDirty()
    {
        dirtyRangeStart = SIZE_MAX;
        dirtyRangeEnd = 0;
        numDirtyRanges = 0;
    }

    HYP_FORCE_INLINE size_t TotalSizeBytes() const
    {
        return gpuBuffer->Size();
    }

    template <typename T>
    HYP_FORCE_INLINE const T* GetReadPtr(size_t elementIndex) const
    {
        const size_t offset = elementIndex * elementSize;

        Assert(offset + sizeof(T) <= TotalSizeBytes());

        return reinterpret_cast<const T*>(GetReadDataPtr() + offset);
    }

    template <typename T>
    HYP_FORCE_INLINE T* GetWritePtr(size_t elementIndex)
    {
        const size_t offset = elementIndex * elementSize;

        Assert(offset + sizeof(T) <= TotalSizeBytes());

        MarkDirty(offset, sizeof(T));

        return reinterpret_cast<T*>(GetWriteDataPtr() + offset);
    }

    template <typename T>
    HYP_FORCE_INLINE T* GetWritePtr(size_t elementIndex, size_t count)
    {
        const size_t offset = elementIndex * elementSize;
        const size_t size = count * elementSize;

        Assert(offset + size <= TotalSizeBytes());

        MarkDirty(offset, size);

        return reinterpret_cast<T*>(GetWriteDataPtr() + offset);
    }

    void Initialize();
    void Shutdown();

    void Write(size_t offset, size_t count, const void* data);

    /// Write \p value at \p offset, \p count times (buffer not flushed immediately)
    void Fill(size_t offset, size_t count, ubyte value);

    void FlushInto(CommandBuffer& cmdBuffer);
    void Flush();

    static void FlushAllInto(CommandBuffer& cmdBuffer, Span<RawBuffer* const> buffers);

    /// Sets the buffer to be flushed at the top of the frame, preceding other recorded rendering commands.
    void FlushBatched();

    /// \p newSize Raw buffer size in bytes.
    /// The existing buffer will be enqueued for deletion if size differs, and a new buffer will be created.
    /// \return true if resize happened.  Otherwise, no changes were applied.
    bool Resize(size_t newSize);

    GpuBuffer* gpuBuffer;

    size_t elementSize;

    size_t dirtyRangeStart;
    size_t dirtyRangeEnd;

    RawBufferDirtyRange dirtyRanges[MaxDirtyRanges];
    uint32 numDirtyRanges;

    EnumFlags<RawBufferFlags> flags;

protected:
    HYP_FORCE_INLINE static size_t GetDirtyRangeGap(const RawBufferDirtyRange& range, size_t start, size_t end)
    {
        if (start > range.end)
        {
            return start - range.end;
        }

        if (range.start > end)
        {
            return range.start - end;
        }

        return 0;
    }

    HYP_FORCE_INLINE void AddDirtyRange(size_t start, size_t end)
    {
        uint32 closestIndex = 0;
        size_t closestGap = SIZE_MAX;

        for (uint32 i = 0; i < numDirtyRanges; i++)
        {
            const size_t gap = GetDirtyRangeGap(dirtyRanges[i], start, end);

            if (gap < closestGap)
            {
                closestGap = gap;
                closestIndex = i;
            }
        }

        if (closestGap > DirtyRangeMergeGap && numDirtyRanges < MaxDirtyRanges)
        {
            dirtyRanges[numDirtyRanges++] = RawBufferDirtyRange { start, end };

            return;
        }

        RawBufferDirtyRange& range = dirtyRanges[closestIndex];
        range.start = start < range.start ? start : range.start;
        range.end = end > range.end ? end : range.end;
    }

    uint32 CollectDirtyRanges(RawBufferDirtyRange (&outRanges)[MaxDirtyRanges]) const;

    static inline constexpr size_t GetAlignedBufferSize(size_t numElements, size_t elementSize, size_t alignment)
    {
        size_t totalSize = elementSize != 0 ? numElements * elementSize : numElements;
        return alignment != 0 ? ByteUtil::AlignAs(totalSize, alignment) : totalSize;
    }

    HYP_FORCE_INLINE const ubyte* GetReadDataPtr() const
    {
        AssertDebug(!(flags & RawBufferFlags::GpuOnly));

        if (flags & (RawBufferFlags::CpuWriteCoherent | RawBufferFlags::CpuReadCoherent))
        {
            return static_cast<const ubyte*>(gpuBuffer->Map());
        }

        return cpuBuffer.Data();
    }

    HYP_FORCE_INLINE ubyte* GetWriteDataPtr()
    {
        AssertDebug(!(flags & RawBufferFlags::GpuOnly));

        if (flags & RawBufferFlags::CpuWriteCoherent)
        {
            return static_cast<ubyte*>(gpuBuffer->Map());
        }

        return cpuBuffer.Data();
    }
    
    CpuBuffer cpuBuffer;
};

/// A wrapper for a HLSL StructuredBuffer. Shaders can read, CPU writes.
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

/// A wrapper for a HLSL RWStructuredBuffer. Shaders can write to it.
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

/// A wrapper for a HLSL ByteAddressBuffer. Shaders can read, CPU writes.
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
