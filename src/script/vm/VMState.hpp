#ifndef VM_STATE_HPP
#define VM_STATE_HPP

#include <script/vm/StackMemory.hpp>
#include <script/vm/StaticMemory.hpp>
#include <script/vm/HeapMemory.hpp>
#include <script/vm/Exception.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/vm/Tracemap.hpp>
#include <script/vm/ExportedSymbolTable.hpp>

#include <types.h>

#include <util/non_owning_ptr.h>

#define ENABLE_GC 1
#define GC_THRESHOLD_MIN 150
#define GC_THRESHOLD_MAX 5000

#define VM_MAX_THREADS 1//8
#define VM_NUM_REGISTERS 8

namespace hyperion {
namespace vm {

// forward declaration
class VM;

struct Registers {
    Value m_reg[VM_NUM_REGISTERS];
    int m_flags = 0;

    inline Value &operator[](UInt8 index) { return m_reg[index]; }
    inline void ResetFlags()              { m_flags = 0; }
};

struct ExceptionState {
    // incremented each time BEGIN_TRY is encountered,
    // decremented each time END_TRY is encountered
    int m_try_counter = 0;

    // set to true when an exception occurs,
    // set to false when handled in BEGIN_TRY
    bool m_exception_occured = false;

    inline bool HasExceptionOccurred() const { return m_exception_occured; }
    inline void Reset() { m_try_counter = 0; m_exception_occured = false; }
};

struct ExecutionThread {
    friend struct VMState;

    Stack m_stack;
    ExceptionState m_exception_state;
    Registers m_regs;

    UInt m_func_depth = 0;

    inline Stack &GetStack() { return m_stack; }
    inline ExceptionState &GetExceptionState() { return m_exception_state; }
    inline Registers &GetRegisters() { return m_regs; }
    inline int GetId() const { return m_id; }

private:
    int m_id;
};

struct VMState {
    VMState();
    VMState(const VMState &other) = delete;
    ~VMState();

    ExecutionThread *m_threads[VM_MAX_THREADS];
    Heap m_heap;
    StaticMemory m_static_memory;
    non_owning_ptr<VM> m_vm;
    Tracemap m_tracemap;
    ExportedSymbolTable m_exported_symbols;

    bool good = true;
    bool enable_auto_gc = ENABLE_GC;
    int m_max_heap_objects = GC_THRESHOLD_MIN;

    /** Reset the state of the VM, destroying all heap objects,
        stack objects and exception flags, etc.
     */
    void Reset();

    void ThrowException(ExecutionThread *thread, const Exception &exception);
    HeapValue *HeapAlloc(ExecutionThread *thread);
    void GC();

    void CloneValue(const Value &other, ExecutionThread *thread, Value &out);

    /** Add a thread */
    ExecutionThread *CreateThread();
    /** Destroy thread with ID */
    void DestroyThread(int id);
    
    /** Get the number of threads currently in use */
    inline size_t GetNumThreads() const { return m_num_threads; }
    inline ExecutionThread *GetMainThread() const { AssertThrow(m_num_threads != 0); return m_threads[0]; }

    inline Heap &GetHeap() { return m_heap; }
    inline StaticMemory &GetStaticMemory() { return m_static_memory; }

    inline ExportedSymbolTable &GetExportedSymbols() { return m_exported_symbols; }
    inline const ExportedSymbolTable &GetExportedSymbols() const { return m_exported_symbols; }

private:
    size_t m_num_threads = 0;
};

} // namespace vm
} // namespace hyperion

#endif
