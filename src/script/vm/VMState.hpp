#pragma once

#include <core/containers/Array.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HeapArray.hpp>
#include <core/utilities/ValueStorage.hpp>

#include <script/vm/StackMemory.hpp>
#include <script/vm/StaticMemory.hpp>
#include <script/vm/HeapMemory.hpp>
#include <script/vm/Exception.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/vm/Tracemap.hpp>
#include <script/vm/ExportedSymbolTable.hpp>

#include <core/Types.hpp>

#include <atomic>

#define ENABLE_GC 1
#define GC_THRESHOLD_MIN 150
#define GC_THRESHOLD_MAX 50000

#define VM_MAX_THREADS 1 // 8
#define VM_NUM_REGISTERS 8

namespace hyperion {
namespace vm {

// forward declaration
class VM;
class GC;

struct Script_RegisterMemory
{
    Value m_reg[VM_NUM_REGISTERS];
    int m_flags = 0;

    Value& operator[](uint8 index)
    {
        return m_reg[index];
    }

    void ResetFlags()
    {
        m_flags = 0;
    }
};

struct Script_ExceptionState
{
    // incremented each time BEGIN_TRY is encountered,
    // decremented each time END_TRY is encountered
    uint32 m_tryCounter = 0;

    // set to true when an exception occurs,
    // set to false when handled in BEGIN_TRY
    uint32 m_exceptionDepth = 0;

    bool HasExceptionOccurred() const
    {
        return m_exceptionDepth != 0;
    }
};

struct Script_ExecutionThread
{
    friend struct VMState;

    Script_StackMemory m_stack;
    Script_ExceptionState m_exceptionState;
    Script_RegisterMemory m_regs;

    uint32 m_funcDepth = 0;
    int m_id = -1;

    Script_StackMemory& GetStack()
    {
        return m_stack;
    }

    Script_ExceptionState& GetExceptionState()
    {
        return m_exceptionState;
    }

    Script_RegisterMemory& GetRegisters()
    {
        return m_regs;
    }
};

struct DynModule
{
    UniquePtr<void> ptr;
};

struct VMState
{
    VMState();
    VMState(const VMState& other) = delete;
    ~VMState();

    Script_ExecutionThread* m_threads[VM_MAX_THREADS];
    Heap m_heap;
    GC* m_gc = nullptr;
    StaticMemory m_staticMemory;
    VM* m_vm = nullptr;
    Tracemap m_tracemap;
    ExportedSymbolTable m_exportedSymbols;
    FlatMap<uint32, Weak<DynModule>> m_dynModules;

    bool good = true;
    bool enableAutoGc = ENABLE_GC;
    int m_maxHeapObjects = GC_THRESHOLD_MIN;

    /** Reset the state of the VM, destroying all heap objects,
        stack objects and exception flags, etc.
     */
    void Reset();

    void ThrowException(Script_ExecutionThread* thread, const Exception& exception);
    HeapValue* HeapAlloc(Script_ExecutionThread* thread);
    // void GC();

    // void CloneValue(const Value &other, Script_ExecutionThread *thread, Value &out);

    /** Add a thread */
    Script_ExecutionThread* CreateThread();
    /** Destroy thread with ID */
    void DestroyThread(int id);

    /** Get the number of threads currently in use */
    SizeType GetNumThreads() const
    {
        return m_numThreads;
    }

    Script_ExecutionThread* GetMainThread() const
    {
        Assert(m_numThreads != 0);
        return m_threads[0];
    }

    Heap& GetHeap()
    {
        return m_heap;
    }

    const Heap& GetHeap() const
    {
        return m_heap;
    }

    GC* GetGC() const
    {
        return m_gc;
    }

    ExportedSymbolTable& GetExportedSymbols()
    {
        return m_exportedSymbols;
    }

    const ExportedSymbolTable& GetExportedSymbols() const
    {
        return m_exportedSymbols;
    }

private:
    SizeType m_numThreads = 0;
};

} // namespace vm
} // namespace hyperion
