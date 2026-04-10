#pragma once

#include "Backend/RxGpuBuffer.hpp"

#include <Core/FxTypes.hpp>
#include <cstdlib>


class RxUniforms
{
public:
public:
    RxUniforms() = default;

    void Create(uint32 size);

    /**
     * @brief Retrieve the pointer that the buffer starts at relative to the current frame.
     */
    uint8* GetCurrentBuffer();

    RxRawGpuBuffer& GetGpuBuffer() { return mGpuBuffer; }

    void FlushToGpu() { mGpuBuffer.FlushToGpu(GetBaseOffset(), mUniformIndex + 1); }

    template <typename TValueType>
    void Write(const TValueType& value)
    {
        constexpr uint32 type_size = sizeof(TValueType);

        WritePtr(&value, type_size);
    }

    template <typename TPtrType>
    void WritePtr(const TPtrType* value, uint32 size)
    {
        FxAssert(mGpuBuffer.IsMapped());

        if (mUniformIndex + size >= Size) {
            FxLogError("Could not submit uniform as uniform buffer is full!");
            return;
        }

        uint8* dest_ptr = GetCurrentBuffer() + mUniformIndex;
        std::memcpy(dest_ptr, value, size);

        // Offset for the next value
        mUniformIndex += size;
    }

    void AssertSize(uint32 expected_size) { FxAssert(mUniformIndex == expected_size); }

    /**
     * @brief Get the index for start of the buffer at the current frame.
     */
    uint32 GetBaseOffset() const;

    /**
     * @brief Resets the uniform buffer back to the start.
     */
    void Rewind();

    void Destroy() { mGpuBuffer.Destroy(); }

public:
    uint32 Size = 0;

private:
    /// Gpu buffer that stores current uniform buffer data. Note that this is a continguous buffer that stores
    /// `RxFramesInFlight` number of uniform structures that are of size `scUniformBufferSize`.
    RxRawGpuBuffer mGpuBuffer {};


    uint32 mUniformIndex = 0;
};
