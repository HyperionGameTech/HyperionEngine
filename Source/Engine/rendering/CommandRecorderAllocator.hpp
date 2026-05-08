/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/containers/LinkedList.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/CommandRecorder.hpp>

namespace Hyperion {

extern uint32 GetFrameCounter();

class HYP_API CommandRecorderAllocator
{
public:
    CommandRecorderAllocator();

    CommandRecorderAllocator(const CommandRecorderAllocator&) = delete;
    CommandRecorderAllocator& operator=(const CommandRecorderAllocator&) = delete;

    CommandRecorderAllocator(CommandRecorderAllocator&&) = delete;
    CommandRecorderAllocator& operator=(CommandRecorderAllocator&&) = delete;

    ~CommandRecorderAllocator();

    void Shutdown();

    void UpdateQueue();

    CommandRecorder& GetCommandRecorder();

private:
    // for calling on another thread than sim thread / render thread.
    Mutex m_mutex;

    LinkedList<CommandRecorder> m_tempCommandRecorders;
    volatile int32 m_tempCommandRecordersCount = 0;

    CommandRecorder m_renderThreadCommandRecorder;
};

} // namespace Hyperion
