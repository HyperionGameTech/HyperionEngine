#ifndef STACK_MEMORY_HPP
#define STACK_MEMORY_HPP

#include <core/lib/HeapArray.hpp>
#include <script/vm/Value.hpp>
#include <system/Debug.hpp>

#include <Types.hpp>

namespace hyperion {
namespace vm {

class StackMemory
{
public:
    static constexpr SizeType STACK_SIZE = 20000;

    friend std::ostream &operator<<(std::ostream &os, const StackMemory &stack);

public:
    StackMemory();
    StackMemory(const StackMemory &other) = delete;
    StackMemory &operator=(const StackMemory &other) = delete;
    ~StackMemory();

    /** Purge all items on the stack */
    void Purge();
    /** Mark all items on the stack to not be garbage collected */
    void MarkAll();

    HYP_FORCE_INLINE Value *GetData() { return m_data.Data(); }
    HYP_FORCE_INLINE const Value *GetData() const { return m_data.Data(); }

    HYP_FORCE_INLINE SizeType GetStackPointer() const { return m_sp; }

    HYP_FORCE_INLINE Value &operator[](SizeType index)
    {
        AssertThrowMsg(index < STACK_SIZE, "out of bounds");
        return m_data[index];
    }

    HYP_FORCE_INLINE const Value &operator[](SizeType index) const
    {
        AssertThrowMsg(index < STACK_SIZE, "out of bounds");
        return m_data[index];
    }

    // return the top value from the stack
    HYP_FORCE_INLINE Value &Top()
    {
        AssertThrowMsg(m_sp > 0, "read from empty stack");
        return m_data[m_sp - 1];
    }

    // return the top value from the stack
    HYP_FORCE_INLINE const Value &Top() const
    {
        AssertThrowMsg(m_sp > 0, "read from empty stack");
        return m_data[m_sp - 1];
    }

    // push a value to the stack
    HYP_FORCE_INLINE void Push(const Value &value)
    {
        AssertThrowMsg(m_sp < STACK_SIZE, "stack overflow");
        m_data[m_sp++] = value;
    }

    // push a value to the stack
    HYP_FORCE_INLINE void Push(Value &&value)
    {
        AssertThrowMsg(m_sp < STACK_SIZE, "stack overflow");
        m_data[m_sp++] = std::move(value);
    }

    // pop top value from the stack
    HYP_FORCE_INLINE void Pop()
    {
        AssertThrowMsg(m_sp > 0, "pop from empty stack");
        m_sp--;
    }

    // pop top n value(s) from the stack
    HYP_FORCE_INLINE void Pop(SizeType count)
    {
        AssertThrowMsg(m_sp >= count, "pop from empty stack");
        m_sp -= count;
    }

    HeapArray<Value, STACK_SIZE>    m_data;
    SizeType                        m_sp;
};

} // namespace vm
} // namespace hyperion

#endif
