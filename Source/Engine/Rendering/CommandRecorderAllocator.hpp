/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/List.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Rendering/RenderResult.hpp>
#include <Rendering/CommandRecorder.hpp>

namespace Hyperion {

extern uint32 GetFrameCounter();

class CommandRecorderAllocator
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

    CommandRecorder root;

private:
    // for calling on another thread than sim thread / render thread.
    Mutex m_mutex;

    TList<CommandRecorder> m_tempCommandRecorders;
    volatile int32 m_tempCommandRecordersCount = 0;
};

} // namespace Hyperion
