/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/List.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Rendering/RenderResult.hpp>
#include <Rendering/CommandRecorder.hpp>

namespace Hyperion {

extern uint32 GetFrameCounter();

enum class CommandRecorderQueue : uint8
{
    /*! \brief Executed after the frame's render commands. (Default) */
    PostRender = 0,

    /*! \brief Executed before the frame's render commands. Intended for
     *  resource uploads and initialization that needs to happen before
     *  they are needed for drawing/compute. */
    PreRender
};

class CommandRecorderAllocator
{
public:
    CommandRecorderAllocator();

    CommandRecorderAllocator(const CommandRecorderAllocator&) = delete;
    CommandRecorderAllocator& operator=(const CommandRecorderAllocator&) = delete;

    CommandRecorderAllocator(CommandRecorderAllocator&&) = delete;
    CommandRecorderAllocator& operator=(CommandRecorderAllocator&&) = delete;

    ~CommandRecorderAllocator();

    /*! \brief Flushes the queue and submits all via transient command buffers.
     *  if \p isShuttingDown is true, will prevent new entries from being added until after Shutdown() */
    void Flush(bool isShuttingDown);

    void Shutdown();

    void UpdateQueue();

    CommandRecorder& GetCommandRecorder(CommandRecorderQueue queue = CommandRecorderQueue::PostRender);

    CommandRecorder root;
    CommandRecorder rootPreRender;

private:
    void UpdateQueue_Internal();
    void DrainTempCommandRecorders(List<CommandRecorder>& tempCommandRecorders, CommandRecorder& dst);

    // for calling on another thread than sim thread / render thread.
    Mutex m_mutex;

    List<CommandRecorder> m_tempCommandRecorders;
    List<CommandRecorder> m_tempPreRenderCommandRecorders;
    volatile int32 m_tempCommandRecordersCount = 0;

    bool m_isShuttingDown : 1;
};

} // namespace Hyperion
