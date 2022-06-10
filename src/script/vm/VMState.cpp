#include <script/vm/VMState.hpp>

#include <util/utf8.hpp>

#include <algorithm>
#include <thread>
#include <iostream>
#include <cmath>
#include <mutex>

namespace hyperion {
namespace vm {

static std::mutex mtx;

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
    // purge the heap
    m_heap.Purge();
    // reset heap threshold
    m_max_heap_objects = GC_THRESHOLD_MIN;
    // purge static memory
    m_static_memory.Purge();

    for (int i = 0; i < VM_MAX_THREADS; i++) {
        DestroyThread(i);
    }

    // we're good to go
    good = true;
}

void VMState::ThrowException(ExecutionThread *thread, const Exception &exception)
{
    thread->m_exception_state.m_exception_occured = true;

    if (thread->m_exception_state.m_try_counter <= 0) {
        // exception cannot be handled, no try block found
        if (thread->m_id == 0) {
            utf::printf(UTF8_CSTR("unhandled exception in main thread: %" PRIutf8s "\n"),
                UTF8_TOWIDE(exception.ToString()));
        } else {
            utf::printf(UTF8_CSTR("unhandled exception in thread #%d: %" PRIutf8s "\n"),
                thread->m_id + 1, UTF8_TOWIDE(exception.ToString()));
        }

        good = false;
    }
}

void VMState::CloneValue(const Value &other, ExecutionThread *thread, Value &out)
{
    switch (other.m_type) {
        case Value::HEAP_POINTER:
            if (other.m_value.ptr != nullptr) {
                HeapValue *hv = HeapAlloc(thread);
                AssertThrow(hv != nullptr);

                hv->AssignCopy(*other.m_value.ptr);

                out.m_type = Value::HEAP_POINTER;
                out.m_value.ptr = hv;

                break;
            }
            // fallthrough
        default:
            out = Value(other);
    }
}

HeapValue *VMState::HeapAlloc(ExecutionThread *thread)
{
    AssertThrow(thread != nullptr);

    const size_t heap_size = m_heap.Size();
        
    if (heap_size >= m_max_heap_objects) {
        if (heap_size >= GC_THRESHOLD_MAX) {
            // heap overflow.
            char buffer[256];
            std::sprintf(
                buffer,
                "heap overflow, heap size is %zu, max is %d",
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
    std::lock_guard<std::mutex> lock(mtx);

    // mark stack objects on each thread
    for (int i = 0; i < VM_MAX_THREADS; i++) {
        if (m_threads[i] != nullptr) {
            m_threads[i]->m_stack.MarkAll();
            for (int j = 0; j < VM_NUM_REGISTERS; j++) {
                m_threads[i]->GetRegisters()[j].Mark();
            }
        }
    }

    m_heap.Sweep();

    //utf::cout << "gc()\n";
}

ExecutionThread *VMState::CreateThread()
{
    AssertThrow(m_num_threads < VM_MAX_THREADS);

    // find a free slot
    for (int i = 0; i < VM_MAX_THREADS; i++) {
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
        thread->m_exception_state.Reset();
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
