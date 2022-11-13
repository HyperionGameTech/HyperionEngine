#ifndef STACK_MEMORY_HPP
#define STACK_MEMORY_HPP

#include <script/vm/Value.hpp>
#include <system/Debug.hpp>

#include <Types.hpp>

#include <array>

namespace hyperion {
namespace vm {

class StackMemory
{
public:
    static const SizeType STACK_SIZE;

    friend std::ostream &operator<<(std::ostream &os, const StackMemory &stack);

public:
    StackMemory();
    StackMemory(const StackMemory &other) = delete;
    ~StackMemory();

    /** Purge all items on the stack */
    void Purge();
    /** Mark all items on the stack to not be garbage collected */
    void MarkAll();

    Value *GetData() { return m_data; }
    const Value *GetData() const { return m_data; }
    SizeType GetStackPointer() const { return m_sp; }

    Value &operator[](size_t index)
    {
        AssertThrowMsg(index < STACK_SIZE, "out of bounds");
        return m_data[index];
    }

    const Value &operator[](size_t index) const
    {
        AssertThrowMsg(index < STACK_SIZE, "out of bounds");
        return m_data[index];
    }

    // return the top value from the stack
    Value &Top()
    {
        AssertThrowMsg(m_sp > 0, "stack underflow");
        return m_data[m_sp - 1];
    }

    // return the top value from the stack
    const Value &Top() const
    {
        AssertThrowMsg(m_sp > 0, "stack underflow");
        return m_data[m_sp - 1];
    }

    // push a value to the stack
    void Push(const Value &value)
    {
        AssertThrowMsg(m_sp < STACK_SIZE, "stack overflow");
        m_data[m_sp++] = value;
    }

    // pop top value from the stack
    void Pop()
    {
        AssertThrowMsg(m_sp > 0, "stack underflow");
        m_sp--;
    }

    // pop top n value(s) from the stack
    void Pop(size_t n)
    {
        AssertThrowMsg(m_sp >= n, "stack underflow");
        m_sp -= n;
    }

    Value *m_data;
    size_t m_sp;
};

} // namespace vm
} // namespace hyperion

#endif
