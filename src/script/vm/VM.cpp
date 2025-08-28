#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMTypeInfo.hpp>
#include <script/vm/GC.hpp>
#include <script/vm/Exception.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypMember.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/debug/Debug.hpp>
#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <script/Instructions.hpp>

#include <algorithm>
#include <cstdio>
#include <cinttypes>
#include <mutex>
#include <sstream>

#define HYP_NUMERIC_OPERATION(a, b, oper)                                         \
    do                                                                            \
    {                                                                             \
        switch (numericType)                                                      \
        {                                                                         \
        case NT_I8:                                                               \
            result.i = static_cast<int8>(a.i) oper static_cast<int8>(b.i);        \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            break;                                                                \
        case NT_I16:                                                              \
            result.i = static_cast<int16>(a.i) oper static_cast<int16>(b.i);      \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            break;                                                                \
        case NT_I32:                                                              \
            result.i = static_cast<int32>(a.i) oper static_cast<int32>(b.i);      \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            break;                                                                \
        case NT_I64:                                                              \
            result.i = a.i oper b.i;                                              \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            break;                                                                \
        case NT_U8:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint8>(a.i);                               \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = static_cast<uint8>(a.u);                               \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint8>(b.i);                        \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = static_cast<uint8>(b.u);                        \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            break;                                                                \
        case NT_U16:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint16>(a.i);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = static_cast<uint16>(a.u);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint16>(b.i);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = static_cast<uint16>(b.u);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            break;                                                                \
        case NT_U32:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint32>(a.i);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = static_cast<uint32>(a.u);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint32>(b.i);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = static_cast<uint32>(b.u);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            break;                                                                \
        case NT_U64:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint64>(a.i);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = a.u;                                                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint64>(b.i);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = b.u;                                            \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            break;                                                                \
        case NT_F32:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f = static_cast<float>(a.i);                               \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f = static_cast<float>(a.u);                               \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f = static_cast<float>(a.f);                               \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f oper## = static_cast<float>(b.i);                        \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f oper## = static_cast<float>(b.u);                        \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f oper## = static_cast<float>(b.f);                        \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            break;                                                                \
        case NT_F64:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f = static_cast<double>(a.i);                              \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f = static_cast<double>(a.u);                              \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f = a.f;                                                   \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f oper## = static_cast<double>(b.i);                       \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f oper## = static_cast<double>(b.u);                       \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f oper## = b.f;                                            \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            break;                                                                \
        default:                                                                  \
            Assert(false, "Invalid type, should not reach this state.");          \
            break;                                                                \
        }                                                                         \
    }                                                                             \
    while (0)

#define HYP_NUMERIC_OPERATION_BITWISE(a, b, oper)                            \
    do                                                                       \
    {                                                                        \
        switch (numericType)                                                 \
        {                                                                    \
        case NT_I8:                                                          \
            result.i = static_cast<int8>(a.i) oper static_cast<int8>(b.i);   \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;         \
            break;                                                           \
        case NT_I16:                                                         \
            result.i = static_cast<int16>(a.i) oper static_cast<int16>(b.i); \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;        \
            break;                                                           \
        case NT_I32:                                                         \
            result.i = static_cast<int32>(a.i) oper static_cast<int32>(b.i); \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;        \
            break;                                                           \
        case NT_I64:                                                         \
            result.i = a.i oper b.i;                                         \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;        \
            break;                                                           \
        case NT_U8:                                                          \
            if (a.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u = static_cast<uint8>(a.i);                          \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;   \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u = static_cast<uint8>(a.u);                          \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;   \
            }                                                                \
            if (b.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u oper## = static_cast<uint8>(b.i);                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;   \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u oper## = static_cast<uint8>(b.u);                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;   \
            }                                                                \
            break;                                                           \
        case NT_U16:                                                         \
            if (a.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u = static_cast<uint16>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;  \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u = static_cast<uint16>(a.u);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;  \
            }                                                                \
            if (b.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u oper## = static_cast<uint16>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;  \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u oper## = static_cast<uint16>(b.u);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;  \
            }                                                                \
            break;                                                           \
        case NT_U32:                                                         \
            if (a.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u = static_cast<uint32>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;  \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u = static_cast<uint32>(a.u);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;  \
            }                                                                \
            if (b.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u oper## = static_cast<uint32>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;  \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u oper## = static_cast<uint32>(b.u);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;  \
            }                                                                \
            break;                                                           \
        case NT_U64:                                                         \
            if (a.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u = static_cast<uint64>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;  \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u = a.u;                                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;  \
            }                                                                \
            if (b.flags & Number::FLAG_SIGNED)                               \
            {                                                                \
                result.u oper## = static_cast<uint64>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;  \
            }                                                                \
            else                                                             \
            {                                                                \
                result.u oper## = b.u;                                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;  \
            }                                                                \
            break;                                                           \
        default:                                                             \
            vm->ThrowException(thread, Exception::InvalidBitwiseArgument()); \
            break;                                                           \
        }                                                                    \
    }                                                                        \
    while (0)

namespace hyperion {

extern const char* LookupTypeName(TypeId typeId);

#pragma region ScriptApi

template <class T, typename = std::enable_if_t<!std::is_same_v<vm::Script_VMData, NormalizedType<T>> && !std::is_same_v<vm::Number, NormalizedType<T>> && !std::is_same_v<HypData, NormalizedType<T>>>>
static inline vm::Value ScriptApi_MakeValue(T&& data)
{
    return vm::Value(HypData(std::forward<T>(data)));
}

vm::Value ScriptApi_MakeValue(HypData&& data)
{
    return vm::Value(std::move(data));
}

vm::Value ScriptApi_MakeValue(const vm::Script_VMData& data)
{
    return vm::Value(data);
}

vm::Value ScriptApi_MakeValue(const vm::Number& number)
{
    return vm::Value(number);
}

/*! \brief Use for loading into registers - does not promote to tracked memory */
vm::Value ScriptApi_MakeRef(vm::Value& refValue)
{
    vm::Value* pValue = &refValue;

    vm::Script_VMData vmData;
    vmData.type = vm::Script_VMData::VALUE_REF;
    vmData.valueRef = refValue.Deref();

    Assert(vmData.valueRef != nullptr);

    return vm::Value(vmData);
}

/*! \brief Use for loading into registers - promotes to tracked memory if needed */
vm::Value ScriptApi_MakeRef(vm::Value& refValue, vm::GC* gc, bool promoteToTrackedMemory)
{
    if (promoteToTrackedMemory)
    {
        Assert(gc != nullptr);
        Assert(!refValue.IsRef());

        if (refValue.GetGCIndex() != vm::INVALID_GC_INDEX)
        {
            // already in tracked memory, make a reference to this value
            return ScriptApi_MakeRef(refValue);
        }

        const TypeId originalTypeId = refValue.GetHypData()->GetTypeId();

        vm::Value* pValue = gc->MoveToTrackedMemory(std::move(refValue));
        Assert(pValue != nullptr);
        Assert(pValue->GetGCIndex() != vm::INVALID_GC_INDEX);

        // update original reference to point to tracked memory
        refValue = ScriptApi_MakeRef(*pValue);

        Assert(refValue.IsRef());
        Assert(refValue.Deref() == pValue);
        Assert(refValue.Deref()->GetHypData()->GetTypeId() == originalTypeId);
    }

    return ScriptApi_MakeRef(refValue);
}

// Performs a shallow copy of the value. Numeric and primitive types are copied as-is.
vm::Value ScriptApi_ShallowCopy(vm::Value& refValue, vm::GC* gc)
{
    vm::Value* pValue = refValue.Deref();

    if (pValue->IsRef())
    {
        // already a reference, make a new reference to the same value
        return ScriptApi_MakeRef(*refValue.Deref());
    }

    if (pValue->GetGCIndex() != vm::INVALID_GC_INDEX)
    {
        // already in tracked memory
        return ScriptApi_MakeRef(refValue);
    }

    const HypData& hypData = *pValue->GetHypData();

    // 'Any' is used internally by HypData for object that is heap-allocated,
    // and we use reference semantics for it rather than copying.
    const bool shouldDoCopy = !hypData.Is<Any>();

    if (shouldDoCopy)
    {
        HypData newHypData;

        Visit(hypData.value, [&newHypData](const auto& val)
            {
                if constexpr (!std::is_base_of_v<AnyBase, NormalizedType<decltype(val)>>)
                {
                    newHypData.value.Set<NormalizedType<decltype(val)>>(val);
                }
                else
                {
                    // should never be hit; all of the types in the condition would be copy-constructible
                    HYP_UNREACHABLE();
                }
            });

        newHypData.serializeFunction = hypData.serializeFunction;

        return vm::Value(std::move(newHypData));
    }

    // reference type - promote to tracked memory
    return ScriptApi_MakeRef(refValue, gc, true);
}

// set return value on main thread
static void ScriptApi_SetReturnValue(void* ctx, vm::Value&& value)
{
    Assert(ctx != nullptr);

    vm::VM* vm = static_cast<vm::VM*>(ctx);

    vm::Script_ExecutionThread* mainThread = vm->GetMainThread();
    Assert(mainThread != nullptr);

    mainThread->GetRegisters()[0].AssignValue(std::move(value), false);
}

static void ScriptApi_ThrowException(void* ctx, const vm::Exception& exception)
{
    Assert(ctx != nullptr);
    vm::VM* vm = static_cast<vm::VM*>(ctx);
    vm->ThrowException(vm->GetMainThread(), exception);
}

#pragma endregion ScriptApi

namespace vm {

#pragma region Script_StaticMemory

const uint16 Script_StaticMemory::staticSize = 65535;

Script_StaticMemory::Script_StaticMemory()
    : m_data(new Value[staticSize])
{
}

Script_StaticMemory::~Script_StaticMemory()
{
    delete[] m_data;
}

#pragma endregion Script_StaticMemory

#pragma region Script_StackMemory

Script_StackMemory::Script_StackMemory()
    : m_sp(0)
{
}

Script_StackMemory::~Script_StackMemory()
{
    Purge();
}

void Script_StackMemory::Purge()
{
    for (SizeType i = m_sp; i > 0; i--)
    {
        m_data[i - 1].Destruct();
    }

    m_sp = 0;
}

#pragma endregion Script_StackMemory

#pragma region InstructionHandler

class InstructionHandler
{
public:
    VM* vm;
    Script_ExecutionThread* thread;
    BytecodeStream* bs;

    InstructionHandler(
        VM* vm,
        Script_ExecutionThread* thread,
        BytecodeStream* bs)
        : vm(vm),
          thread(thread),
          bs(bs)
    {
    }

    HYP_FORCE_INLINE void LoadI32(BCRegister reg, int32 i32)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(i32), false);
    }

    HYP_FORCE_INLINE void LoadI64(BCRegister reg, int64 i64)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(i64), false);
    }

    HYP_FORCE_INLINE void LoadU32(BCRegister reg, uint32 u32)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(u32), false);
    }

    HYP_FORCE_INLINE void LoadU64(BCRegister reg, uint64 u64)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(u64), false);
    }

    HYP_FORCE_INLINE void LoadF32(BCRegister reg, float f32)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(f32), false);
    }

    HYP_FORCE_INLINE void LoadF64(BCRegister reg, double f64)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(f64), false);
    }

    HYP_FORCE_INLINE void LoadOffset(BCRegister reg, uint16 offset)
    {
        Script_StackMemory& stackMemory = thread->m_stack;

        Assert(
            offset <= stackMemory.GetStackPointer(),
            "Stack offset out of bounds (%u)",
            offset);

        // read value from stack at (sp - offset)
        // into the the register
        thread->m_regs[reg].AssignValue(ScriptApi_ShallowCopy(stackMemory[stackMemory.GetStackPointer() - offset], vm->GetGC()), false);
    }

    HYP_FORCE_INLINE void LoadIndex(BCRegister reg, uint16 index)
    {
        Script_StackMemory& stackMemory = thread->m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stackMemory.GetStackPointer());

        // read value from stack at the index into the the register
        thread->m_regs[reg].AssignValue(ScriptApi_ShallowCopy(stackMemory[index], vm->GetGC()), false);
    }

    HYP_FORCE_INLINE void LoadStatic(BCRegister reg, uint16 index)
    {
        // read value from static memory
        // at the index into the the register
        Value& value = vm->m_staticMemory[index];

        thread->m_regs[reg].AssignValue(ScriptApi_ShallowCopy(value, vm->GetGC()), false);
    }

    HYP_FORCE_INLINE void LoadConstantString(BCRegister reg, uint32 len, const char* str)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(VMString(str)), false);
    }

    HYP_FORCE_INLINE void LoadAddr(BCRegister reg, Script_FunctionAddress addr)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::ADDRESS;
        vmData.addr = addr;

        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(vmData), false);
    }

    HYP_FORCE_INLINE void LoadFunc(BCRegister reg, Script_FunctionAddress addr, uint8 nargs, uint8 flags)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::FUNCTION;
        vmData.func.m_addr = addr;
        vmData.func.m_nargs = nargs;
        vmData.func.m_flags = flags;

        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(vmData), false);
    }

    HYP_FORCE_INLINE void LoadArrayIdx(BCRegister dstReg, BCRegister srcReg, BCRegister indexReg)
    {
        Value& src = *thread->m_regs[srcReg].Deref();

        Number key;

        if (!thread->m_regs[indexReg].GetSignedOrUnsigned(&key))
        {
            vm->ThrowException(thread, Exception("Array index must be an integral type"));

            return;
        }

        if (VMArray* array = src.GetArray())
        {
            if (key.flags & Number::FLAG_SIGNED)
            {
                if (key.i < 0)
                {
                    // wrap around (python style)
                    key.u = SizeType(array->GetSize() - SizeType(-key.i));
                    if (key.u >= array->GetSize())
                    {
                        vm->ThrowException(thread, Exception::OutOfBoundsException(key.u, array->GetSize()));

                        return;
                    }
                }

                if (SizeType(key.i) >= array->GetSize())
                {
                    vm->ThrowException(thread, Exception::OutOfBoundsException(SizeType(key.i), array->GetSize()));
                    return;
                }

                thread->m_regs[dstReg].AssignValue(ScriptApi_ShallowCopy(array->AtIndex(key.i), vm->GetGC()), false);
            }
            else if (key.flags & Number::FLAG_UNSIGNED)
            {
                if (key.u >= array->GetSize())
                {
                    vm->ThrowException(thread, Exception::OutOfBoundsException(key.u, array->GetSize()));

                    return;
                }

                thread->m_regs[dstReg].AssignValue(ScriptApi_ShallowCopy(array->AtIndex(key.u), vm->GetGC()), false);
            }

            return;
        }

        // throw an exception
        vm->ThrowException(thread, Exception("Not an array!"));
    }

    HYP_FORCE_INLINE void LoadOffsetRef(BCRegister reg, uint16 offset)
    {
        // load reference to stack value at (sp - offset) into the register
        Value newRef = ScriptApi_MakeRef(thread->m_stack[thread->m_stack.GetStackPointer() - offset], vm->GetGC(), true);
        Assert(newRef.IsRef());

        thread->m_regs[reg].AssignValue(std::move(newRef), false);
    }

    HYP_FORCE_INLINE void LoadIndexRef(BCRegister reg, uint16 index)
    {
        Script_StackMemory& stackMemory = vm->GetMainThread()->m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stackMemory.GetStackPointer());

        Value newRef = ScriptApi_MakeRef(stackMemory[index], vm->GetGC(), true);
        Assert(newRef.IsRef());

        // load reference to stack value at index into the register
        thread->m_regs[reg].AssignValue(std::move(newRef), false);
    }

    HYP_FORCE_INLINE void LoadRef(BCRegister dstReg, BCRegister srcReg)
    {
        Value newRef = ScriptApi_MakeRef(thread->m_regs[srcReg], vm->GetGC(), true);
        Assert(newRef.IsRef());

        // load reference to value in srcReg into dstReg
        thread->m_regs[dstReg].AssignValue(std::move(newRef), false);
    }

    HYP_FORCE_INLINE void LoadDeref(BCRegister dstReg, BCRegister srcReg)
    {
        Value& src = *thread->m_regs[srcReg].Deref();
        thread->m_regs[dstReg].AssignValue(ScriptApi_ShallowCopy(src, vm->GetGC()), false);
    }

    HYP_FORCE_INLINE void LoadNull(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(Value(), false);
    }

    HYP_FORCE_INLINE void LoadTrue(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(true), false);
    }

    HYP_FORCE_INLINE void LoadFalse(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(ScriptApi_MakeValue(false), false);
    }

    HYP_FORCE_INLINE void LoadClass(BCRegister reg, uint64 nameHash)
    {
        Name name = Name(NameID(nameHash));
        const HypClass* hypClass = HypClassRegistry::GetInstance().GetClass(name);
        if (!hypClass)
        {
            vm->ThrowException(thread, Exception::ClassNotFoundException(name.LookupString()));

            return;
        }

        Value classValue = ScriptApi_MakeValue(HypData(AnyRef(const_cast<HypClass*>(hypClass))));

        thread->m_regs[reg].AssignValue(std::move(classValue), false);
    }

    HYP_FORCE_INLINE void MovOffset(uint16 offset, BCRegister reg)
    {
        // copy value from register to stack value at (sp - offset)
        thread->m_stack[thread->m_stack.GetStackPointer() - offset].AssignValue(std::move(thread->m_regs[reg]), true);
    }

    HYP_FORCE_INLINE void MovIndex(uint16 index, BCRegister reg)
    {
        // copy value from register to stack value at index
        vm->GetMainThread()->m_stack[index].AssignValue(std::move(thread->m_regs[reg]), true);
    }

    HYP_FORCE_INLINE void MovStatic(uint16 index, BCRegister reg)
    {
        Assert(index < vm->m_staticMemory.staticSize);

        Value& value = vm->m_staticMemory[index];
        value.AssignValue(std::move(thread->m_regs[reg]), false);
    }

    HYP_FORCE_INLINE void MovArrayIdx(BCRegister dstReg, uint32 index, BCRegister srcReg)
    {
        Value& src = *thread->m_regs[dstReg].Deref();

        VMArray* array = src.GetArray();

        if (array != nullptr)
        {
            if (index >= array->GetSize())
            {
                vm->ThrowException(thread, Exception::OutOfBoundsException(SizeType(index), array->GetSize()));

                return;
            }

            array->AtIndex(index).AssignValue(ScriptApi_ShallowCopy(thread->m_regs[srcReg], vm->GetGC()), false);
            array->AtIndex(index).Mark();
            return;
        }

        // not an Array
        vm->ThrowException(thread, Exception("Not an array!"));
    }

    HYP_FORCE_INLINE void MovArrayIdxReg(BCRegister dstReg, BCRegister indexReg, BCRegister srcReg)
    {
        Value& src = *thread->m_regs[dstReg].Deref();

        VMArray* array = src.GetArray();

        Number index;
        Value& indexRegisterValue = thread->m_regs[indexReg];

        if (!indexRegisterValue.GetSignedOrUnsigned(&index))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidArgsException("integer"));

            return;
        }

        if (array != nullptr)
        {
            if (index.flags & Number::FLAG_SIGNED)
            {
                int64 indexValue = index.i;

                if (indexValue < 0)
                {
                    // wrap around (python style)
                    SizeType uIndexValue = SizeType(array->GetSize() - SizeType(-indexValue));

                    if (uIndexValue >= array->GetSize())
                    {
                        vm->ThrowException(thread, Exception::OutOfBoundsException(uIndexValue, array->GetSize()));

                        return;
                    }
                }

                if (SizeType(indexValue) >= array->GetSize())
                {
                    vm->ThrowException(thread, Exception::OutOfBoundsException(SizeType(indexValue), array->GetSize()));

                    return;
                }

                array->AtIndex(indexValue).AssignValue(ScriptApi_ShallowCopy(thread->m_regs[srcReg], vm->GetGC()), false);
                array->AtIndex(indexValue).Mark();
            }
            else
            { // unsigned
                const uint64 indexValue = index.u;

                if (SizeType(indexValue) >= array->GetSize())
                {
                    vm->ThrowException(thread, Exception::OutOfBoundsException(indexValue, array->GetSize()));

                    return;
                }

                array->AtIndex(indexValue).AssignValue(ScriptApi_ShallowCopy(thread->m_regs[srcReg], vm->GetGC()), false);
                array->AtIndex(indexValue).Mark();
            }

            return;
        }

        vm->ThrowException(thread, Exception("Not an array!"));
    }

    HYP_FORCE_INLINE void Mov(BCRegister dstReg, BCRegister srcReg)
    {
        thread->m_regs[dstReg] = std::move(thread->m_regs[srcReg]);
    }

    HYP_FORCE_INLINE void CheckHasMember(BCRegister dstReg, BCRegister srcReg, uint64 hash)
    {
        Value& src = *thread->m_regs[srcReg].Deref();
        Value& result = thread->m_regs[dstReg];

        if (const AnyHandle& object = src.GetObject())
        {
            const HypClass* hypClass = object.ptr->InstanceClass();
            Assert(hypClass != nullptr);

            HypField* field = hypClass->GetField(WeakName(NameID(hash)));

            if (field)
            {
                result.AssignValue(ScriptApi_MakeValue(true), false);
            }
            else
            {
                result.AssignValue(ScriptApi_MakeValue(false), false);
            }

            return;
        }

        result.AssignValue(ScriptApi_MakeValue(false), false);
    }

    HYP_FORCE_INLINE void SetMember(BCRegister dstReg, uint64 hash, BCRegister srcReg)
    {
        Value* pValue = thread->m_regs[dstReg].Deref();

        const AnyHandle& object = pValue->GetObject();
        if (!object)
        {
            vm->ThrowException(thread, Exception::InvalidMemberAccessException(pValue));

            return;
        }

        const HypClass* hypClass = object.ptr->InstanceClass();
        Assert(hypClass != nullptr);

        HypField* field = hypClass->GetField(WeakName(NameID(hash)));

        if (!field)
        {
            vm->ThrowException(thread, Exception::MemberNotFoundException(hash));
            return;
        }

        field->Set(*pValue->GetHypData(), *thread->m_regs[srcReg].GetHypData());
    }

    HYP_FORCE_INLINE void GetMember(BCRegister dstReg, BCRegister srcReg, uint64 hash)
    {
        Value& src = *thread->m_regs[srcReg].Deref();

        const AnyHandle& object = src.GetObject();

        if (!object)
        {
            vm->ThrowException(thread, Exception::InvalidMemberAccessException(&src));

            return;
        }

        const HypClass* hypClass = object.ptr->InstanceClass();
        Assert(hypClass != nullptr);

        IHypMember* member = hypClass->GetMember(WeakName(NameID(hash)));
        if (!member)
        {
            vm->ThrowException(thread, Exception::MemberNotFoundException(hash));

            return;
        }

        if (member->GetMemberType() == HypMemberType::TYPE_FIELD)
        {
            HypField* field = static_cast<HypField*>(member);

            thread->m_regs[dstReg].AssignValue(ScriptApi_MakeValue(field->Get(*src.GetHypData())), false);
        }
        else if (member->GetMemberType() == HypMemberType::TYPE_METHOD)
        {
            HypMethod* method = static_cast<HypMethod*>(member);

            Script_VMData vmData;

            if (method->IsScriptFunction())
            {
                Assert(method->GetParameters().Size() <= UINT8_MAX);

                vmData.type = Script_VMData::FUNCTION;
                vmData.func.m_addr = method->GetScriptAddress();
                vmData.func.m_nargs = (uint8)method->GetParameters().Size();
                vmData.func.m_flags = (uint8)method->GetFlags();
            }
            else
            {
                vmData.type = Script_VMData::NATIVE_FUNCTION;
                vmData.nativeFunc = method;
            }

            thread->m_regs[dstReg].AssignValue(ScriptApi_MakeValue(vmData), false);
        }
        else
        {
            vm->ThrowException(thread, Exception("Member is not a field or method"));
        }
    }

    HYP_FORCE_INLINE void Push(BCRegister reg)
    {
        DebugLog(
            LogType::Debug,
            "Pushing register %u to stack (sp = %u), value = %s\n",
            reg,
            thread->m_stack.GetStackPointer(),
            thread->m_regs[reg].ToString().GetData());
        // Move value from register to top of stack
        thread->m_stack.Push(ScriptApi_ShallowCopy(thread->m_regs[reg], vm->GetGC()));
    }

    HYP_FORCE_INLINE void Pop()
    {
        thread->m_stack.Pop();
    }

    HYP_FORCE_INLINE void PushArray(BCRegister dstReg, BCRegister srcReg)
    {
        Value& dst = *thread->m_regs[dstReg].Deref();

        VMArray* array = dst.GetArray();
        if (!array)
        {
            vm->ThrowException(
                thread,
                Exception("Not an Array"));
            return;
        }

        array->Push(ScriptApi_ShallowCopy(thread->m_regs[srcReg], vm->GetGC()));
        array->AtIndex(array->GetSize() - 1).Mark();
    }

    HYP_FORCE_INLINE void AddSp(uint16 n)
    {
        thread->m_stack.m_sp += n;
    }

    HYP_FORCE_INLINE void SubSp(uint16 n)
    {
        thread->m_stack.m_sp -= n;
    }

    HYP_FORCE_INLINE void Jmp(Script_FunctionAddress addr)
    {
        bs->Seek((uint32)addr);
    }

    HYP_FORCE_INLINE void Je(Script_FunctionAddress addr)
    {
        if (thread->m_regs.m_flags & EQUAL)
        {
            bs->Seek((uint32)addr);
        }
    }

    HYP_FORCE_INLINE void Jne(Script_FunctionAddress addr)
    {
        if (!(thread->m_regs.m_flags & EQUAL))
        {
            bs->Seek((uint32)addr);
        }
    }

    HYP_FORCE_INLINE void Jg(Script_FunctionAddress addr)
    {
        if (thread->m_regs.m_flags & GREATER)
        {
            bs->Seek((uint32)addr);
        }
    }

    HYP_FORCE_INLINE void Jge(Script_FunctionAddress addr)
    {
        if (thread->m_regs.m_flags & (GREATER | EQUAL))
        {
            bs->Seek((uint32)addr);
        }
    }

    HYP_FORCE_INLINE void Call(BCRegister reg, uint8_t nargs)
    {
        vm->Invoke(this, std::move(thread->m_regs[reg]), nargs);
    }

    HYP_FORCE_INLINE void Ret()
    {
        // get top of stack (should be the address before jumping)
        Value& top = thread->GetStack().Top();

        Script_VMData* vmData = top.GetVMData();
        Assert(vmData != nullptr);
        Assert(vmData->type == Script_VMData::FUNCTION_CALL);

        auto& callInfo = vmData->call;

        // leave function and return to previous position
        bs->Seek((uint32)callInfo.returnAddress);

        // increase stack size by the amount required by the call
        thread->GetStack().m_sp += callInfo.varargsPush - 1;
        // NOTE: the -1 is because we will be popping the FUNCTION_CALL
        // object from the stack anyway...

        // decrease function depth
        thread->m_funcDepth--;
    }

    HYP_FORCE_INLINE void BeginTry(Script_FunctionAddress addr)
    {
        ++thread->m_exceptionState.m_tryCounter;

        // increase stack size to store data about this try block
        Script_VMData vmData;
        vmData.type = Script_VMData::TRY_CATCH_INFO;
        vmData.tryCatchInfo.catchAddress = addr;

        // store the info
        thread->m_stack.Push(ScriptApi_MakeValue(vmData));
    }

    HYP_FORCE_INLINE void EndTry()
    {
        // pop the try catch info from the stack
        Value& top = thread->m_stack.Top();

        Script_VMData* vmData = top.GetVMData();
        Assert(vmData != nullptr);
        Assert(vmData->type == Script_VMData::TRY_CATCH_INFO);

        Assert(thread->m_exceptionState.m_tryCounter != 0);

        // pop try catch info
        thread->m_stack.Pop();
        --thread->m_exceptionState.m_tryCounter;
    }

    HYP_FORCE_INLINE void New(BCRegister dst, BCRegister src) // come back to this
    {
        // read value from register
        Value& classValue = *thread->m_regs[src].Deref();

        const HypClass* hypClass = classValue.ToRef().TryGet<HypClass>();
        Assert(hypClass != nullptr);

        HypData hypData;
        if (!hypClass->CreateInstance(hypData))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "NEW",
                    "Could not create instance of type",
                    hypClass->GetName().LookupString()));

            return;
        }

        thread->m_regs[dst].AssignValue(ScriptApi_MakeValue(std::move(hypData)), false);
    }

    HYP_FORCE_INLINE void NewArray(BCRegister dst, uint32 size)
    {
        // assign register value to the allocated object
        thread->m_regs[dst] = ScriptApi_MakeValue(VMArray(size));
    }

    HYP_FORCE_INLINE void BeginClass(BCRegister reg)
    {
        // Read class name length and name
        uint16 nameLen;
        bs->Read(&nameLen);

        char* nameStr = (char*)std::malloc(nameLen + 1);
        nameStr[nameLen] = '\0';
        bs->Read(nameStr, nameLen);

        // Read type id
        TypeId::ValueType typeIdValue;
        bs->Read(&typeIdValue);

        // Create a new class with the given name
        Name className = CreateNameFromDynamicString(nameStr);
        std::free(nameStr);

        Array<HypMember> members;
        bool hitEnd = false;

        // Read members until we hit END_CLASS
        while (!bs->Eof() && !hitEnd)
        {
            ubyte nextByte;
            bs->Read(&nextByte);

            if (nextByte == Instructions::END_CLASS)
            {
                hitEnd = true;
                break;
            }

            HypMemberType memberType = HypMemberType(nextByte);
            static_assert(sizeof(HypMemberType) == 1, "HypMemberType must be 1 byte");

            // Read member count
            uint16 memberCount;
            bs->Read(&memberCount);

            // Read each member
            for (uint16 i = 0; i < memberCount; i++)
            {
                // Read member name
                uint16 memberNameLen;
                bs->Read(&memberNameLen);

                char* memberNameStr = (char*)std::malloc(memberNameLen + 1);
                memberNameStr[memberNameLen] = '\0';
                bs->Read(memberNameStr, memberNameLen);

                // Read attributes
                uint16 numAttrs;
                bs->Read(&numAttrs);

                Array<HypClassAttribute> attrs;
                attrs.Reserve(numAttrs);

                // Skip attributes for now - read and discard them
                for (uint16 attrIdx = 0; attrIdx < numAttrs; attrIdx++)
                {
                    HypClassAttribute attr;

                    // Read attribute name
                    uint16 attrNameLen;
                    bs->Read(&attrNameLen);

                    char* attrNameStr = (char*)std::malloc(attrNameLen + 1);
                    attrNameStr[attrNameLen] = '\0';
                    bs->Read(attrNameStr, attrNameLen);

                    attr.name = CreateNameFromDynamicString(attrNameStr);
                    std::free(attrNameStr);

                    // Read attribute type
                    uint8 attrType;
                    bs->Read(&attrType);

                    // Skip attribute value based on type
                    switch (HypClassAttributeType(attrType))
                    {
                    case HypClassAttributeType::STRING:
                    {
                        uint32 strLen;
                        bs->Read(&strLen);

                        Array<char> strData;
                        strData.Resize(strLen + 1);
                        strData[strLen] = '\0';

                        bs->Read(strData.Data(), strLen);

                        attr.value = HypClassAttributeValue(String(strData.Begin(), strData.End()));

                        break;
                    }
                    case HypClassAttributeType::INT:
                    {
                        int32 iValue;
                        bs->Read(&iValue);

                        attr.value = HypClassAttributeValue(iValue);

                        break;
                    }
                    case HypClassAttributeType::BOOLEAN:
                    {
                        ubyte bValue;
                        bs->Read(&bValue);

                        attr.value = HypClassAttributeValue(bValue != 0);

                        break;
                    }
                    default:
                        break;
                    }

                    attrs.PushBack(std::move(attr));
                }

                // Read member type id
                TypeId::ValueType memberTypeIdValue;
                bs->Read(&memberTypeIdValue);

                switch (memberType)
                {
                case HypMemberType::TYPE_FIELD:
                {
                    // field writes target typeid, offset, size
                    TypeId::ValueType targetTypeIdValue;
                    bs->Read(&targetTypeIdValue);

                    uint32 offset;
                    bs->Read(&offset);

                    uint32 size;
                    bs->Read(&size);

                    // Create field
                    members.PushBack(HypMember(HypField(
                        CreateNameFromDynamicString(memberNameStr),
                        TypeId(memberTypeIdValue),
                        TypeId(targetTypeIdValue),
                        offset,
                        size,
                        attrs.ToSpan())));

                    break;
                }
                case HypMemberType::TYPE_METHOD:
                {
                    TypeId::ValueType targetTypeIdValue;
                    bs->Read(&targetTypeIdValue);

                    uint8 flags;
                    bs->Read(&flags);

                    uint16 stackOffset;
                    bs->Read(&stackOffset);

                    // load function info from stack address
                    Assert(stackOffset <= thread->GetStack().GetStackPointer(), "Stack offset out of bounds!");
                    Value& funcValue = thread->GetStack()[thread->GetStack().GetStackPointer() - stackOffset];

                    Script_VMData* funcVmData = funcValue.GetVMData();
                    Assert(funcVmData != nullptr);
                    Assert(funcVmData->type == Script_VMData::FUNCTION);

                    Script_FunctionAddress functionAddress = funcVmData->func.m_addr;
                    Assert(functionAddress != INVALID_FUNCTION_ADDRESS);

                    HypMethod method(
                        CreateNameFromDynamicString(memberNameStr),
                        TypeId(memberTypeIdValue),
                        TypeId(targetTypeIdValue),
                        functionAddress,
                        HypMethodFlags(flags),
                        attrs.ToSpan());

                    uint8 nargs = funcVmData->func.m_nargs;

                    if (flags & (uint8)HypMethodFlags::VARIADIC)
                    {
                        AssertDebug(nargs > 0);

                        --nargs;
                    }

                    method.GetParameters().Reserve(nargs);

                    for (uint8 j = 0; j < nargs; j++)
                    {
                        method.GetParameters().PushBack(HypMethodParameter { TypeId::ForType<HypData>() });
                    }

                    members.PushBack(HypMember(std::move(method)));

                    break;
                }
                default:
                    HYP_NOT_IMPLEMENTED();
                    break;
                }

                std::free(memberNameStr);
            }
        }

        Assert(hitEnd);

        // Read parent class register
        Value& parentClassValue = thread->m_regs[reg];

        const HypClass* parentClass = nullptr;

        if (parentClassValue.IsValid())
        {
            parentClass = parentClassValue.ToRef().TryGet<const HypClass>();
            Assert(parentClass != nullptr);
        }

        /// @TODO: Delete on GC
        DynamicHypClassInstance* newClass = new DynamicHypClassInstance(
            TypeId(typeIdValue),
            className,
            parentClass,
            Span<const HypClassAttribute>(), // @TODO: Class attributes
            HypClassFlags::NONE,
            members.ToSpan());

        HypClassRegistry::GetInstance().RegisterClass(newClass->GetTypeId(), newClass);

        Value classValue = ScriptApi_MakeValue(AnyRef(static_cast<HypClass*>(newClass)));
        thread->m_regs[reg].AssignValue(std::move(classValue), false);
    }

    HYP_FORCE_INLINE void Cmp(BCRegister lhsReg, BCRegister rhsReg)
    {
        // dropout early for comparing something against itself
        if (lhsReg == rhsReg)
        {
            thread->m_regs.m_flags = EQUAL;
            return;
        }

        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetSignedOrUnsigned(&a) && rhs->GetSignedOrUnsigned(&b))
        {
            if ((a.flags & Number::FLAG_SIGNED) && (b.flags & Number::FLAG_SIGNED))
            {
                thread->m_regs.m_flags = (a.i == b.i)
                    ? EQUAL
                    : ((a.i > b.i)
                              ? GREATER
                              : NONE);
            }
            else if ((a.flags & Number::FLAG_SIGNED) && (b.flags & Number::FLAG_UNSIGNED))
            {
                thread->m_regs.m_flags = (a.i == b.u)
                    ? EQUAL
                    : ((a.i > b.u)
                              ? GREATER
                              : NONE);
            }
            else if ((a.flags & Number::FLAG_UNSIGNED) && (b.flags & Number::FLAG_SIGNED))
            {
                thread->m_regs.m_flags = (a.u == b.i)
                    ? EQUAL
                    : ((a.u > b.i)
                              ? GREATER
                              : NONE);
            }
            else if ((a.flags & Number::FLAG_UNSIGNED) && (b.flags & Number::FLAG_UNSIGNED))
            {
                thread->m_regs.m_flags = (a.u == b.u)
                    ? EQUAL
                    : ((a.u > b.u)
                              ? GREATER
                              : NONE);
            }
        }
        else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f))
        {
            thread->m_regs.m_flags = (a.f == b.f)
                ? EQUAL
                : ((a.f > b.f)
                          ? GREATER
                          : NONE);
        }
        else
        {
            bool lhsBool;
            bool rhsBool;

            if (lhs->GetBoolean(&lhsBool) && rhs->GetBoolean(&rhsBool))
            {
                thread->m_regs.m_flags = (lhsBool == rhsBool)
                    ? EQUAL
                    : ((lhsBool > rhsBool)
                              ? GREATER
                              : NONE);
            }
            else
            {
                const int res = Value::CompareAsPointers(lhs, rhs);

                if (res != -1)
                {
                    thread->m_regs.m_flags = res;
                }
                else
                {
                    vm->ThrowException(
                        thread,
                        Exception::InvalidComparisonException(
                            lhs->GetTypeString(),
                            rhs->GetTypeString()));
                }
            }
        }
    }

    HYP_FORCE_INLINE void CmpZ(BCRegister reg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[reg].Deref();

        Number num;

        if (lhs->GetSignedOrUnsigned(&num))
        {
            thread->m_regs.m_flags = ((num.flags & Number::FLAG_SIGNED) ? !num.i : !num.u) ? EQUAL : NONE;
        }
        else if (lhs->GetFloatingPoint(&num.f))
        {
            thread->m_regs.m_flags = !num.f ? EQUAL : NONE;
        }
        else
        {
            bool boolValue;
            if (lhs->GetBoolean(&boolValue))
            {
                thread->m_regs.m_flags = !boolValue ? EQUAL : NONE;
            }
            else
            {
                void* ptrValue = lhs->ToRef().GetPointer();

                thread->m_regs.m_flags = !ptrValue ? EQUAL : NONE;
            }
        }
    }

    HYP_FORCE_INLINE void Add(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION(a, b, +);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "ADD",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Sub(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION(a, b, -);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SUB",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Mul(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION(a, b, *);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "MUL",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Div(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                vm->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                vm->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }

            HYP_NUMERIC_OPERATION(a, b, /);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "DIV",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Mod(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            // custom handling for mod to allow floats to work
            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                vm->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                vm->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }

            if (a.flags & Number::FLAG_FLOATING_POINT || b.flags & Number::FLAG_FLOATING_POINT)
            {
                // at least one operand is a float, do floating point mod
                result.f = std::fmod(a.f, b.f);
                result.flags = Number::FLAG_FLOATING_POINT;
            }
            else if (a.flags & Number::FLAG_SIGNED && b.flags & Number::FLAG_SIGNED)
            {
                result.i = a.i % b.i;
                result.flags = Number::FLAG_SIGNED;
            }
            else if (a.flags & Number::FLAG_SIGNED && b.flags & Number::FLAG_UNSIGNED)
            {
                result.i = a.i % static_cast<int64>(b.u);
                result.flags = Number::FLAG_SIGNED;
            }
            else if (a.flags & Number::FLAG_UNSIGNED && b.flags & Number::FLAG_SIGNED)
            {
                result.u = a.u % static_cast<uint64>(b.i);
                result.flags = Number::FLAG_UNSIGNED;
            }
            else if (a.flags & Number::FLAG_UNSIGNED && b.flags & Number::FLAG_UNSIGNED)
            {
                result.u = a.u % b.u;
                result.flags = Number::FLAG_UNSIGNED;
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "MOD",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void And(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, &);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "AND",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Or(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, |);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "OR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Xor(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, ^);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "XOR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Shl(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, <<);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SHL",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Shr(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = thread->m_regs[lhsReg].Deref();
        Value* rhs = thread->m_regs[rhsReg].Deref();

        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, >>);
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SHR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Not(BCRegister reg)
    {
        // load value from register
        Value& value = *thread->m_regs[reg].Deref();

        Number num;

        Number result;
        result.flags = num.flags;

        // we only allow bitwise NOT on integers
        if (value.GetNumber(&num) && (num.flags & (Number::FLAG_SIGNED | Number::FLAG_UNSIGNED)))
        {
            if (num.flags & Number::FLAG_SIGNED)
            {
                if (num.flags & Number::FLAG_8_BIT)
                {
                    result.i = ~static_cast<int8>(num.i);
                }
                else if (num.flags & Number::FLAG_16_BIT)
                {
                    result.i = ~static_cast<int16>(num.i);
                }
                else if (num.flags & Number::FLAG_32_BIT)
                {
                    result.i = ~static_cast<int32>(num.i);
                }
                else
                {
                    result.i = ~num.i;
                }
            }
            else if (num.flags & Number::FLAG_UNSIGNED)
            {
                if (num.flags & Number::FLAG_8_BIT)
                {
                    result.u = ~static_cast<uint8>(num.u);
                }
                else if (num.flags & Number::FLAG_16_BIT)
                {
                    result.u = ~static_cast<uint16>(num.u);
                }
                else if (num.flags & Number::FLAG_32_BIT)
                {
                    result.u = ~static_cast<uint32>(num.u);
                }
                else
                {
                    result.u = ~num.u;
                }
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }
        else
        {
            vm->ThrowException(
                thread,
                Exception::InvalidBitwiseArgument());
            return;
        }

        thread->m_regs[reg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void Throw(BCRegister reg)
    {
        // load value from register
        Value* value = thread->m_regs[reg].Deref();

        // @TODO Allow throwing the arugment

        vm->ThrowException(
            thread,
            Exception("User exception"));
    }

    HYP_FORCE_INLINE void ExportSymbol(BCRegister reg, uint64 hash)
    {
        if (!vm->GetExportedSymbols().Store(hash, ScriptApi_ShallowCopy(*thread->m_regs[reg].Deref(), vm->GetGC())).second)
        {
            vm->ThrowException(
                thread,
                Exception::DuplicateExportException());
        }
    }

    HYP_FORCE_INLINE void Neg(BCRegister reg)
    {
        // load value from register
        Value& value = *thread->m_regs[reg].Deref();

        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "NEG",
                    value.GetTypeString()));

            return;
        }

        Number result;
        result.flags = num.flags;

        if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = -num.i;
        }
        else if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint64>(-static_cast<int64>(num.u));
            result.flags = Number::FLAG_SIGNED | (num.flags & (Number::FLAG_8_BIT | Number::FLAG_16_BIT | Number::FLAG_32_BIT));
        }
        else
        {
            result.f = -num.f;
        }

        thread->m_regs[reg] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastU8(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = *thread->m_regs[src].Deref();

        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U8",
                    value.GetTypeString()));

            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint8>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint8>(num.i);
        }
        else
        {
            result.u = static_cast<uint8>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastU16(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = *thread->m_regs[src].Deref();

        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U16",
                    value.GetTypeString()));

            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint16>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint16>(num.i);
        }
        else
        {
            result.u = static_cast<uint16>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastU32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U32",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint32>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint32>(num.i);
        }
        else
        {
            result.u = static_cast<uint32>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastU64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U64",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = num.u;
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint64>(num.i);
        }
        else
        {
            result.u = static_cast<uint64>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastI8(BCRegister dst, BCRegister src)
    {
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I8",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int8>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = static_cast<int8>(num.i);
        }
        else
        {
            result.i = static_cast<int8>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastI16(BCRegister dst, BCRegister src)
    {
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I16",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int16>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = static_cast<int16>(num.i);
        }
        else
        {
            result.i = static_cast<int16>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastI32(BCRegister dst, BCRegister src)
    {
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I32",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int32>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = static_cast<int32>(num.i);
        }
        else
        {
            result.i = static_cast<int32>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastI64(BCRegister dst, BCRegister src)
    {
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I64",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int64>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = num.i;
        }
        else
        {
            result.i = static_cast<int64>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastF32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_F32",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.f = static_cast<float>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.f = static_cast<float>(num.i);
        }
        else
        {
            result.f = static_cast<float>(num.f);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastF64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = *thread->m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_F64",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_FLOATING_POINT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.f = static_cast<double>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.f = static_cast<double>(num.i);
        }
        else
        {
            result.f = num.f;
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastBool(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = *thread->m_regs[src].Deref();

        // use same logic as CmpZ to determine truthiness
        bool result = false;
        Number num;

        if (value.GetSignedOrUnsigned(&num))
        {
            result = (num.flags & Number::FLAG_SIGNED) ? (num.i != 0) : (num.u != 0);
        }
        else if (value.GetFloatingPoint(&num.f))
        {
            result = (num.f != 0.0);
        }
        else if (value.GetBoolean(&result))
        {
            // already a bool, do nothing
        }
        else
        {
            void* ptrValue = value.ToRef().GetPointer();
            result = (ptrValue != nullptr);
        }

        thread->m_regs[dst] = ScriptApi_MakeValue(result);
    }

    HYP_FORCE_INLINE void CastDynamic(BCRegister dst, BCRegister src) // come back to this
    {
        HYP_NOT_IMPLEMENTED();
#if 0
        // load the VMObject from dst
        Value& value = *thread->m_regs[src].Deref();

        // Ensure it is a VMObject
        VMObject* classObjectPtr = value.GetObject();

        if (!classObjectPtr)
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    value.GetTypeString()));

            return;
        }

        // load the target from src
        Value& target = *thread->m_regs[src].Deref();

        // Ensure it is a VMObject
        VMObject* targetObjectPtr = target.GetObject();

        if (!targetObjectPtr)
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    target.GetTypeString()));

            return;
        }

        bool isInstance = false;

        // Check if the target is an instance of the type
        Value* pBase = nullptr;

        if (const Value& targetClassValue = targetObjectPtr->GetClassPointer(); targetClassValue.IsValid())
        {
            constexpr uint32 maxDepth = 1024;
            uint32 depth = 0;

            VMObject* targetClassObject = targetClassValue.GetObject();

            while (targetClassObject != nullptr && depth < maxDepth)
            {
                isInstance = (*targetClassObject == *classObjectPtr);

                if (isInstance)
                {
                    break;
                }

                if (!(targetClassObject->LookupBasePointer(&pBase) && (targetClassObject = pBase->GetObject())))
                {
                    break;
                }

                depth++;
            }

            if (depth == maxDepth)
            {
                vm->ThrowException(
                    thread,
                    Exception::InvalidOperationException(
                        "CAST_DYNAMIC",
                        "Max depth reached"));

                return;
            }
        }

        // If it is not an instance, throw an exception
        if (!isInstance)
        {
            vm->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    "Not an instance"));

            return;
        }

        Assert(pBase != nullptr);

        // Set the destination register to be the target
        thread->m_regs[dst].AssignValue(ScriptApi_ShallowCopy(*pBase, vm->GetGC()), false);
#endif
    }
};

HYP_FORCE_INLINE static void HandleInstruction(
    InstructionHandler& handler,
    BytecodeStream* bs,
    ubyte code)
{

    switch (code)
    {
    case LOAD_I32:
    {
        BCRegister reg;
        bs->Read(&reg);
        int32_t i32;
        bs->Read(&i32);

        handler.LoadI32(
            reg,
            i32);

        break;
    }
    case LOAD_I64:
    {
        BCRegister reg;
        bs->Read(&reg);
        int64_t i64;
        bs->Read(&i64);

        handler.LoadI64(
            reg,
            i64);

        break;
    }
    case LOAD_U32:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint32 u32;
        bs->Read(&u32);

        handler.LoadU32(
            reg,
            u32);

        break;
    }
    case LOAD_U64:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint64_t u64;
        bs->Read(&u64);

        handler.LoadU64(
            reg,
            u64);

        break;
    }
    case LOAD_F32:
    {
        BCRegister reg;
        bs->Read(&reg);
        float f32;
        bs->Read(&f32);

        handler.LoadF32(
            reg,
            f32);

        break;
    }
    case LOAD_F64:
    {
        BCRegister reg;
        bs->Read(&reg);
        double f64;
        bs->Read(&f64);

        handler.LoadF64(
            reg,
            f64);

        break;
    }
    case LOAD_OFFSET:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 offset;
        bs->Read(&offset);

        handler.LoadOffset(
            reg,
            offset);

        break;
    }
    case LOAD_INDEX:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 index;
        bs->Read(&index);

        handler.LoadIndex(
            reg,
            index);

        break;
    }
    case LOAD_STATIC:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 index;
        bs->Read(&index);

        handler.LoadStatic(
            reg,
            index);

        break;
    }
    case LOAD_STRING:
    {
        BCRegister reg;
        bs->Read(&reg);
        // get string length
        uint32 len;
        bs->Read(&len);

        // read string based on length
        char* str = new char[len + 1];
        str[len] = '\0';
        bs->Read(str, len);

        handler.LoadConstantString(
            reg,
            len,
            str);

        delete[] str;

        break;
    }
    case LOAD_ADDR:
    {
        BCRegister reg;
        bs->Read(&reg);

        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler.LoadAddr(reg, addr);

        break;
    }
    case LOAD_FUNC:
    {
        BCRegister reg;
        bs->Read(&reg);

        Script_FunctionAddress addr;
        bs->Read(&addr);

        uint8 nargs;
        bs->Read(&nargs);

        uint8 flags;
        bs->Read(&flags);

        handler.LoadFunc(reg, addr, nargs, flags);

        break;
    }
    case LOAD_ARRAYIDX:
    {
        BCRegister dstReg;
        bs->Read(&dstReg);

        BCRegister srcReg;
        bs->Read(&srcReg);

        BCRegister indexReg;
        bs->Read(&indexReg);

        handler.LoadArrayIdx(
            dstReg,
            srcReg,
            indexReg);

        break;
    }
    case LOAD_OFFSET_REF:
    {
        BCRegister reg;
        bs->Read(&reg);

        uint16 offset;
        bs->Read(&offset);

        handler.LoadOffsetRef(reg, offset);

        break;
    }
    case LOAD_INDEX_REF:
    {
        BCRegister reg;
        bs->Read(&reg);

        uint16 index;
        bs->Read(&index);

        handler.LoadIndexRef(reg, index);

        break;
    }
    case REF:
    {
        BCRegister dstReg;
        BCRegister srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler.LoadRef(dstReg, srcReg);

        break;
    }
    case DEREF:
    {
        BCRegister dstReg;
        BCRegister srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler.LoadDeref(dstReg, srcReg);

        break;
    }
    case LOAD_NULL:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.LoadNull(reg);

        break;
    }
    case LOAD_TRUE:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.LoadTrue(reg);

        break;
    }
    case LOAD_FALSE:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.LoadFalse(reg);

        break;
    }
    case LOAD_CLASS:
    {
        BCRegister reg;
        bs->Read(&reg);

        uint64 nameHash;
        bs->Read(&nameHash);

        handler.LoadClass(reg, nameHash);

        break;
    }
    case MOV_OFFSET:
    {
        uint16 offset;
        bs->Read(&offset);

        BCRegister reg;
        bs->Read(&reg);

        handler.MovOffset(offset, reg);

        break;
    }
    case MOV_INDEX:
    {
        uint16 index;
        bs->Read(&index);
        BCRegister reg;
        bs->Read(&reg);

        handler.MovIndex(index, reg);

        break;
    }
    case MOV_STATIC:
    {
        uint16 index;
        bs->Read(&index);

        BCRegister reg;
        bs->Read(&reg);

        handler.MovStatic(index, reg);

        break;
    }
    case MOV_ARRAYIDX:
    {
        BCRegister dst;
        bs->Read(&dst);

        uint32 index;
        bs->Read(&index);

        BCRegister src;
        bs->Read(&src);

        handler.MovArrayIdx(dst, index, src);

        break;
    }
    case MOV_ARRAYIDX_REG:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister indexReg;
        bs->Read(&indexReg);

        BCRegister src;
        bs->Read(&src);

        handler.MovArrayIdxReg(dst, indexReg, src);

        break;
    }
    case MOV:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.Mov(dst, src);

        break;
    }
    case CHECK_HAS_MEMBER:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        uint64 hash;
        bs->Read(&hash);

        handler.CheckHasMember(dst, src, hash);

        break;
    }
    case SET_MEMBER:
    {
        BCRegister dst;
        bs->Read(&dst);

        uint64 hash;
        bs->Read(&hash);

        BCRegister src;
        bs->Read(&src);

        handler.SetMember(dst, hash, src);

        break;
    }
    case GET_MEMBER:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        uint64 hash;
        bs->Read(&hash);

        handler.GetMember(dst, src, hash);

        break;
    }
    case PUSH:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Push(reg);

        break;
    }
    case POP:
    {
        handler.Pop();

        break;
    }
    case PUSH_ARRAY:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.PushArray(dst, src);

        break;
    }
    case ADD_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler.AddSp(val);

        break;
    }
    case SUB_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler.SubSp(val);

        break;
    }
    case JMP:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler.Jmp(addr);

        break;
    }
    case JE:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler.Je(addr);

        break;
    }
    case JNE:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler.Jne(addr);

        break;
    }
    case JG:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler.Jg(addr);

        break;
    }
    case JGE:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler.Jge(addr);

        break;
    }
    case CALL:
    {
        BCRegister reg;
        bs->Read(&reg);

        uint8 nargs;
        bs->Read(&nargs);

        handler.Call(reg, nargs);

        break;
    }
    case RET:
    {
        handler.Ret();

        break;
    }
    case BEGIN_TRY:
    {
        Script_FunctionAddress catchAddress;
        bs->Read(&catchAddress);

        handler.BeginTry(catchAddress);

        break;
    }
    case END_TRY:
    {
        handler.EndTry();

        break;
    }
    case NEW:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.New(dst, src);

        break;
    }
    case NEW_ARRAY:
    {
        BCRegister dst;
        bs->Read(&dst);

        uint32 size;
        bs->Read(&size);

        handler.NewArray(dst, size);

        break;
    }
    case CMP:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        handler.Cmp(lhsReg, rhsReg);

        break;
    }
    case BEGIN_CLASS:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.BeginClass(reg);

        break;
    }
    case CMPZ:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.CmpZ(reg);

        break;
    }
    case ADD:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Add(lhsReg, rhsReg, dstReg);

        break;
    }
    case SUB:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Sub(lhsReg, rhsReg, dstReg);

        break;
    }
    case MUL:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Mul(lhsReg, rhsReg, dstReg);

        break;
    }
    case DIV:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Div(lhsReg, rhsReg, dstReg);

        break;
    }
    case MOD:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Mod(lhsReg, rhsReg, dstReg);

        break;
    }
    case AND:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.And(lhsReg, rhsReg, dstReg);

        break;
    }
    case OR:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Or(lhsReg, rhsReg, dstReg);

        break;
    }
    case XOR:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Xor(lhsReg, rhsReg, dstReg);

        break;
    }
    case SHL:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Shl(lhsReg, rhsReg, dstReg);

        break;
    }
    case SHR:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Shr(lhsReg, rhsReg, dstReg);

        break;
    }
    case NEG:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Neg(reg);

        break;
    }
    case NOT:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Not(reg);

        break;
    }
    case THROW:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Throw(reg);

        break;
    }
    case TRACEMAP:
    {
        uint32 len;
        bs->Read(&len);

        uint32 stringmapCount;
        bs->Read(&stringmapCount);

        Tracemap::StringmapEntry* stringmap = nullptr;

        if (stringmapCount != 0)
        {
            stringmap = new Tracemap::StringmapEntry[stringmapCount];

            for (uint32 i = 0; i < stringmapCount; i++)
            {
                bs->Read(&stringmap[i].entryType);
                bs->ReadZeroTerminatedString(stringmap[i].data);
            }
        }

        uint32 linemapCount;
        bs->Read(&linemapCount);

        Tracemap::LinemapEntry* linemap = nullptr;

        if (linemapCount != 0)
        {
            linemap = new Tracemap::LinemapEntry[linemapCount];
            bs->Read(linemap, sizeof(Tracemap::LinemapEntry) * linemapCount);
        }

        handler.vm->m_tracemap.Set(stringmap, linemap);

        break;
    }
    case REM:
    {
        uint32 len;
        bs->Read(&len);
        // just skip comment
        bs->Skip(len);

        break;
    }
    case EXPORT:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint64 hash;
        bs->Read(&hash);

        handler.ExportSymbol(reg, hash);

        break;
    }
    case CAST_U8:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastU8(dst, src);

        break;
    }
    case CAST_U16:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastU16(dst, src);

        break;
    }
    case CAST_U32:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastU32(dst, src);

        break;
    }
    case CAST_U64:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastU64(dst, src);

        break;
    }
    case CAST_I8:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastI8(dst, src);

        break;
    }
    case CAST_I16:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastI16(dst, src);

        break;
    }
    case CAST_I32:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastI32(dst, src);

        break;
    }
    case CAST_I64:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastI64(dst, src);

        break;
    }
    case CAST_F32:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastF32(dst, src);

        break;
    }
    case CAST_F64:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastF64(dst, src);

        break;
    }
    case CAST_BOOL:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastBool(dst, src);

        break;
    }
    case CAST_DYNAMIC:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler.CastDynamic(dst, src);

        break;
    }
    default:
    {
        int64 lastPos = int64(bs->Position()) - sizeof(ubyte);
        HYP_FAIL("unknown instruction '{}' referenced at location {}", code, lastPos);
        // seek to end of bytecode stream
        bs->Seek(bs->Size());

        return;
    }
    }
}

#pragma endregion InstructionHandler

#pragma region VM

VM::VM(APIInstance& apiInstance)
    : m_apiInstance(apiInstance),
      m_unhandledException(nullptr)
{
    m_executionThread = new Script_ExecutionThread();
    m_gc = new GC();
}

VM::~VM()
{
    delete m_unhandledException;
    delete m_gc;
    delete m_executionThread;
}

void VM::ThrowException(Script_ExecutionThread* thread, const Exception& exception)
{
    ++thread->m_exceptionState.m_exceptionDepth;

    if (thread->m_exceptionState.m_tryCounter == 0)
    {
        // exception cannot be handled, no try block found
        if (thread->m_id == 0)
        {
            DebugLog(LogType::Error, "unhandled exception in main thread: %s", exception.ToString());
        }
        else
        {
            DebugLog(LogType::Error, "unhandled exception in thread %d: %s", thread->m_id, exception.ToString());
        }

        m_unhandledException = new Exception(exception);
    }
}

void VM::Invoke(InstructionHandler* handler, Value&& value, uint8 nargs)
{
    static const HashCode::ValueType invokeHash = HashCode::GetHashCode("$invoke").Value();

    Script_ExecutionThread* thread = handler->thread;
    BytecodeStream* bs = handler->bs;

    Assert(thread != nullptr);
    Assert(bs != nullptr);

    Value& deref = *value.Deref();

    if (deref.IsFunction())
    {
        if (deref.IsNativeFunction())
        {
            HypData** argsHypData = (HypData**)StackAlloc((nargs > 0 ? nargs : 1) * sizeof(HypData*));

            int64 i = static_cast<int64>(thread->m_stack.GetStackPointer()) - 1;
            for (int j = nargs - 1; j >= 0 && i >= 0; i--, j--)
            {
                argsHypData[j] = thread->m_stack[i].GetHypData();
            }

            // @TODO: Implement
            // disable auto gc so no collections happen during a native function
            //            enableAutoGc = false;

            // call the native function
            Script_VMData* vmData = deref.GetVMData();
            Assert(vmData != nullptr && vmData->nativeFunc != nullptr);

            HypData resultHypData = vmData->nativeFunc->Invoke(Span<HypData*>(argsHypData, nargs));

            // set register 0 to the result
            thread->GetRegisters()[0].AssignValue(ScriptApi_MakeValue(std::move(resultHypData)), false);

            // re-enable auto gc
            //            enableAutoGc = ENABLE_GC;

            return;
        }
        else if (const AnyHandle& object = deref.GetObject()) // functor object
        {
            HYP_NOT_IMPLEMENTED(); // come back to this
#if 0
            // Lookup $invoke member
            const HypClass* hypClass = object.ptr->InstanceClass();
            Assert(hypClass != nullptr);

            IHypMember* member = hypClass->GetMember(WeakName("$invoke"));

            if (member != nullptr)
            {
                const int64 sp = int64(thread->m_stack.GetStackPointer());
                const int64 argsStart = sp - nargs;

                if (nargs > 0)
                {
                    // shift over by 1 -- and insert 'self' to start of args
                    // make a copy of last item to not overwrite it
                    thread->m_stack.Push(std::move(thread->m_stack[sp - 1]));

                    for (SizeType i = argsStart; i < sp - 1; i++)
                    {
                        thread->m_stack[i + 1].AssignValue(std::move(thread->m_stack[i]), false);
                    }

                    // set 'self' object to start of args
                    thread->m_stack[argsStart].AssignValue(ScriptApi_ShallowCopy(deref, GetGC()), false);
                }
                else
                {
                    thread->m_stack.Push(ScriptApi_ShallowCopy(deref, GetGC()));
                }

                Invoke(handler, ScriptApi_ShallowCopy(member->value, GetGC()), nargs + 1);

                Value& top = thread->m_stack.Top(); // shouldn't need to deref - should directly hold stack frame

                Script_VMData* topVmData = top.GetVMData();
                Assert(topVmData != nullptr && topVmData->type == Script_VMData::FUNCTION_CALL);

                // bookkeeping to remove the closure object
                // normally, arguments are popped after the call is returned,
                // rather than within the body
                topVmData->call.varargsPush--;

                return;
            }
#endif
        }
        // non-native function here
        Script_VMData* vmData = deref.GetVMData();
        Assert(vmData != nullptr && vmData->type == Script_VMData::FUNCTION);

        if ((vmData->func.m_flags & (uint8)HypMethodFlags::VARIADIC) && nargs < vmData->func.m_nargs - 1)
        {
            // if variadic, make sure the arg count is /at least/ what is required
            ThrowException(thread, Exception::InvalidArgsException(vmData->func.m_nargs, nargs, true));
        }
        else if (!(vmData->func.m_flags & (uint8)HypMethodFlags::VARIADIC) && vmData->func.m_nargs != nargs)
        {
            ThrowException(thread, Exception::InvalidArgsException(vmData->func.m_nargs, nargs));
        }
        else
        {
            Script_VMData previousAddr;
            previousAddr.type = Script_VMData::FUNCTION_CALL;
            previousAddr.call.varargsPush = 0;
            previousAddr.call.returnAddress = (Script_FunctionAddress)bs->Position();

            if (vmData->func.m_flags & (uint8)HypMethodFlags::VARIADIC)
            {
                // for each argument that is over the expected size, we must pop it from
                // the stack and add it to a new array.
                int varargsAmt = nargs - vmData->func.m_nargs + 1;
                if (varargsAmt < 0)
                {
                    varargsAmt = 0;
                }

                // set varargsPush value so we know how to get back to the stack size before.
                previousAddr.call.varargsPush = varargsAmt - 1;

                // create VMArray object to hold variadic args
                VMArray arr(varargsAmt);

                for (int i = varargsAmt - 1; i >= 0; i--)
                {
                    // push to array
                    arr.AtIndex(i, std::move(thread->GetStack().Top()));
                    thread->GetStack().Pop();
                }

                // push the array to the stack
                thread->GetStack().Push(ScriptApi_MakeValue(std::move(arr)));
            }

            // push the address
            thread->GetStack().Push(ScriptApi_MakeValue(previousAddr));

            // seek to the new address
            bs->Seek((uint32)vmData->func.m_addr);

            // increase function depth
            thread->m_funcDepth++;
        }

        return;
    }

    char buffer[256];
    std::snprintf(
        buffer,
        HYP_ARRAY_SIZE(buffer),
        "cannot invoke type '%s' as a function",
        value.GetTypeString());

    ThrowException(thread, Exception(buffer));
}

void VM::InvokeNow(BytecodeStream* bs, Value&& value, uint8 nargs)
{
    Script_ExecutionThread* thread = GetMainThread();

    const SizeType positionBefore = bs->Position();
    const uint32 originalFunctionDepth = thread->m_funcDepth;
    const SizeType stackSizeBefore = thread->GetStack().GetStackPointer();

    InstructionHandler handler(this, thread, bs);

    Value* deref = value.Deref();
    Assert(deref != nullptr);

    Script_VMData* pVmData = deref->GetVMData();
    Assert(pVmData != nullptr);
    Assert(pVmData->type == Script_VMData::FUNCTION || pVmData->type == Script_VMData::NATIVE_FUNCTION);

    Script_VMData vmData = *pVmData;

    Invoke(&handler, std::move(value), nargs);

    if (vmData.type == Script_VMData::FUNCTION)
    { // don't do this for native function calls
        ubyte code;

        while (!bs->Eof())
        {
            bs->Read(&code);

            HandleInstruction(handler, bs, code);

            if (handler.thread->GetExceptionState().HasExceptionOccurred())
            {
                if (!HandleException(&handler))
                {
                    thread->m_exceptionState.m_exceptionDepth = 0;

                    Assert(thread->GetStack().GetStackPointer() >= stackSizeBefore);
                    thread->GetStack().Pop(thread->GetStack().GetStackPointer() - stackSizeBefore);

                    break;
                }
            }

            if (code == RET)
            {
                if (thread->m_funcDepth == originalFunctionDepth)
                {
                    break;
                }
            }
        }
    }

    bs->SetPosition(positionBefore);
}

void VM::CreateStackTrace(Script_ExecutionThread* thread, StackTrace* out)
{
    const SizeType maxStackTraceSize = std::size(out->callAddresses);

    for (int& callAddress : out->callAddresses)
    {
        callAddress = -1;
    }

    SizeType numRecordedCallAddresses = 0;

    for (SizeType sp = thread->m_stack.GetStackPointer(); sp != 0; sp--)
    {
        if (numRecordedCallAddresses >= maxStackTraceSize)
        {
            break;
        }

        const Value& top = thread->m_stack[sp - 1];

        const Script_VMData* topVmData = top.GetVMData();

        if (topVmData && topVmData->type == Script_VMData::FUNCTION_CALL)
        {
            out->callAddresses[numRecordedCallAddresses++] = int(topVmData->call.returnAddress);
        }
    }
}

bool VM::HandleException(InstructionHandler* handler)
{
    Script_ExecutionThread* thread = handler->thread;
    BytecodeStream* bs = handler->bs;

    if (thread->m_exceptionState.m_tryCounter != 0)
    {
        // handle exception
        --thread->m_exceptionState.m_tryCounter;

        Assert(thread->m_exceptionState.m_exceptionDepth != 0);
        --thread->m_exceptionState.m_exceptionDepth;

        Value* top = &thread->m_stack.Top();
        Script_VMData* topVmData = top->GetVMData();

        while (topVmData && topVmData->type != Script_VMData::TRY_CATCH_INFO)
        {
            thread->m_stack.Pop();

            top = &thread->m_stack.Top();
            topVmData = top->GetVMData();
        }

        // top should be exception data
        Assert(topVmData && topVmData->type != Script_VMData::TRY_CATCH_INFO);

        // jump to the catch block
        bs->Seek((uint32)topVmData->tryCatchInfo.catchAddress);

        // pop exception data from stack
        thread->m_stack.Pop();

        return true;
    }
    else
    {
        StackTrace stackTrace;
        CreateStackTrace(thread, &stackTrace);

        std::cout << "stackTrace = \n";

        for (auto callAddress : stackTrace.callAddresses)
        {
            if (callAddress == -1)
            {
                break;
            }

            std::cout << "\t" << std::hex << callAddress << "\n";
        }

        std::cout << "=====\n";

        // TODO: Seek outside function, if calling from outside?
        // so we can keep calling
    }

    return false;
}

void VM::Execute(BytecodeStream* bs)
{
    Assert(bs != nullptr);

    InstructionHandler handler(this, GetMainThread(), bs);

    ubyte code;

    while (!bs->Eof())
    {
        bs->Read(&code);

        HandleInstruction(handler, bs, code);

        if (handler.thread->GetExceptionState().HasExceptionOccurred())
        {
            HandleException(&handler);

            if (m_unhandledException)
            {
                DebugLog(LogType::Error, "Unhandled exception in VM, stopping execution...\n");

                break;
            }
        }
    }
}

#pragma endregion VM

} // namespace vm
} // namespace hyperion
