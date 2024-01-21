#include <script/vm/VMState.hpp>

#include <util/UTF8.hpp>

#include <algorithm>
#include <thread>
#include <iostream>
#include <cmath>
#include <mutex>

namespace hyperion {
namespace vm {

VMState::VMState()
{
    for (int i = 0; i < VM_MAX_THREADS; i++) {
        m_threads[i] = nullptr;
    }
}

VMState::~VMState()
{
    Reset();
}

void VMState::Reset()
{
    // mark all static memory for deallocation,
    // objects will be deallocated when the heap is purged
    m_static_memory.MarkAllForDeallocation();
    // purge the heap
    m_heap.Purge();
    // reset heap threshold
    m_max_heap_objects = GC_THRESHOLD_MIN;

    for (UInt i = 0; i < VM_MAX_THREADS; i++) {
        DestroyThread(i);
    }

    // we're good to go
    good = true;
}

void VMState::ThrowException(ExecutionThread *thread, const Exception &exception)
{
    ++thread->m_exception_state.m_exception_depth;

    if (thread->m_exception_state.m_try_counter == 0) {
        // exception cannot be handled, no try block found
        if (thread->m_id == 0) {
            utf::printf(HYP_UTF8_CSTR("unhandled exception in main thread: %" PRIutf8s "\n"),
                HYP_UTF8_TOWIDE(exception.ToString()));
        } else {
            utf::printf(HYP_UTF8_CSTR("unhandled exception in thread #%d: %" PRIutf8s "\n"),
                thread->m_id + 1, HYP_UTF8_TOWIDE(exception.ToString()));
        }

        good = false;
    }
}

HeapValue *VMState::HeapAlloc(ExecutionThread *thread)
{
    AssertThrow(thread != nullptr);

    const SizeType heap_size = m_heap.Size();
        
    if (heap_size >= m_max_heap_objects) {
        if (heap_size >= GC_THRESHOLD_MAX) {
            // heap overflow.
            char buffer[256];
            std::sprintf(
                buffer,
                "out of budgeted heap memory : size is %zu, max is %d",
                heap_size,
                GC_THRESHOLD_MAX
            );

            ThrowException(thread, Exception(buffer));

            return nullptr;
        }

#if ENABLE_GC
        if (enable_auto_gc) {
            // run the gc
            GC();

            // check if size is still over the maximum,
            // and resize the maximum if necessary.
            if (m_heap.Size() >= m_max_heap_objects) {
                // resize max number of objects
                m_max_heap_objects = std::min(
                    1 << (unsigned)std::ceil(std::log(m_max_heap_objects) / std::log(2.0)),
                    GC_THRESHOLD_MAX
                );
            }
        }
#endif
    }

    return m_heap.Alloc();
}

void VMState::GC()
{
    DebugLog(
        LogType::Debug,
        "Begin gc\n"
    );

    m_exported_symbols.MarkAll();

    // mark stack objects on each thread
    for (UInt i = 0; i < VM_MAX_THREADS; i++) {
        if (m_threads[i] != nullptr) {
            m_threads[i]->m_stack.MarkAll();

            for (UInt j = 0; j < VM_NUM_REGISTERS; j++) {
                m_threads[i]->GetRegisters()[j].Mark();
            }
        }
    }

    UInt num_collected;
    m_heap.Sweep(&num_collected);

    DebugLog(
        LogType::Debug,
        "%u objects garbage collected\n",
        num_collected
    );
}

ExecutionThread *VMState::CreateThread()
{
    AssertThrow(m_num_threads < VM_MAX_THREADS);

    // find a free slot
    for (UInt i = 0; i < VM_MAX_THREADS; i++) {
        if (m_threads[i] == nullptr) {
            ExecutionThread *thread = new ExecutionThread();
            thread->m_id = i;

            m_threads[i] = thread;
            m_num_threads++;

            return thread;
        }
    }

    return nullptr;
}

void VMState::DestroyThread(int id)
{
    AssertThrow(id < VM_MAX_THREADS);

    ExecutionThread *thread = m_threads[id];

    if (thread != nullptr) {
        // purge the stack
        thread->m_stack.Purge();
        // reset exception state
        thread->m_exception_state = { };
        // reset register flags
        thread->m_regs.ResetFlags();

        // delete it
        delete m_threads[id];
        m_threads[id] = nullptr;

        m_num_threads--;
    }
}

} // namespace vm
} // namespace hyperion
