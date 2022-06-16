#ifndef VM_HPP
#define VM_HPP

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VMState.hpp>
#include <script/vm/StackTrace.hpp>

#include <array>
#include <limits>
#include <cstdint>
#include <cstdio>

#define MAIN_THREAD m_threads[0]

#define IS_VALUE_STRING(value, out) \
    ((value).m_type == Value::HEAP_POINTER && \
    (out = (value).m_value.ptr->GetPointer<ImmutableString>()))

#define IS_VALUE_ARRAY(value, out) \
    ((value).m_type == Value::HEAP_POINTER && \
    (out = (value).m_value.ptr->GetPointer<Array>()))

#define MATCH_TYPES(left_type, right_type) \
    ((left_type) < (right_type)) ? (right_type) : (left_type)

namespace hyperion {
namespace vm {

enum CompareFlags : int {
    NONE = 0x00,
    EQUAL = 0x01,
    GREATER = 0x02,
    // note that there is no LESS flag.
    // the compiler must make appropriate changes
    // to insure that the operands are switched to
    // use only the GREATER or EQUAL flags.
};

class VM {
public:
    VM();
    VM(const VM &other) = delete;
    ~VM();

    void PushNativeFunctionPtr(NativeFunctionPtr_t ptr);

    inline VMState &GetState() { return m_state; }
    inline const VMState &GetState() const { return m_state; }

    static void Print(const Value &value);
    static void Invoke(
        InstructionHandler *handler,
        const Value &value,
        uint8_t nargs
    );

    void InvokeNow(
        BytecodeStream *bs,
        const Value &value,
        uint8_t nargs
    );

    void Execute(BytecodeStream *bs);

    /** Returns -1 on error */
    static inline int CompareAsPointers(
        Value *lhs,
        Value *rhs)
    {
        HeapValue *a = lhs->m_value.ptr;
        HeapValue *b = rhs->m_value.ptr;

        if (a == b) {
            // pointers equal, drop out early.
            return EQUAL;
        } else if (a == nullptr || b == nullptr) {
            // one of them (not both) is null, not equal
            return NONE;
        } else if (a->GetTypeId() == b->GetTypeId()) {
            // comparable types, perform full comparison.
            return (a->operator==(*b)) ? EQUAL : NONE;
        }

        // error
        return -1;
    }

    static inline int CompareAsFunctions(
        Value *lhs,
        Value *rhs)
    {
        return (lhs->m_value.func.m_addr == rhs->m_value.func.m_addr)
            ? EQUAL
            : NONE;
    }

    static inline int CompareAsNativeFunctions(
        Value *lhs,
        Value *rhs)
    {
        return (lhs->m_value.native_func == rhs->m_value.native_func)
            ? EQUAL
            : NONE;
    }

private:
    void HandleException(InstructionHandler *handler);
    void CreateStackTrace(ExecutionThread *thread, StackTrace *out);

    VMState m_state;

    uint32_t m_invoke_now_level;
};

} // namespace vm
} // namespace hyperion

#endif
