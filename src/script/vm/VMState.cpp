#include <script/vm/VMState.hpp>
#include <script/vm/GC.hpp>

#include <util/UTF8.hpp>

#include <algorithm>
#include <thread>
#include <iostream>
#include <cmath>
#include <mutex>

namespace hyperion {
namespace vm {

VMState::VMState()
    : m_gc(new GC())
{
    for (int i = 0; i < VM_MAX_THREADS; i++)
    {
        m_threads[i] = nullptr;
    }
}

VMState::~VMState()
{
    Reset();

    delete m_gc;
    m_gc = nullptr;
}

void VMState::Reset()
{
    // mark all static memory for deallocation,
    // objects will be deallocated when the heap is purged
    m_staticMemory.MarkAllForDeallocation();
    // purge the heap
    m_heap.Purge();
    // reset heap threshold
    m_maxHeapObjects = GC_THRESHOLD_MIN;

    for (uint32 i = 0; i < VM_MAX_THREADS; i++)
    {
        DestroyThread(i);
    }

    // we're good to go
    good = true;
}

void VMState::ThrowException(Script_ExecutionThread* thread, const Exception& exception)
{
    ++thread->m_exceptionState.m_exceptionDepth;

    if (thread->m_exceptionState.m_tryCounter == 0)
    {
        // exception cannot be handled, no try block found
        if (thread->m_id == 0)
        {
            DebugLog(LogType::Error, "unhandled exception in main thread: %s", exception.ToString());
        }
        else
        {
            DebugLog(LogType::Error, "unhandled exception in thread %d: %s", thread->m_id, exception.ToString());
        }

        good = false;
    }
}

HeapValue* VMState::HeapAlloc(Script_ExecutionThread* thread)
{
    Assert(thread != nullptr);

    const SizeType heapSize = m_heap.Size();

    if (heapSize >= m_maxHeapObjects)
    {
        if (heapSize >= GC_THRESHOLD_MAX)
        {
            // heap overflow.
            char buffer[256];
            std::sprintf(
                buffer,
                "out of budgeted heap memory : size is %zu, max is %d",
                heapSize,
                GC_THRESHOLD_MAX);

            ThrowException(thread, Exception(buffer));

            return nullptr;
        }

#if ENABLE_GC
        if (enableAutoGc)
        {
            // run the gc
            GC();

            // check if size is still over the maximum,
            // and resize the maximum if necessary.
            if (m_heap.Size() >= m_maxHeapObjects)
            {
                // resize max number of objects
                m_maxHeapObjects = std::min(
                    1 << (unsigned)std::ceil(std::log(m_maxHeapObjects) / std::log(2.0)),
                    GC_THRESHOLD_MAX);
            }
        }
#endif
    }

    return m_heap.Alloc();
}

// void VMState::GC()
// {
//     DebugLog(
//         LogType::Debug,
//         "Begin gc\n");

//     m_exportedSymbols.MarkAll();

//     // mark stack objects on each thread
//     for (uint32 i = 0; i < VM_MAX_THREADS; i++)
//     {
//         if (m_threads[i] != nullptr)
//         {
//             m_threads[i]->m_stack.MarkAll();

//             for (uint32 j = 0; j < VM_NUM_REGISTERS; j++)
//             {
//                 m_threads[i]->GetRegisters()[j].Mark();
//             }
//         }
//     }

//     uint32 numCollected;
//     m_heap.Sweep(&numCollected);

//     DebugLog(
//         LogType::Debug,
//         "%u objects garbage collected\n",
//         numCollected);
// }

Script_ExecutionThread* VMState::CreateThread()
{
    Assert(m_numThreads < VM_MAX_THREADS);

    // find a free slot
    for (uint32 i = 0; i < VM_MAX_THREADS; i++)
    {
        if (m_threads[i] == nullptr)
        {
            Script_ExecutionThread* thread = new Script_ExecutionThread();
            thread->m_id = i;

            m_threads[i] = thread;
            m_numThreads++;

            return thread;
        }
    }

    return nullptr;
}

void VMState::DestroyThread(int id)
{
    Assert(id < VM_MAX_THREADS);

    Script_ExecutionThread* thread = m_threads[id];

    if (thread != nullptr)
    {
        // purge the stack
        thread->m_stack.Purge();
        // reset exception state
        thread->m_exceptionState = {};
        // reset register flags
        thread->m_regs.ResetFlags();

        // delete it
        delete m_threads[id];
        m_threads[id] = nullptr;

        m_numThreads--;
    }
}

} // namespace vm
} // namespace hyperion
