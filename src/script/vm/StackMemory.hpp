#pragma once

#include <core/containers/HeapArray.hpp>
#include <script/vm/Value.hpp>
#include <core/debug/Debug.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace vm {

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
        Assert(index < STACK_SIZE, "out of bounds");
        return m_data[index].Get();
    }

    HYP_FORCE_INLINE const Value& operator[](SizeType index) const
    {
        Assert(index < STACK_SIZE, "out of bounds");
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

} // namespace vm
} // namespace hyperion
