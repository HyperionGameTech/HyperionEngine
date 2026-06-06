#pragma once

#include <Lang/VM/BytecodeStream.hpp>
#include <Lang/VM/Trace.hpp>
#include <Lang/VM/Tracemap.hpp>
#include <Lang/VM/SymbolTable.hpp>

#include <Core/Containers/HeapArray.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Types.hpp>
#include <Core/Utilities/Span.hpp>

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

extern BoxedValue MakeValue(const ScriptObjectData& data);
extern BoxedValue MakeValue(const Number& number);
extern BoxedValue MakeValue(BoxedValue&& value);
extern BoxedValue MakeRef(BoxedValue* refValue);
extern BoxedValue MakeTrackedRef(BoxedValue* refValue, GarbageCollector* gc);
extern BoxedValue ShallowCopy(BoxedValue& value, GarbageCollector* gc);
extern bool ShouldValuePassByRef(const BoxedValue& value);
extern const char* GetTypeString(const BoxedValue& value);
extern String ValueToString(const BoxedValue& value, int currDepth = 0);

class GarbageCollector;

struct Script_RegisterMemory
{
    static constexpr uint32 NumRegisters = 4;

    ValueStorage<BoxedValue, NumRegisters> values;

    int flags = 0;

    Script_RegisterMemory();

    Script_RegisterMemory(const Script_RegisterMemory& other) = delete;
    Script_RegisterMemory& operator=(const Script_RegisterMemory& other) = delete;

    ~Script_RegisterMemory();

    HYP_FORCE_INLINE BoxedValue& operator[](int index)
    {
        return values.GetPointer()[index];
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

    HYP_FORCE_INLINE BoxedValue& operator[](size_t index)
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
    Script_StackMemory();
    
    Script_StackMemory(const Script_StackMemory& other) = delete;
    Script_StackMemory& operator=(const Script_StackMemory& other) = delete;

    ~Script_StackMemory();

    void Purge();

    HYP_FORCE_INLINE BoxedValue* GetData()
    {
        return m_data;
    }

    HYP_FORCE_INLINE const BoxedValue* GetData() const
    {
        return m_data;
    }

    HYP_FORCE_INLINE size_t GetStackPointer() const
    {
        return m_sp;
    }

    HYP_FORCE_INLINE BoxedValue& operator[](size_t index)
    {
        return m_data[index];
    }

    HYP_FORCE_INLINE const BoxedValue& operator[](size_t index) const
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
    HYP_FORCE_INLINE void Pop(size_t count)
    {
        for (size_t i = 0; i < count; i++)
        {
            m_data[--m_sp].~BoxedValue();
        }
    }

    BoxedValue* m_data;
    size_t m_sp;
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
    
    Script_RegisterMemory m_regs;
    Script_StackMemory m_stack;

    Script_ExceptionState m_exceptionState;

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

struct ScriptInstance
{
    BytecodeStream stream;
    Script_ExecutionThread thread;
    SymbolTable exportedSymbols;
};

class VirtualMachine
{
public:
    VirtualMachine();
    
    VirtualMachine(const VirtualMachine& other) = delete;
    VirtualMachine& operator=(const VirtualMachine& other) = delete;
    
    VirtualMachine(VirtualMachine&& other) noexcept = delete;
    VirtualMachine& operator=(VirtualMachine&& other) noexcept = delete;

    ~VirtualMachine();

    void Invoke(ScriptInstance* instance, BoxedValue&& value, uint8 nargs);
    void InvokeImmediate(ScriptInstance* instance, BoxedValue&& value, uint8 nargs);

    void Execute(ScriptInstance* instance);

    void ThrowException(ScriptInstance* instance, const Exception& exception);

    void CollectGarbage(Span<ScriptInstance*> instances);

    GarbageCollector* GetGC() const
    {
        return m_gc;
    }

    Script_StaticMemory m_staticMemory;
    GarbageCollector* m_gc = nullptr;
    Tracemap m_tracemap;
    Exception* m_unhandledException = nullptr;

private:
    bool HandleException(ScriptInstance* instance);
    void CreateTrace(ScriptInstance* instance, Script_Trace* outTrace);
};

} // namespace Hyperion
