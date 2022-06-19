#ifndef VM_HPP
#define VM_HPP

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VMState.hpp>
#include <script/vm/StackTrace.hpp>

#include <types.h>

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

class VM {
public:
    VM();
    VM(const VM &other) = delete;
    ~VM();

    void PushNativeFunctionPtr(NativeFunctionPtr_t ptr);

    VMState &GetState()             { return m_state; }
    const VMState &GetState() const { return m_state; }
    
    static void Invoke(
        InstructionHandler *handler,
        const Value &value,
        UInt8 nargs
    );

    void InvokeNow(
        BytecodeStream *bs,
        const Value &value,
        UInt8 nargs
    );

    void Execute(BytecodeStream *bs);

private:
    bool HandleException(InstructionHandler *handler);
    void CreateStackTrace(ExecutionThread *thread, StackTrace *out);

    VMState m_state;

    uint32_t m_invoke_now_level;
};

} // namespace vm
} // namespace hyperion

#endif
