#pragma once

#include <script/vm/Stream.hpp>
#include <script/vm/Trace.hpp>
#include <script/vm/Tracemap.hpp>
#include <script/vm/SymbolTable.hpp>

#include <core/containers/HeapArray.hpp>

#include <core/Types.hpp>

#include <array>
#include <limits>
#include <cstdint>
#include <cstdio>

#define MAIN_THREAD m_threads[0]
namespace hyperion {

/*! \brief Table for type promotion for binops. */
static constexpr int g_typePromoLut[10][10] = {
    // NT_U8=0, NT_I8=1, NT_U16=2, NT_I16=3, NT_U32=4, NT_I32=5, NT_U64=6, NT_I64=7, NT_F32=8, NT_F64=9
    /*U8*/ { 0, 1, 2, 3, 4, 7, 6, 7, 8, 9 },
    /*I8*/ { 1, 1, 3, 3, 7, 5, 7, 7, 8, 9 },
    /*U16*/ { 2, 3, 2, 3, 4, 7, 6, 7, 8, 9 },
    /*I16*/ { 3, 3, 3, 3, 7, 5, 7, 7, 8, 9 },
    /*U32*/ { 4, 7, 4, 7, 4, 7, 6, 7, 8, 9 },
    /*I32*/ { 7, 5, 7, 5, 7, 5, 7, 7, 8, 9 },
    /*U64*/ { 6, 7, 6, 7, 6, 7, 6, 7, 9, 9 },
    /*I64*/ { 7, 7, 7, 7, 7, 7, 7, 7, 9, 9 },
    /*F32*/ { 8, 8, 8, 8, 8, 8, 9, 9, 8, 9 },
    /*F64*/ { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 }
};

#define MATCH_TYPES(leftType, rightType) ((NumericType)g_typePromoLut[(leftType)][(rightType)])

extern Script_Value ScriptApi_MakeValue(const Script_VMData& data);
extern Script_Value ScriptApi_MakeValue(const Number& number);
extern Script_Value ScriptApi_MakeValue(HypData&& data);
extern Script_Value ScriptApi_MakeRef(Script_Value* refValue);
extern Script_Value ScriptApi_MakeTrackedRef(Script_Value* refValue, Script_GC* gc);
extern Script_Value ScriptApi_ShallowCopy(Script_Value& refValue, Script_GC* gc);
extern bool ScriptApi_ShouldValuePassByRef(const Script_Value& value);

class Script_GC;

static constexpr uint32 VM_NUM_REGISTERS = 8;

struct Script_RegisterMemory
{
    Script_Value data[VM_NUM_REGISTERS];
    int flags = 0;

    Script_RegisterMemory();

    HYP_FORCE_INLINE Script_Value& operator[](uint8 index)
    {
        return data[index];
    }

    HYP_FORCE_INLINE void ResetFlags()
    {
        flags = 0;
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

    HYP_FORCE_INLINE Script_Value& operator[](SizeType index)
    {
        AssertDebug(index < staticSize, "out of bounds");
        return m_data[index];
    }

private:
    Script_Value* m_data;
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

    HYP_FORCE_INLINE Script_Value* GetData()
    {
        return reinterpret_cast<Script_Value*>(m_data.Data());
    }
    HYP_FORCE_INLINE const Script_Value* GetData() const
    {
        return reinterpret_cast<const Script_Value*>(m_data.Data());
    }

    HYP_FORCE_INLINE SizeType GetStackPointer() const
    {
        return m_sp;
    }

    HYP_FORCE_INLINE Script_Value& operator[](SizeType index)
    {
        AssertDebug(index < STACK_SIZE, "out of bounds");
        AssertDebug(index < m_sp, "reading uninitialized stack memory");

        return m_data[index].Get();
    }

    HYP_FORCE_INLINE const Script_Value& operator[](SizeType index) const
    {
        Assert(index < STACK_SIZE, "out of bounds");
        Assert(index < m_sp, "reading uninitialized stack memory");

        return m_data[index].Get();
    }

    // return the top value from the stack
    HYP_FORCE_INLINE Script_Value& Top()
    {
        Assert(m_sp > 0, "read from empty stack");
        return m_data[m_sp - 1].Get();
    }

    // return the top value from the stack
    HYP_FORCE_INLINE const Script_Value& Top() const
    {
        Assert(m_sp > 0, "read from empty stack");
        return m_data[m_sp - 1].Get();
    }

    // push a value to the stack
    HYP_FORCE_INLINE void Push(Script_Value&& value)
    {
        Assert(m_sp < STACK_SIZE, "stack overflow");
        new (&m_data[m_sp++]) Script_Value(std::move(value));
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

    HeapArray<ValueStorage<Script_Value>, STACK_SIZE> m_data;
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

struct Script_Instance
{
    Script_Stream stream;
    Script_ExecutionThread thread;
};

class Script_Interpreter
{
public:
    Script_Interpreter();
    Script_Interpreter(const Script_Interpreter& other) = delete;
    Script_Interpreter& operator=(const Script_Interpreter& other) = delete;
    Script_Interpreter(Script_Interpreter&& other) noexcept = delete;
    Script_Interpreter& operator=(Script_Interpreter&& other) noexcept = delete;
    ~Script_Interpreter();

    void Invoke(
        Script_Instance* instance,
        Script_Value&& value,
        uint8 nargs);

    void InvokeNow(
        Script_Instance* instance,
        Script_Value&& value,
        uint8 nargs);

    void Execute(Script_Instance* instance);

    /** Reset the state of the Script_Interpreter, destroying all heap objects,
        stack objects and exception flags, etc.
     */
    void Reset();

    void ThrowException(Script_Instance* instance, const Script_Exception& exception);

    Script_GC* GetGC() const
    {
        return m_gc;
    }

    Script_SymbolTable& GetExportedSymbols()
    {
        return m_exportedSymbols;
    }

    const Script_SymbolTable& GetExportedSymbols() const
    {
        return m_exportedSymbols;
    }

    Script_StaticMemory m_staticMemory;
    Script_GC* m_gc = nullptr;
    Script_Tracemap m_tracemap;
    Script_SymbolTable m_exportedSymbols;
    Script_Exception* m_unhandledException = nullptr;

private:
    bool HandleException(Script_Instance* instance);
    void CreateTrace(Script_Instance* instance, Script_Trace* outTrace);
};

} // namespace hyperion
