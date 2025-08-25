#pragma once

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/StackTrace.hpp>
#include <script/vm/Tracemap.hpp>
#include <script/vm/ExportedSymbolTable.hpp>

#include <core/containers/HeapArray.hpp>

#include <core/Types.hpp>

#include <array>
#include <limits>
#include <cstdint>
#include <cstdio>

#define MAIN_THREAD m_threads[0]

#define MATCH_TYPES(leftType, rightType) \
    ((leftType) < (rightType)) ? (rightType) : (leftType)

namespace hyperion {

class APIInstance;

extern vm::Value ScriptApi_MakeValue(const vm::Script_VMData& data);
extern vm::Value ScriptApi_MakeValue(const vm::Number& number);
extern vm::Value ScriptApi_MakeValue(HypData&& data);
extern vm::Value ScriptApi_MakeRef(vm::Value& refValue);
extern vm::Value ScriptApi_MakeRef(vm::Value& refValue, vm::GC* gc, bool promoteToTrackedMemory);
extern vm::Value ScriptApi_ShallowCopy(vm::Value& refValue, vm::GC* gc);

namespace vm {

class GC;

static constexpr uint32 VM_NUM_REGISTERS = 8;

struct Script_RegisterMemory
{
    Value m_reg[VM_NUM_REGISTERS];
    int m_flags = 0;

    HYP_FORCE_INLINE Value& operator[](uint8 index)
    {
        return m_reg[index];
    }

    HYP_FORCE_INLINE void ResetFlags()
    {
        m_flags = 0;
    }
};

class Script_StaticMemory
{
public:
    static const uint16 staticSize;

public:
    Script_StaticMemory();
    Script_StaticMemory(const Script_StaticMemory& other) = delete;
    Script_StaticMemory& operator=(const Script_StaticMemory& other) = delete;
    Script_StaticMemory(Script_StaticMemory&& other) noexcept = delete;
    Script_StaticMemory& operator=(Script_StaticMemory&& other) noexcept = delete;
    ~Script_StaticMemory();

    HYP_FORCE_INLINE Value& operator[](SizeType index)
    {
        AssertDebug(index < staticSize, "out of bounds");
        return m_data[index];
    }

private:
    Value* m_data;
};

class Script_StackMemory
{
public:
    static constexpr SizeType STACK_SIZE = 20000;

    friend std::ostream& operator<<(std::ostream& os, const Script_StackMemory& stack);

public:
    Script_StackMemory();
    Script_StackMemory(const Script_StackMemory& other) = delete;
    Script_StackMemory& operator=(const Script_StackMemory& other) = delete;
    ~Script_StackMemory();

    /** Purge all items on the stack */
    void Purge();
    /** Mark all items on the stack to not be garbage collected */
    void MarkAll();

    HYP_FORCE_INLINE Value* GetData()
    {
        return reinterpret_cast<Value*>(m_data.Data());
    }
    HYP_FORCE_INLINE const Value* GetData() const
    {
        return reinterpret_cast<const Value*>(m_data.Data());
    }

    HYP_FORCE_INLINE SizeType GetStackPointer() const
    {
        return m_sp;
    }

    HYP_FORCE_INLINE Value& operator[](SizeType index)
    {
        AssertDebug(index < STACK_SIZE, "out of bounds");
        AssertDebug(index < m_sp, "reading uninitialized stack memory");

        return m_data[index].Get();
    }

    HYP_FORCE_INLINE const Value& operator[](SizeType index) const
    {
        Assert(index < STACK_SIZE, "out of bounds");
        Assert(index < m_sp, "reading uninitialized stack memory");

        return m_data[index].Get();
    }

    // return the top value from the stack
    HYP_FORCE_INLINE Value& Top()
    {
        Assert(m_sp > 0, "read from empty stack");
        return m_data[m_sp - 1].Get();
    }

    // return the top value from the stack
    HYP_FORCE_INLINE const Value& Top() const
    {
        Assert(m_sp > 0, "read from empty stack");
        return m_data[m_sp - 1].Get();
    }

    // push a value to the stack
    HYP_FORCE_INLINE void Push(Value&& value)
    {
        Assert(m_sp < STACK_SIZE, "stack overflow");
        new (&m_data[m_sp++]) Value(std::move(value));
    }

    // pop top value from the stack
    HYP_FORCE_INLINE void Pop()
    {
        Assert(m_sp > 0, "pop from empty stack");
        m_sp--;

        m_data[m_sp].Destruct();
    }

    // pop top n value(s) from the stack
    HYP_FORCE_INLINE void Pop(SizeType count)
    {
        Assert(m_sp >= count, "pop from empty stack");

        for (SizeType i = 0; i < count; i++)
        {
            m_data[--m_sp].Destruct();
        }
    }

    HeapArray<ValueStorage<Value>, STACK_SIZE> m_data;
    SizeType m_sp;
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

class VM
{
public:
    VM(APIInstance& apiInstance);
    VM(const VM& other) = delete;
    VM& operator=(const VM& other) = delete;
    VM(VM&& other) noexcept = delete;
    VM& operator=(VM&& other) noexcept = delete;
    ~VM();

    void PushNativeFunctionPtr(Script_NativeFunction ptr);

    void Invoke(
        InstructionHandler* handler,
        Value&& value,
        uint8 nargs);

    void InvokeNow(
        BytecodeStream* bs,
        Value&& value,
        uint8 nargs);

    void Execute(BytecodeStream* bs);

    /** Reset the state of the VM, destroying all heap objects,
        stack objects and exception flags, etc.
     */
    void Reset();

    void ThrowException(Script_ExecutionThread* thread, const Exception& exception);

    Script_ExecutionThread* GetMainThread() const
    {
        return m_executionThread;
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

    Script_ExecutionThread* m_executionThread = nullptr;
    Script_StaticMemory m_staticMemory;
    GC* m_gc = nullptr;
    VM* m_vm = nullptr;
    Tracemap m_tracemap;
    ExportedSymbolTable m_exportedSymbols;
    Exception* m_unhandledException = nullptr;

private:
    bool HandleException(InstructionHandler* handler);
    void CreateStackTrace(Script_ExecutionThread* thread, StackTrace* out);

    APIInstance& m_apiInstance;
};

} // namespace vm
} // namespace hyperion
