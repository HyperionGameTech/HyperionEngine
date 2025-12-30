#pragma once

#include <script/vm/Stream.hpp>
#include <script/vm/Trace.hpp>
#include <script/vm/Tracemap.hpp>
#include <script/vm/SymbolTable.hpp>

#include <core/containers/HeapArray.hpp>

#include <core/reflection/BoxedValue.hpp>

#include <core/Types.hpp>

#include <limits>
#include <cstdint>
#include <cstdio>

namespace Hyperion {

/*! \brief Table for type promotion for binops. */
static constexpr int typePromoTable[10][10] = {
    // NT_U8=0, NT_I8=1, NT_U16=2, NT_I16=3, NT_U32=4, NT_I32=5, NT_U64=6, NT_I64=7, NT_F32=8, NT_F64=9
    /*U8*/ { 5, 5, 5, 5, 4, 5, 6, 7, 8, 9 },
    /*I8*/ { 5, 5, 5, 5, 4, 5, 6, 7, 8, 9 },
    /*U16*/ { 5, 5, 5, 5, 4, 5, 6, 7, 8, 9 },
    /*I16*/ { 5, 5, 5, 5, 4, 5, 6, 7, 8, 9 },
    /*U32*/ { 4, 4, 4, 4, 4, 4, 6, 7, 8, 9 },
    /*I32*/ { 5, 5, 5, 5, 4, 5, 6, 7, 8, 9 },
    /*U64*/ { 6, 6, 6, 6, 6, 6, 6, 7, 9, 9 },
    /*I64*/ { 7, 7, 7, 7, 7, 7, 6, 7, 9, 9 },
    /*F32*/ { 8, 8, 8, 8, 8, 8, 8, 8, 8, 9 },
    /*F64*/ { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 }
};

#define MATCH_TYPES(leftType, rightType) ((NumericType)typePromoTable[(leftType)][(rightType)])

extern BoxedValue MakeValue(const Script_VMData& data);
extern BoxedValue MakeValue(const Number& number);
extern BoxedValue MakeValue(BoxedValue&& data);
extern BoxedValue MakeRef(BoxedValue* refValue);
extern BoxedValue MakeTrackedRef(BoxedValue* refValue, Script_GC* gc);
extern BoxedValue ShallowCopy(BoxedValue& value, Script_GC* gc);
extern bool ShouldValuePassByRef(const BoxedValue& value);
extern const char* GetTypeString(const BoxedValue& data);
extern String ValueToString(const BoxedValue& data, int currDepth = 0);

class Script_GC;

struct Script_RegisterMemory
{
    static constexpr uint32 NumRegisters = 8;

    BoxedValue regs[NumRegisters];
    int flags = 0;

    Script_RegisterMemory();

    HYP_FORCE_INLINE BoxedValue& operator[](uint8 index)
    {
        return regs[index];
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

    HYP_FORCE_INLINE BoxedValue& operator[](SizeType index)
    {
        AssertDebug(index < staticSize, "out of bounds");
        return m_data[index];
    }

private:
    BoxedValue* m_data;
};

class Script_StackMemory
{
public:
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

    HYP_FORCE_INLINE BoxedValue* GetData()
    {
        return m_data;
    }

    HYP_FORCE_INLINE const BoxedValue* GetData() const
    {
        return m_data;
    }

    HYP_FORCE_INLINE SizeType GetStackPointer() const
    {
        return m_sp;
    }

    HYP_FORCE_INLINE BoxedValue& operator[](SizeType index)
    {
        return m_data[index];
    }

    HYP_FORCE_INLINE const BoxedValue& operator[](SizeType index) const
    {
        return m_data[index];
    }

    // return the top value from the stack
    HYP_FORCE_INLINE BoxedValue& Top()
    {
        return m_data[m_sp - 1];
    }

    // return the top value from the stack
    HYP_FORCE_INLINE const BoxedValue& Top() const
    {
        return m_data[m_sp - 1];
    }

    // push a value to the stack
    HYP_FORCE_INLINE void Push(BoxedValue&& value)
    {
        new (&m_data[m_sp++]) BoxedValue(std::move(value));
    }

    // pop top value from the stack
    HYP_FORCE_INLINE void Pop()
    {
        m_sp--;

        m_data[m_sp].~BoxedValue();
    }

    // pop top n value(s) from the stack
    HYP_FORCE_INLINE void Pop(SizeType count)
    {
        for (SizeType i = 0; i < count; i++)
        {
            m_data[--m_sp].~BoxedValue();
        }
    }

    BoxedValue* m_data;
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
    Script_SymbolTable exportedSymbols;
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
        BoxedValue&& value,
        uint8 nargs);

    void InvokeNow(
        Script_Instance* instance,
        BoxedValue&& value,
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

    Script_StaticMemory m_staticMemory;
    Script_GC* m_gc = nullptr;
    Script_Tracemap m_tracemap;
    Script_Exception* m_unhandledException = nullptr;

private:
    bool HandleException(Script_Instance* instance);
    void CreateTrace(Script_Instance* instance, Script_Trace* outTrace);
};

} // namespace Hyperion
