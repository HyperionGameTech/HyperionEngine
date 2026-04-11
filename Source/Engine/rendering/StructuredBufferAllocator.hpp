/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/memory/Pimpl.hpp>

#include <rendering/RenderMemory.hpp>

namespace Hyperion {

class StructuredBuffer;

template <class AllocatorType>
class TCommandRecorder;

class StructuredBufferAllocator
{
public:
    StructuredBufferAllocator();
    ~StructuredBufferAllocator();

    StructuredBufferAllocator(const StructuredBufferAllocator&) = delete;
    StructuredBufferAllocator& operator=(const StructuredBufferAllocator&) = delete;

    void OnFrameStart();
    void OnFrameEnd();

    template <class AllocatorType>
    void UpdateAllUsedInFrame(TCommandRecorder<AllocatorType>& cr);

    StructuredBuffer& AcquireBuffer(size_t numElements, size_t elementSize);

    void Shutdown();

private:
    Pimpl<struct StructuredBufferAllocatorImpl> m_impl;
};

template <> void StructuredBufferAllocator::UpdateAllUsedInFrame<RenderAllocator>(TCommandRecorder<RenderAllocator>& cr);

} // namespace Hyperion
