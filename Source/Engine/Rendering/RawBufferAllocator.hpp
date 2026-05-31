/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

class RawBuffer;
class StructuredBuffer;
class RWStructuredBuffer;
class ByteAddressBuffer;

template <class AllocatorType>
class TCommandRecorder;

class BufferAllocator final
{
public:
    BufferAllocator();
    ~BufferAllocator();

    BufferAllocator(const BufferAllocator&) = delete;
    BufferAllocator& operator=(const BufferAllocator&) = delete;

    void OnFrameStart();
    void OnFrameEnd();

    StructuredBuffer& AcquireStructuredBuffer(size_t numElements, size_t elementSize);
    RWStructuredBuffer& AcquireRWStructuredBuffer(size_t numElements, size_t elementSize);

    ByteAddressBuffer& AcquireByteAddressBuffer(size_t totalSizeBytes);

    void Shutdown();

private:
    Pimpl<struct BufferAllocatorImpl> m_impl;
};

} // namespace Hyperion
