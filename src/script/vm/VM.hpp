#pragma once

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VMState.hpp>
#include <script/vm/StackTrace.hpp>

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

    VMState& GetState()
    {
        return m_state;
    }

    const VMState& GetState() const
    {
        return m_state;
    }

    GC* GetGC() const
    {
        return m_state.GetGC();
    }

    void Invoke(
        InstructionHandler* handler,
        Value&& value,
        uint8 nargs);

    void InvokeNow(
        BytecodeStream* bs,
        Value&& value,
        uint8 nargs);

    void Execute(BytecodeStream* bs);

private:
    bool HandleException(InstructionHandler* handler);
    void CreateStackTrace(Script_ExecutionThread* thread, StackTrace* out);

    APIInstance& m_apiInstance;
    VMState m_state;
};

} // namespace vm
} // namespace hyperion
