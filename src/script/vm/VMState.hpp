#ifndef VM_STATE_HPP
#define VM_STATE_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/LinkedList.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/HeapArray.hpp>
#include <core/lib/ValueStorage.hpp>

#include <script/vm/StackMemory.hpp>
#include <script/vm/StaticMemory.hpp>
#include <script/vm/HeapMemory.hpp>
#include <script/vm/Exception.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/vm/Tracemap.hpp>
#include <script/vm/ExportedSymbolTable.hpp>

#include <Types.hpp>

#include <util/NonOwningPtr.hpp>

#include <atomic>

#define ENABLE_GC 1
#define GC_THRESHOLD_MIN 150
#define GC_THRESHOLD_MAX 50000

#define VM_MAX_THREADS 1//8
#define VM_NUM_REGISTERS 8

namespace hyperion {
namespace vm {

// forward declaration
class VM;

struct Registers
{
    Value   m_reg[VM_NUM_REGISTERS];
    int     m_flags = 0;

    Value &operator[](UInt8 index) { return m_reg[index]; }
    void ResetFlags() { m_flags = 0; }
};

struct ExceptionState
{
    // incremented each time BEGIN_TRY is encountered,
    // decremented each time END_TRY is encountered
    UInt m_try_counter = 0;

    // set to true when an exception occurs,
    // set to false when handled in BEGIN_TRY
    UInt m_exception_depth = 0;

    bool HasExceptionOccurred() const { return m_exception_depth != 0; }
};

struct ExecutionThread
{
    friend struct VMState;

    StackMemory     m_stack;
    ExceptionState  m_exception_state;
    Registers       m_regs;

    UInt            m_func_depth = 0;
    int             m_id = -1;

    StackMemory &GetStack()
        { return m_stack; }

    ExceptionState &GetExceptionState()
        { return m_exception_state; }

    Registers &GetRegisters()
        { return m_regs; }
};

struct DynModule
{
    UniquePtr<void> ptr;
};

struct VMState
{
    VMState();
    VMState(const VMState &other) = delete;
    ~VMState();

    ExecutionThread                     *m_threads[VM_MAX_THREADS];
    Heap                                m_heap;
    StaticMemory                        m_static_memory;
    non_owning_ptr<VM>                  m_vm;
    Tracemap                            m_tracemap;
    ExportedSymbolTable                 m_exported_symbols;
    FlatMap<UInt32, Weak<DynModule>>    m_dyn_modules;

    bool                                good = true;
    bool                                enable_auto_gc = ENABLE_GC;
    int                                 m_max_heap_objects = GC_THRESHOLD_MIN;

    /** Reset the state of the VM, destroying all heap objects,
        stack objects and exception flags, etc.
     */
    void Reset();

    void ThrowException(ExecutionThread *thread, const Exception &exception);
    HeapValue *HeapAlloc(ExecutionThread *thread);
    void GC();

    // void CloneValue(const Value &other, ExecutionThread *thread, Value &out);

    /** Add a thread */
    ExecutionThread *CreateThread();
    /** Destroy thread with ID */
    void DestroyThread(int id);
    
    /** Get the number of threads currently in use */
    SizeType GetNumThreads() const
        { return m_num_threads; }

    ExecutionThread *GetMainThread() const
        { AssertThrow(m_num_threads != 0); return m_threads[0]; }

    Heap &GetHeap()
        { return m_heap; }

    const Heap &GetHeap() const
        { return m_heap; }

    ExportedSymbolTable &GetExportedSymbols()
        { return m_exported_symbols; }

    const ExportedSymbolTable &GetExportedSymbols() const
        { return m_exported_symbols; }

private:
    SizeType m_num_threads = 0;
};

} // namespace vm
} // namespace hyperion

#endif
