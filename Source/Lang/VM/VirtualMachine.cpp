#include <Lang/VM/VirtualMachine.hpp>
#include <Lang/VM/Value.hpp>
#include <Lang/VM/String.hpp>
#include <Lang/VM/Map.hpp>
#include <Lang/VM/GarbageCollector.hpp>
#include <Lang/VM/Exception.hpp>
#include <Lang/VM/ScriptMemory.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/MemberVariant.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Method.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/IO/BufferedByteReader.hpp>

#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Debug/Debug.hpp>
#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Lang/Instructions.hpp>
#include <Lang/Compiler/Dis/DecompilationUnit.hpp>

#include <iostream>

#if defined(HYP_DEBUG_MODE) && !defined(HYP_SCRIPT_NOOPT)
// #define HYP_SCRIPT_NOOPT 1
#endif

#if defined(HYP_SCRIPT_NOOPT) && HYP_SCRIPT_NOOPT
#define SCRIPT_INLINE
#else
#define SCRIPT_INLINE HYP_FORCE_INLINE
#endif

#include <Lang/VM/Inl/Interpreter.inl>

namespace Hyperion {

using ScriptArray = Array<BoxedValue, ScriptAllocator>;

using RegisterIndex = uint8;

#pragma region ScriptApi

static const Name s_nameToString = NAME("ToString");

static const String s_nullString = "null";
static const String s_boolStrings[2] = { "false", "true" };

static constexpr TypeId s_typeIdI8 { CONSTEXPR_TYPE_ID(int8) };
static constexpr TypeId s_typeIdI16 { CONSTEXPR_TYPE_ID(int16) };
static constexpr TypeId s_typeIdI32 { CONSTEXPR_TYPE_ID(int32) };
static constexpr TypeId s_typeIdI64 { CONSTEXPR_TYPE_ID(int64) };
static constexpr TypeId s_typeIdU8 { CONSTEXPR_TYPE_ID(uint8) };
static constexpr TypeId s_typeIdU16 { CONSTEXPR_TYPE_ID(uint16) };
static constexpr TypeId s_typeIdU32 { CONSTEXPR_TYPE_ID(uint32) };
static constexpr TypeId s_typeIdU64 { CONSTEXPR_TYPE_ID(uint64) };
static constexpr TypeId s_typeIdF32 { CONSTEXPR_TYPE_ID(float32) };
static constexpr TypeId s_typeIdF64 { CONSTEXPR_TYPE_ID(float64) };
static constexpr TypeId s_typeIdBool { CONSTEXPR_TYPE_ID(bool) };
static constexpr TypeId s_typeIdString { CONSTEXPR_TYPE_ID(ScriptString) };

// clang-format off

static const Map<TypeId, String (*)(const void*)> s_builtinToStringFunctions = {
    { s_typeIdI8, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int8*>(p)); } },
    { s_typeIdI16, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int16*>(p)); } },
    { s_typeIdI32, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int32*>(p)); } },
    { s_typeIdI64, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int64*>(p)); } },
    { s_typeIdU8, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint8*>(p)); } },
    { s_typeIdU16, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint16*>(p)); } },
    { s_typeIdU32, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint32*>(p)); } },
    { s_typeIdU64, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint64*>(p)); } },
    { s_typeIdF32, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const float*>(p)); } },
    { s_typeIdF64, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const double*>(p)); } },
    { s_typeIdBool, [](const void* p) -> String { return s_boolStrings[*reinterpret_cast<const bool*>(p) ? 1 : 0]; } },
    { s_typeIdString, [](const void* p) -> String { return *reinterpret_cast<const ScriptString*>(p); } }
};

static const Map<TypeId, const char*> s_builtinTypeNames = {
    { s_typeIdI8, "int8" },
    { s_typeIdI16, "int16" },
    { s_typeIdI32, "int32" },
    { s_typeIdI64, "int64" },
    { s_typeIdU8, "uint8" },
    { s_typeIdU16, "uint16" },
    { s_typeIdU32, "uint32" },
    { s_typeIdU64, "uint64" },
    { s_typeIdF32, "float" },
    { s_typeIdF64, "double" },
    { s_typeIdBool, "bool" },
    { s_typeIdString, "string" }
};

// clang-format on

static inline ScriptObjectData* GetVMData(BoxedValue& value)
{
    return reinterpret_cast<ScriptObjectData*>(value.TryGet<BoxedValue::InlineData>().TryGet());
}

static inline const ScriptObjectData* GetVMData(const BoxedValue& value)
{
    return reinterpret_cast<const ScriptObjectData*>(value.TryGet<BoxedValue::InlineData>().TryGet());
}

template <class T, typename = std::enable_if_t<!std::is_same_v<ScriptObjectData, NormalizedType<T>> && !std::is_same_v<Number, NormalizedType<T>> && !std::is_same_v<BoxedValue, NormalizedType<T>>>>
static inline BoxedValue MakeValue(T&& value)
{
    return BoxedValue(std::forward<T>(value));
}

BoxedValue MakeValue(BoxedValue&& value)
{
    return BoxedValue(std::move(value));
}

BoxedValue MakeValue(const ScriptObjectData& value)
{
    static_assert(sizeof(ScriptObjectData) <= sizeof(BoxedValue::InlineData), "ScriptObjectData must fit inside BoxedValue::InlineData");
    static_assert(alignof(ScriptObjectData) <= alignof(BoxedValue::InlineData), "ScriptObjectData must have alignment less than or equal to BoxedValue::InlineData");

    BoxedValue::InlineData resultData {};
    Memory::Copy(&resultData, &value, sizeof(ScriptObjectData));

    return BoxedValue(resultData);
}

namespace {

using BoxedValueCtor = BoxedValue (*)(const Number& number);

template <typename T>
static BoxedValue ConstructBoxedValue_Float(const Number& number)
{
    return BoxedValue(T(number.f));
}

template <typename T>
static BoxedValue ConstructBoxedValue_Signed(const Number& number)
{
    return BoxedValue(T(number.i));
}

template <typename T>
static BoxedValue ConstructBoxedValue_Unsigned(const Number& number)
{
    return BoxedValue(T(number.u));
}

static constexpr uint32 NumericFlagsMask = 0x7Fu;
static constexpr size_t NumericTableSize = NumericFlagsMask + 1;

static constexpr FixedArray<BoxedValueCtor, NumericTableSize> BoxedValueCtorTable = []
{
    FixedArray<BoxedValueCtor, NumericTableSize> t {};

    t[Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT] = &ConstructBoxedValue_Float<float32>;
    t[Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT] = &ConstructBoxedValue_Float<float64>;

    t[Number::FLAG_SIGNED | Number::FLAG_8_BIT]  = &ConstructBoxedValue_Signed<int8>;
    t[Number::FLAG_SIGNED | Number::FLAG_16_BIT] = &ConstructBoxedValue_Signed<int16>;
    t[Number::FLAG_SIGNED | Number::FLAG_32_BIT] = &ConstructBoxedValue_Signed<int32>;
    t[Number::FLAG_SIGNED | Number::FLAG_64_BIT] = &ConstructBoxedValue_Signed<int64>;

    t[Number::FLAG_UNSIGNED | Number::FLAG_8_BIT]  = &ConstructBoxedValue_Unsigned<uint8>;
    t[Number::FLAG_UNSIGNED | Number::FLAG_16_BIT] = &ConstructBoxedValue_Unsigned<uint16>;
    t[Number::FLAG_UNSIGNED | Number::FLAG_32_BIT] = &ConstructBoxedValue_Unsigned<uint32>;
    t[Number::FLAG_UNSIGNED | Number::FLAG_64_BIT] = &ConstructBoxedValue_Unsigned<uint64>;

    return t;
}();

} // anonymous namespace

BoxedValue MakeValue(const Number& number)
{
    return BoxedValueCtorTable[number.flags](number);

#if 0
    ValueStorage<BoxedValue> resultStorage;
    BoxedValue* ptr = resultStorage.GetPointer();

    if (number.flags & Number::FLAG_FLOATING_POINT)
    {
        if (number.flags & Number::FLAG_32_BIT)
        {
            new (ptr) BoxedValue(float32(number.f));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (ptr) BoxedValue(number.f);
        }
    }
    else if (number.flags & Number::FLAG_SIGNED)
    {
        if (number.flags & Number::FLAG_8_BIT)
        {
            new (ptr) BoxedValue(int8(number.i));
        }
        else if (number.flags & Number::FLAG_16_BIT)
        {
            new (ptr) BoxedValue(int16(number.i));
        }
        else if (number.flags & Number::FLAG_32_BIT)
        {
            new (ptr) BoxedValue(int32(number.i));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (ptr) BoxedValue(number.i);
        }
    }
    else if (number.flags & Number::FLAG_UNSIGNED)
    {
        if (number.flags & Number::FLAG_8_BIT)
        {
            new (ptr) BoxedValue(uint8(number.u));
        }
        else if (number.flags & Number::FLAG_16_BIT)
        {
            new (ptr) BoxedValue(uint16(number.u));
        }
        else if (number.flags & Number::FLAG_32_BIT)
        {
            new (ptr) BoxedValue(uint32(number.u));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (ptr) BoxedValue(number.u);
        }
    }
    else
    {
        HYP_UNREACHABLE();
    }

    return reinterpret_cast<BoxedValue&&>(*ptr);
#endif
}

/*! \brief Use for loading into registers - does not promote to tracked memory so the lifetime of `refValue` must be managed by the caller */
BoxedValue MakeRef(BoxedValue* pValue)
{
    Assert(pValue != nullptr);

    ScriptObjectData data;
    data.type = ScriptObjectData::Type::Reference;
    data.valueRef = pValue;

    Assert(data.valueRef != nullptr);
    Assert(!IsGarbage(*data.valueRef), "Creating a reference to garbage value");

    return MakeValue(data);
}

BoxedValue MakeTrackedRef(BoxedValue* pValue, GarbageCollector* gc)
{
    Assert(gc != nullptr);
    Assert(pValue != nullptr);

    if (pValue->extData.gcIndex != INVALID_GC_INDEX)
    {
        // already in tracked memory, make a reference to this value
        return MakeRef(pValue);
    }

    gc->MoveToTrackedMemory(*pValue);

    return *pValue;
}

#define PASS_AS_REF(value) ((value).Is<Any>())

// Performs a shallow copy of the value. Numeric and primitive types are copied as-is.
BoxedValue ShallowCopy(BoxedValue& refValue, GarbageCollector* gc)
{
    if (IsRef(refValue))
    {
        return refValue; // already a reference, return as-is
    }

    if (refValue.extData.gcIndex != INVALID_GC_INDEX)
    {
        // Only create a reference for Any-backed types where deep copying
        // is expensive. Inline types (primitives, Name, Handle, etc.) are
        // always copied by value regardless of tracked memory status.
        if (PASS_AS_REF(refValue))
        {
            return MakeRef(&refValue);
        }
    }

    BoxedValue newValue;

    Visit(refValue.value, [&newValue](const auto& val)
        {
            newValue.value.Set<NormalizedType<decltype(val)>>(val);
        });

    return newValue;
}

bool ShouldValuePassByRef(const BoxedValue& value)
{
    return PASS_AS_REF(value);
}

static const char* GetTypeString(const TypeInfo& typeInfo)
{
    auto it = s_builtinTypeNames.Find(typeInfo.id);
    if (it != s_builtinTypeNames.End())
    {
        return it->second;
    }

    return typeInfo.name.LookupString();
}

const char* GetTypeString(const BoxedValue& value)
{
    if (!value.IsValid())
    {
        return "<Uninitialized Data>";
    }

    return GetTypeString(*value.GetTypeInfo());
}

bool StringifyData(const BoxedValue& value, ScriptString& outString, int maxDepth, int currDepth)
{
    if (currDepth >= maxDepth && maxDepth >= 0)
    {
        outString = ScriptString("...");
        return true;
    }

    constexpr size_t StringBufferSize = 256;

    char buf[StringBufferSize] = { 0 };

    if (!value.IsValid())
    {
        outString = ScriptString(s_nullString);

        return true;
    }

    auto formatIt = s_builtinToStringFunctions.Find(value.GetTypeId());
    if (formatIt != s_builtinToStringFunctions.End())
    {
        outString = ScriptString(formatIt->second(value.ToRef().GetPointer()));

        return true;
    }

    if (GenericArrayWrapper* arrayWrapper = value.TryGet<GenericArrayWrapper>().TryGet())
    {
        const size_t arraySize = arrayWrapper->Size();

        if (arrayWrapper->CanGetElementByIndex())
        {
            outString = "[";

            for (size_t i = 0; i < arraySize; i++)
            {
                if (i > 0)
                {
                    outString += ScriptString(", ");
                }

                AnyRef elementRef = arrayWrapper->GetElementAt(i);

                if (elementRef.HasValue())
                {
                    // get the existing reference for ScriptArray instances
                    if (elementRef.GetTypeId().Value() == BoxedValueTypeId)
                    {
                        outString += ToString(elementRef.Get<BoxedValue>());
                    }
                    // Otherwise, box up the reference and get the string from that
                    else
                    {
                        outString += ToString(BoxedValue(elementRef));
                    }
                }
                else
                {
                    outString += ScriptString("<Error accessing element>");
                }
            }

            outString += "]";
        }
        else
        {
            outString = ScriptString("Array" + HYP_FORMAT(" (size = {})", arraySize));
        }

        return true;
    }

#if 0
    if (const ScriptMap* pMap = value.TryGet<ScriptMap>().TryGet())
    {
        auto& map = pMap->GetMap();

        outString = "{";

        int i = 0;

        for (auto& kv : map)
        {
            if (i > 0)
            {
                outString += ScriptString(", ");
            }
            outString += ValueToString(*kv.first.key.GetHypData(), currDepth + 1);
            outString += " => ";
            outString += ValueToString(*kv.second.GetHypData(), currDepth + 1);
            i++;
        }

        return true;
    }
#endif

    if (const Class* cls = GetClass(value.GetTypeId()))
    {
        const Method* toStringMethod = cls->GetMethod(s_nameToString);

        if (toStringMethod != nullptr)
        {
            BoxedValue result = toStringMethod->Invoke(Span<BoxedValue> { const_cast<BoxedValue*>(&value), 1 });

            if (const ScriptString* str = result.TryGet<ScriptString>().TryGet())
            {
                outString = *str;

                return true;
            }

            if (const String* str = result.TryGet<String>().TryGet())
            {
                outString = ScriptString(*str);

                return true;
            }

            // not a string, try again recursively
            if (StringifyData(result, outString, maxDepth, currDepth + 1))
            {
                return true;
            }
        }

        constexpr const char* ObjectFormatString = "<%s @ %p>";

        int n = std::snprintf(
            buf,
            StringBufferSize,
            ObjectFormatString,
            cls->GetName().LookupString(),
            value.ToRef().GetPointer());

        // if the class name is too long, dynamically allocate a larger buffer
        if (n < 0)
        {
            outString = ScriptString("<Error formatting object>");

            return true;
        }

        if (size_t(n) >= StringBufferSize)
        {
            const size_t newBufSize = size_t(n) + 1;

            char* newBuf = (char*)ScriptAlloc(newBufSize);
            Assert(newBuf != nullptr);

            n = std::snprintf(
                newBuf,
                newBufSize,
                ObjectFormatString,
                cls->GetName().LookupString(),
                value.ToRef().GetPointer());

            if (n < 0 || size_t(n) >= newBufSize)
            {
                ScriptFree(newBuf);

                outString = ScriptString("<Error formatting object>");

                return true;
            }

            ScriptString result(newBuf);
            ScriptFree(newBuf);

            outString = result;

            return true;
        }

        outString = ScriptString(buf);

        return true;
    }

    return false;
}

String ValueToString(const BoxedValue& value, int currDepth)
{
    static constexpr int maxDepth = 3;

    ScriptString result("<error>");
    if (StringifyData(value, result, maxDepth, currDepth))
    {
        return result;
    }

    // internal data
    if (const ScriptObjectData* data = reinterpret_cast<const ScriptObjectData*>(value.TryGet<BoxedValue::InlineData>().TryGet()))
    {
        constexpr size_t bufSize = 256;
        char buf[bufSize] = { 0 };

        switch (data->type)
        {
        case ScriptObjectData::Type::Reference:
            return ScriptString("<Reference>");
        case ScriptObjectData::Type::ScriptFunction:
            return ScriptString("<Function>");
        case ScriptObjectData::Type::NativeFunction:
            return ScriptString("<Native Function>");
        case ScriptObjectData::Type::BytecodeAddress:
            std::snprintf(buf, bufSize, "<Function address @ %p>", (void*)data->func.m_addr);
            return ScriptString(buf);
        case ScriptObjectData::Type::StackFrame:
            return ScriptString("<Stack frame>");
        case ScriptObjectData::Type::ExceptionState:
            return ScriptString("<Try catch info>");
        case ScriptObjectData::Type::InvalidState:
            return ScriptString("<Invalid>");
        default:
            HYP_UNREACHABLE();
        }
    }

    return ScriptString(HYP_FORMAT("<{} @ {}>", GetTypeString(value), value.ToRef().GetPointer()));
}

#pragma endregion ScriptApi

#pragma region Script_RegisterMemory

Script_RegisterMemory::Script_RegisterMemory()
{
    for (uint8 i = 0; i < NumRegisters; i++)
    {
        new (values.GetPointer() + i) BoxedValue;
    }
}

Script_RegisterMemory::~Script_RegisterMemory()
{
    for (uint8 i = 0; i < NumRegisters; i++)
    {
        BoxedValue& value = values.GetPointer()[i];

        AssertDebug(!IsGarbage(value));

        value.~BoxedValue();
        value.extData.gcIndex = INVALID_GC_INDEX;
    }
}

#pragma endregion Script_RegisterMemory

#pragma region Script_StaticMemory

const uint16 Script_StaticMemory::staticSize = 2048;

Script_StaticMemory::Script_StaticMemory()
    : m_data((BoxedValue*)ScriptAlloc(staticSize * sizeof(BoxedValue), alignof(BoxedValue)))
{
    Memory::Fill(m_data, 0xFFu, staticSize * sizeof(BoxedValue));
}

Script_StaticMemory::~Script_StaticMemory()
{
    for (uint32 i = 0; i < staticSize; i++)
    {
        if (!IsGarbage(m_data[i]) && m_data[i].extData.isStaticInit)
        {
            m_data[i].~BoxedValue();
        }
    }

    ScriptFree(m_data);
}

#pragma endregion Script_StaticMemory

#pragma region Script_StackMemory

// 64 KiB stack size
static constexpr size_t StackSize = ((64 * 1024) + (sizeof(BoxedValue) - 1)) / sizeof(BoxedValue);

Script_StackMemory::Script_StackMemory()
    : m_data((BoxedValue*)ScriptAlloc(StackSize * sizeof(BoxedValue), alignof(BoxedValue))),
      m_sp(0)
{
    AssertDebug(m_data != nullptr);

    // 0xFFu == Garbage value bit pattern
    Memory::Fill(m_data, 0xFFu, StackSize * sizeof(BoxedValue));
}

Script_StackMemory::~Script_StackMemory()
{
    Purge();
    ScriptFree(m_data);
}

void Script_StackMemory::Purge()
{
    const size_t countBefore = m_sp;
    while (m_sp != 0)
    {
        BoxedValue& value = m_data[--m_sp];
        value.~BoxedValue();
    }

    Memory::Fill(m_data, 0xFFu, countBefore * sizeof(BoxedValue));
}

#pragma endregion Script_StackMemory

#pragma region InstructionHandler

class InstructionHandler
{
public:
    VirtualMachine* vm;
    ScriptInstance* instance;

    InstructionHandler(
        VirtualMachine* vm,
        ScriptInstance* instance)
        : vm(vm),
          instance(instance)
    {
        Assert(vm != nullptr);
        Assert(instance != nullptr);
    }

    SCRIPT_INLINE void OpLoadI32(RegisterIndex reg, int32 i32)
    {
        instance->thread.m_regs[reg] = MakeValue(i32);
    }

    SCRIPT_INLINE void OpLoadI64(RegisterIndex reg, int64 i64)
    {
        instance->thread.m_regs[reg] = MakeValue(i64);
    }

    SCRIPT_INLINE void OpLoadU32(RegisterIndex reg, uint32 u32)
    {
        instance->thread.m_regs[reg] = MakeValue(u32);
    }

    SCRIPT_INLINE void OpLoadU64(RegisterIndex reg, uint64 u64)
    {
        instance->thread.m_regs[reg] = MakeValue(u64);
    }

    SCRIPT_INLINE void OpLoadF32(RegisterIndex reg, float32 f32)
    {
        instance->thread.m_regs[reg] = MakeValue(f32);
    }

    SCRIPT_INLINE void OpLoadF64(RegisterIndex reg, float64 f64)
    {
        instance->thread.m_regs[reg] = MakeValue(f64);
    }

    SCRIPT_INLINE void OpLoadOffset(RegisterIndex reg, uint16 offset)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            offset <= stackMemory.GetStackPointer(),
            "Stack offset out of bounds (%u)",
            offset);

        BoxedValue& srcValue = stackMemory[stackMemory.GetStackPointer() - offset];

        // read value from stack at (sp - offset)
        // into the the register
        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? MakeRef(&srcValue)
            : ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadIndex(RegisterIndex reg, uint16 index)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stackMemory.GetStackPointer());

        BoxedValue& srcValue = stackMemory[index];

        // read value from stack at the index into the the register
        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? MakeRef(&srcValue)
            : ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadStatic(RegisterIndex reg, uint16 index)
    {
        // read value from static memory
        // at the index into the the register
        BoxedValue& srcValue = vm->m_staticMemory[index];

        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? MakeRef(&srcValue)
            : ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadConstantString(RegisterIndex reg, uint32 len, const char* str)
    {
        instance->thread.m_regs[reg] = MakeValue(str != nullptr ? ScriptString(str, str + len) : ScriptString());
    }

    SCRIPT_INLINE void OpLoadAddr(RegisterIndex reg, BytecodeAddress addr)
    {
        ScriptObjectData data;
        data.type = ScriptObjectData::Type::BytecodeAddress;
        data.addr = addr;

        instance->thread.m_regs[reg] = MakeValue(data);
    }

    SCRIPT_INLINE void OpLoadFunc(RegisterIndex reg, BytecodeAddress addr, uint8 nargs, uint8 flags)
    {
        ScriptObjectData data;
        data.type = ScriptObjectData::Type::ScriptFunction;
        data.func.m_addr = addr;
        data.func.m_nargs = nargs;
        data.func.m_flags = flags;

        instance->thread.m_regs[reg] = MakeValue(data);
    }

    SCRIPT_INLINE void OpLoadArrayIdx(RegisterIndex dstReg, RegisterIndex srcReg, RegisterIndex indexReg)
    {
        BoxedValue& src = *Deref(instance->thread.m_regs[srcReg]);

        Number key;

        if (!GetSignedOrUnsigned(instance->thread.m_regs[indexReg], &key))
        {
            vm->ThrowException(instance, Exception("Array index must be an integral type"));

            return;
        }

        if (GenericArrayWrapper* array = src.TryGet<GenericArrayWrapper>().TryGet())
        {
            if (key.flags & Number::FLAG_SIGNED)
            {
                if (key.i < 0)
                {
                    // wrap around (python style)
                    key.u = array->Size() - uint64(-key.i);
                }
                else
                {
                    key.u = uint64(key.i);
                }
            }

            if (key.u >= array->Size())
            {
                vm->ThrowException(instance, Exception::OutOfBoundsException(key.u, array->Size()));

                return;
            }

            AnyRef elementRef = array->GetElementAt(key.i);

            // For ScriptArray we want to get the BoxedValue and work off that rather than
            // double boxing it.
            if (elementRef.GetTypeId().Value() == BoxedValueTypeId)
            {
                BoxedValue& srcValue = elementRef.Get<BoxedValue>();

                instance->thread.m_regs[dstReg] = PASS_AS_REF(srcValue)
                    ? MakeRef(&srcValue)
                    : ShallowCopy(srcValue, vm->GetGC());
            }
            else
            {
                instance->thread.m_regs[dstReg] = BoxedValue(elementRef);
            }

            return;
        }

        // throw an exception
        vm->ThrowException(instance, Exception::InvalidOperationException("Indexing", GetTypeString(src)));
    }

    SCRIPT_INLINE void OpLoadOffsetRef(RegisterIndex reg, uint16 offset)
    {
        // load reference to stack value at (sp - offset) into the register
        BoxedValue newRef = MakeTrackedRef(Deref(instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - offset]), vm->GetGC());
        instance->thread.m_regs[reg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadIndexRef(RegisterIndex reg, uint16 index)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds ({} >= {})",
            index,
            stackMemory.GetStackPointer());

        BoxedValue newRef = MakeTrackedRef(Deref(stackMemory[index]), vm->GetGC());
        instance->thread.m_regs[reg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadRef(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        BoxedValue newRef = MakeTrackedRef(Deref(instance->thread.m_regs[srcReg]), vm->GetGC());
        instance->thread.m_regs[dstReg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadDeref(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        BoxedValue& src = *Deref(instance->thread.m_regs[srcReg]); // double deref to get the actual value
        BoxedValue result = ShallowCopy(*Deref(src), vm->GetGC());

        // ShallowCopy may return a reference for tracked Any-backed types (arrays,
        // maps, etc.). OpLoadDeref must always produce a stable value copy, so if
        // the result is still a reference, force a proper copy through the Variant.
        if (IsRef(result))
        {
            BoxedValue& target = *Deref(src);
            BoxedValue valueCopy;
            Visit(target.value, [&valueCopy](const auto& val)
            {
                valueCopy.value.Set<NormalizedType<decltype(val)>>(val);
            });
            instance->thread.m_regs[dstReg] = std::move(valueCopy);
            return;
        }

        instance->thread.m_regs[dstReg] = std::move(result);
    }

    SCRIPT_INLINE void OpStoreDeref(RegisterIndex dstRefReg, RegisterIndex srcReg)
    {
        BoxedValue* pTarget = Deref(instance->thread.m_regs[dstRefReg]);
        Assert(pTarget != nullptr);

        BoxedValue& srcValue = *Deref(instance->thread.m_regs[srcReg]);

        AssignValue(*pTarget, ShallowCopy(srcValue, vm->GetGC()), true);
    }

    SCRIPT_INLINE void OpLoadNull(RegisterIndex reg)
    {
        instance->thread.m_regs[reg] = BoxedValue(Handle<ObjectBase>());
    }

    SCRIPT_INLINE void OpLoadTrue(RegisterIndex reg)
    {
        instance->thread.m_regs[reg] = MakeValue(true);
    }

    SCRIPT_INLINE void OpLoadFalse(RegisterIndex reg)
    {
        instance->thread.m_regs[reg] = MakeValue(false);
    }

    SCRIPT_INLINE void OpLoadClass(RegisterIndex reg, uint64 nameHash)
    {
        Name name = Name(StringHash(nameHash));
        const Class* cls = ClassRegistry::GetInstance().GetClass(name);
        if (!cls)
        {
            // @TODO Put it back. This is commented out for now because our CodeGen tool isn't emitting
            // stuff conditionally for Lib.hyp so it's trying to load Android* stuff on Windows.

            //vm->ThrowException(instance, Exception::ClassNotFoundException(name.LookupString()));

            // TEMP
            instance->thread.m_regs[reg] = BoxedValue();

            return;
        }

        BoxedValue classValue = MakeValue(BoxedValue(ClassRef(cls)));

        instance->thread.m_regs[reg] = std::move(classValue);
    }

    SCRIPT_INLINE void OpMovOffset(uint16 offset, RegisterIndex reg)
    {
        // copy value from register to stack value at (sp - offset)
        AssignValue(
            instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - offset],
            ShallowCopy(*Deref(instance->thread.m_regs[reg]), vm->GetGC()),
            true);
    }

    SCRIPT_INLINE void OpMovIndex(uint16 index, RegisterIndex reg)
    {
        // copy value from register to stack value at index
        AssignValue(
            instance->thread.m_stack[index],
            ShallowCopy(*Deref(instance->thread.m_regs[reg]), vm->GetGC()),
            true);
    }

    SCRIPT_INLINE void OpMovStatic(uint16 index, RegisterIndex reg)
    {
        Assert(index < vm->m_staticMemory.staticSize);

        BoxedValue& dst = vm->m_staticMemory[index];

        if (dst.extData.isStaticInit)
        {
            dst = std::move(instance->thread.m_regs[reg]);

            return;
        }

        new (&dst) BoxedValue(std::move(instance->thread.m_regs[reg]));
        dst.extData.isStaticInit = 1;
    }

    SCRIPT_INLINE void OpMovArrayIdx(RegisterIndex dstReg, uint32 index, RegisterIndex srcReg)
    {
        BoxedValue& src = *Deref(instance->thread.m_regs[dstReg]);

        if (!src.Is<GenericArrayWrapper>())
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("Indexing", GetTypeString(src)));
            return;
        }

        GenericArrayWrapper& array = src.Get<GenericArrayWrapper>();

        if (index >= array.Size())
        {
            vm->ThrowException(instance, Exception::OutOfBoundsException(size_t(index), array.Size()));

            return;
        }

        BoxedValue& srcValue = *Deref(instance->thread.m_regs[srcReg]);

        BoxedValue copy = ShallowCopy(srcValue, vm->GetGC());

        // ShallowCopy may return a reference for Any-backed types in tracked memory.
        // Array elements must be independent value copies, not references that can
        // be invalidated when the source register is reused.
        if (IsRef(copy))
        {
            BoxedValue& resolved = *Deref(copy);
            BoxedValue valueCopy;
            Visit(resolved.value, [&valueCopy](const auto& val)
            {
                valueCopy.value.Set<NormalizedType<decltype(val)>>(val);
            });
            array.SetElementAt(index, std::move(valueCopy));
        }
        else
        {
            array.SetElementAt(index, std::move(copy));
        }
    }

    SCRIPT_INLINE void OpMovArrayIdxReg(RegisterIndex dstReg, RegisterIndex indexReg, RegisterIndex srcReg)
    {
        BoxedValue& src = *Deref(instance->thread.m_regs[dstReg]);

        if (!src.Is<GenericArrayWrapper>())
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("Indexing", GetTypeString(src)));
            return;
        }

        GenericArrayWrapper& array = src.Get<GenericArrayWrapper>();

        Number index;
        BoxedValue& indexRegisterValue = instance->thread.m_regs[indexReg];

        if (!GetSignedOrUnsigned(indexRegisterValue, &index))
        {
            vm->ThrowException(instance, Exception::InvalidArgsException("integer"));

            return;
        }

        uint64 indexValue;

        if (index.flags & Number::FLAG_SIGNED)
        {
            int64 indexValueSigned = index.i;

            if (indexValueSigned < 0)
            {
                // wrap around (python style)
                indexValue = array.Size() - uint64(-indexValueSigned);
            }
            else
            {
                indexValue = uint64(indexValueSigned);
            }
        }
        else
        { // unsigned
            indexValue = index.u;
        }

        if (indexValue >= array.Size())
        {
            vm->ThrowException(instance, Exception::OutOfBoundsException(indexValue, array.Size()));

            return;
        }

        BoxedValue& srcValue = *Deref(instance->thread.m_regs[srcReg]);

        BoxedValue copy = ShallowCopy(srcValue, vm->GetGC());

        // ShallowCopy may return a reference for Any-backed types in tracked memory.
        // Array elements must be independent value copies.
        if (IsRef(copy))
        {
            BoxedValue& resolved = *Deref(copy);
            BoxedValue valueCopy;
            Visit(resolved.value, [&valueCopy](const auto& val)
            {
                valueCopy.value.Set<NormalizedType<decltype(val)>>(val);
            });
            array.SetElementAt(indexValue, std::move(valueCopy));
        }
        else
        {
            array.SetElementAt(indexValue, std::move(copy));
        }
    }

    SCRIPT_INLINE void OpMov(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        instance->thread.m_regs[dstReg] = std::move(instance->thread.m_regs[srcReg]);
    }

    SCRIPT_INLINE void OpCheckHasMember(RegisterIndex dstReg, RegisterIndex srcReg, uint64 hash)
    {
        BoxedValue& src = *Deref(instance->thread.m_regs[srcReg]);
        BoxedValue& result = instance->thread.m_regs[dstReg];

        const Class* cls = nullptr;

        if (const Handle<ObjectBase>& object = GetObject(src))
        {
            cls = object.ptr->InstanceClass();
        }
        else
        {
            cls = GetClass(src.GetTypeId());
        }

        if (cls != nullptr)
        {
            IMember* member = cls->GetMember(StringHash(NameID(hash)));
            result = MakeValue(member != nullptr);

            return;
        }

        result = MakeValue(false);
    }

    SCRIPT_INLINE void OpSetField(RegisterIndex dstReg, uint64 hash, RegisterIndex srcReg)
    {
        BoxedValue& srcValue = *Deref(instance->thread.m_regs[srcReg]);
        BoxedValue newValue = PASS_AS_REF(srcValue)
            ? MakeTrackedRef(&srcValue, vm->GetGC())
            : ShallowCopy(srcValue, vm->GetGC());

        const Class* cls = nullptr;

        BoxedValue* pValue = Deref(instance->thread.m_regs[dstReg]);

        if (pValue->Is<ClassRef>())
        {
            cls = pValue->Get<ClassRef>().cls;

            StaticField* staticField = cls->GetStaticField(StringHash(NameID(hash)));

            if (!staticField)
            {
                vm->ThrowException(instance, Exception::MemberNotFoundException(pValue, hash));

                return;
            }

            staticField->SetValue(std::move(newValue));
        }
        else
        {
            if (const Handle<ObjectBase>& object = GetObject(*pValue))
            {
                cls = object.ptr->InstanceClass();
            }
            else
            {
                cls = GetClass(pValue->GetTypeId());
            }

            if (!cls)
            {
                vm->ThrowException(instance, Exception::InvalidMemberAccessException(pValue));

                return;
            }

            IMember* member = cls->GetMember(StringHash(NameID(hash)));

            if (!member)
            {
                vm->ThrowException(instance, Exception::MemberNotFoundException(pValue, hash));

                return;
            }

            if (member->GetMemberType() == MemberType::Property)
            {
                Property* property = static_cast<Property*>(member);

                if (!property->CanSet())
                {
                    vm->ThrowException(instance, Exception("Property has no setter"));

                    return;
                }

                property->Set(*pValue, newValue);
            }
            else if (member->GetMemberType() == MemberType::Field)
            {
                Field* field = static_cast<Field*>(member);
                field->Set(*pValue, std::move(newValue));
            }
            else
            {
                vm->ThrowException(instance, Exception("Member is not a field or property"));
            }
        }
    }

    SCRIPT_INLINE void OpGetMember(RegisterIndex dstReg, RegisterIndex srcReg, uint64 hash)
    {
        BoxedValue& src = *Deref(instance->thread.m_regs[srcReg]);

        const Class* cls = nullptr;

        if (const Handle<ObjectBase>& object = GetObject(src))
        {
            // instance member access
            cls = object.ptr->InstanceClass();
        }
        else if (ClassRef* classRef = src.TryGet<ClassRef>().TryGet())
        {
            // static member access on class reference
            cls = *classRef;
        }
        // special case for arrays
        else if (src.Is<GenericArrayWrapper>())
        {
            cls = GetClass(TypeId::ForType<GenericArrayWrapper>());
        }
        else
        {
            cls = GetClass(src.GetTypeId());
        }

        if (!cls)
        {
            vm->ThrowException(instance, Exception::InvalidMemberAccessException(&src));

            return;
        }

        IMember* member = cls->GetMember(StringHash(NameID(hash)));
        if (!member)
        {
            vm->ThrowException(instance, Exception::MemberNotFoundException(&src, hash));

            return;
        }

        if (member->GetMemberType() == MemberType::Field)
        {
            Field* field = static_cast<Field*>(member);
            AssertDebug(field->IsValid());

            instance->thread.m_regs[dstReg] = MakeValue(field->Get(src));
        }
        else if (member->GetMemberType() == MemberType::StaticField)
        {
            StaticField* staticField = static_cast<StaticField*>(member);
            AssertDebug(staticField->IsValid());

            instance->thread.m_regs[dstReg] = MakeValue(staticField->Get());
        }
        else if (member->GetMemberType() == MemberType::Property)
        {
            Property* property = static_cast<Property*>(member);

            if (property->CanGet())
            {
                instance->thread.m_regs[dstReg] = property->Get(src);
            }
            else
            {
                vm->ThrowException(instance, Exception("Property has no getter"));
            }
        }
        else if (member->GetMemberType() == MemberType::Method)
        {
            Method* method = static_cast<Method*>(member);

            ScriptObjectData data;

            if (method->IsScriptFunction())
            {
                Assert(method->GetParameters().Size() <= UINT8_MAX);

                data.type = ScriptObjectData::Type::ScriptFunction;
                data.func.m_addr = method->GetScriptAddress();
                data.func.m_nargs = (uint8)method->GetParameters().Size();
                data.func.m_flags = (uint8)method->GetFlags();
            }
            else
            {
                data.type = ScriptObjectData::Type::NativeFunction;
                data.nativeFunc = method;
            }

            instance->thread.m_regs[dstReg] = MakeValue(data);
        }
        else
        {
            vm->ThrowException(instance, Exception("Member is not a field or method"));
        }
    }

    SCRIPT_INLINE void OpPush(RegisterIndex reg)
    {
        // Move value from register to top of stack.

        // @NOTE - NO Deref() call here. If we loaded a reference into a register, we want to store the REFERENCE, not the value
        BoxedValue& srcValue = instance->thread.m_regs[reg];
        instance->thread.m_stack.Push(ShallowCopy(srcValue, vm->GetGC()));
    }

    SCRIPT_INLINE void OpPop()
    {
        instance->thread.m_stack.Pop();
    }

    SCRIPT_INLINE void OpPushArray(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        BoxedValue& dst = *Deref(instance->thread.m_regs[dstReg]);
        GenericArrayWrapper* arrayWrapper = dst.TryGet<GenericArrayWrapper>().TryGet();

        if (!arrayWrapper)
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("PUSH_ARRAY", GetTypeString(dst)));
            return;
        }

        arrayWrapper->PushBack(ShallowCopy(*Deref(instance->thread.m_regs[srcReg]), vm->GetGC()));
    }

    SCRIPT_INLINE void OpAddSp(uint16 n)
    {
        instance->thread.m_stack.m_sp += n;
    }

    SCRIPT_INLINE void OpSubSp(uint16 n)
    {
        instance->thread.m_stack.Pop(n);
    }

    SCRIPT_INLINE void OpJmp(BytecodeAddress addr)
    {
        instance->stream.Seek((uint32)addr);
    }

    SCRIPT_INLINE void OpJe(BytecodeAddress addr)
    {
        if (instance->thread.m_regs.flags & CF_EQUAL)
        {
            instance->stream.Seek((uint32)addr);
        }
    }

    SCRIPT_INLINE void OpJne(BytecodeAddress addr)
    {
        if (!(instance->thread.m_regs.flags & CF_EQUAL))
        {
            instance->stream.Seek((uint32)addr);
        }
    }

    SCRIPT_INLINE void OpJg(BytecodeAddress addr)
    {
        if (instance->thread.m_regs.flags & CF_GREATER)
        {
            instance->stream.Seek((uint32)addr);
        }
    }

    SCRIPT_INLINE void OpJge(BytecodeAddress addr)
    {
        if (instance->thread.m_regs.flags & (CF_GREATER | CF_EQUAL))
        {
            instance->stream.Seek((uint32)addr);
        }
    }

    SCRIPT_INLINE void OpCall(RegisterIndex reg, uint8_t nargs)
    {
        vm->Invoke(instance, std::move(instance->thread.m_regs[reg]), nargs);
    }

    SCRIPT_INLINE void OpRet()
    {
        // get top of stack (should be the address before jumping)
        BoxedValue& top = instance->thread.GetStack().Top();

        ScriptObjectData* data = GetVMData(top);
        Assert(data != nullptr);
        Assert(data->type == ScriptObjectData::Type::StackFrame);

        auto& callInfo = data->call;

        // leave function and return to previous position
        instance->stream.Seek((uint32)callInfo.returnAddress);

        int numVarArgs = callInfo.varargsPush;

        // pop call info
        instance->thread.GetStack().Pop();

        if (numVarArgs > 0)
        {
            for (int i = 0; i < numVarArgs; i++)
            {
                // NOTE: We used to just do `sp += ...`, but since each popped stack elem needs to be destructed upon Pop(),
                // we push individually constructed objects
                instance->thread.GetStack().Push(BoxedValue());
            }
        }
        else if (numVarArgs < 0)
        {
            // varargsAmt was 0: an empty varargs array was pushed onto the caller's stack
            // (to fill the variadic parameter slot). Pop it so the caller's stack is balanced.
            instance->thread.GetStack().Pop();
        }

        // decrease function depth
        instance->thread.m_funcDepth--;
    }

    SCRIPT_INLINE void OpBeginTry(BytecodeAddress addr)
    {
        ++instance->thread.m_exceptionState.m_tryCounter;

        // increase stack size to store data about this try block
        ScriptObjectData data;
        data.type = ScriptObjectData::Type::ExceptionState;
        data.exceptionState.catchAddress = addr;

        // store the info
        instance->thread.m_stack.Push(MakeValue(data));
    }

    SCRIPT_INLINE void OpEndTry()
    {
        // pop the try catch info from the stack
        BoxedValue& top = instance->thread.m_stack.Top();

        ScriptObjectData* data = GetVMData(top);
        Assert(data != nullptr);
        Assert(data->type == ScriptObjectData::Type::ExceptionState);

        Assert(instance->thread.m_exceptionState.m_tryCounter != 0);

        // pop try catch info
        instance->thread.m_stack.Pop();
        --instance->thread.m_exceptionState.m_tryCounter;
    }

    SCRIPT_INLINE void OpNew(RegisterIndex dst, RegisterIndex src) // come back to this
    {
        // read value from register
        BoxedValue& classValue = *Deref(instance->thread.m_regs[src]);

        const ClassRef& classRef = classValue.Get<ClassRef>();
        Assert(classRef.IsValid());

        BoxedValue boxed;
        if (!classRef->CreateInstance(boxed))
        {
            vm->ThrowException(
                instance,
                Exception::InvalidNewException(classRef->GetName().LookupString()));

            return;
        }

        instance->thread.m_regs[dst] = MakeValue(std::move(boxed));
    }

    SCRIPT_INLINE void OpNewArray(RegisterIndex dst, uint32 size)
    {
        // assign register value to the allocated object
        instance->thread.m_regs[dst] = MakeValue(ScriptArray(size));
    }

    SCRIPT_INLINE void OpBeginClass(RegisterIndex reg)
    {
        BytecodeStream* bs = &instance->stream;

        // Read class name length and name
        uint16 nameLen;
        bs->Read(&nameLen);

        char* nameStr = (char*)ScriptAlloc(nameLen + 1);
        nameStr[nameLen] = '\0';
        bs->Read(nameStr, nameLen);

        // Create a new class with the given name
        Name className = CreateNameFromDynamicString(nameStr);

        ScriptFree(nameStr);
        nameStr = nullptr;

        // Read type id
        TypeId::ValueType typeIdValue;
        bs->Read(&typeIdValue);

        ClassFlags flags;
        bs->Read(&flags);

        Array<MemberVariant, ScriptAllocator> members;
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

            MemberType memberType = MemberType(nextByte);
            static_assert(sizeof(MemberType) == 1, "MemberType must be 1 byte");

            // Read member count
            uint16 memberCount;
            bs->Read(&memberCount);

            // Read each member
            for (uint16 i = 0; i < memberCount; i++)
            {
                // Read member name
                uint16 memberNameLen;
                bs->Read(&memberNameLen);

                char* memberNameStr = (char*)ScriptAlloc(memberNameLen + 1);
                memberNameStr[memberNameLen] = '\0';
                bs->Read(memberNameStr, memberNameLen);

                HYP_DEFER({ ScriptFree(memberNameStr); });

                // Read attributes
                uint16 numAttrs;
                bs->Read(&numAttrs);

                Array<ClassAttribute, ScriptAllocator> attrs;
                attrs.Reserve(numAttrs);

                // Skip attributes for now - read and discard them
                for (uint16 attrIdx = 0; attrIdx < numAttrs; attrIdx++)
                {
                    ClassAttribute attr;

                    // Read attribute name
                    uint16 attrNameLen;
                    bs->Read(&attrNameLen);

                    char* attrNameStr = (char*)ScriptAlloc(attrNameLen + 1);
                    attrNameStr[attrNameLen] = '\0';
                    bs->Read(attrNameStr, attrNameLen);

                    attr.name = CreateNameFromDynamicString(attrNameStr);
                    ScriptFree(attrNameStr);

                    // Read attribute type
                    uint8 attrType;
                    bs->Read(&attrType);

                    // Skip attribute value based on type
                    switch (ClassAttributeType(attrType))
                    {
                    case ClassAttributeType::STRING:
                    {
                        uint32 strLen;
                        bs->Read(&strLen);

                        char* strData = (char*)ScriptAlloc(strLen + 1);
                        strData[strLen] = '\0';
                        bs->Read(strData, strLen);

                        attr.value = ClassAttributeValue(String(strData, strData + strLen));

                        ScriptFree(strData);

                        break;
                    }
                    case ClassAttributeType::INT:
                    {
                        int32 iValue;
                        bs->Read(&iValue);

                        attr.value = ClassAttributeValue(iValue);

                        break;
                    }
                    case ClassAttributeType::BOOLEAN:
                    {
                        ubyte bValue;
                        bs->Read(&bValue);

                        attr.value = ClassAttributeValue(bValue != 0);

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
                case MemberType::StaticField:
                {
                    // static field

                    TypeId::ValueType targetTypeIdValue;
                    bs->Read(&targetTypeIdValue);

                    uint32 size;
                    bs->Read(&size);

                    uint16 stackOffset;
                    bs->Read(&stackOffset);

                    StaticField staticField(
                        CreateNameFromDynamicString(memberNameStr),
                        &TypeOf<BoxedValue>(),
                        size,
                        attrs.ToSpan());

                    // load initial value from stack
                    Assert(stackOffset <= instance->thread.GetStack().GetStackPointer());
                    BoxedValue& initialValue = instance->thread.GetStack()[instance->thread.GetStack().GetStackPointer() - stackOffset];

                    staticField.SetValue(std::move(initialValue));

                    members.PushBack(MemberVariant(std::move(staticField)));

                    break;
                }
                case MemberType::Field:
                {
                    // field writes target typeid, offset, size
                    TypeId::ValueType targetTypeIdValue;
                    bs->Read(&targetTypeIdValue);

                    uint32 offset;
                    bs->Read(&offset);

                    uint32 size;
                    bs->Read(&size);

                    // Create field
                    members.PushBack(MemberVariant(Field(
                        CreateNameFromDynamicString(memberNameStr),
                        &TypeInfo::ForType<BoxedValue>(), // TypeId(memberTypeIdValue),
                        &TypeInfo::ForType<ObjectBase>(), // TypeId(targetTypeIdValue),
                        offset,
                        size,
                        attrs.ToSpan())));

                    break;
                }
                case MemberType::Method:
                {
                    TypeId::ValueType targetTypeIdValue;
                    bs->Read(&targetTypeIdValue);

                    uint8 flags;
                    bs->Read(&flags);

                    uint16 stackOffset;
                    bs->Read(&stackOffset);

                    // load function info from stack address
                    Assert(stackOffset <= instance->thread.GetStack().GetStackPointer(), "Stack offset out of bounds!");
                    BoxedValue& funcValue = instance->thread.GetStack()[instance->thread.GetStack().GetStackPointer() - stackOffset];

                    ScriptObjectData* funcVmData = GetVMData(funcValue);
                    Assert(funcVmData != nullptr);
                    Assert(funcVmData->type == ScriptObjectData::Type::ScriptFunction);

                    BytecodeAddress functionAddress = funcVmData->func.m_addr;
                    Assert(functionAddress != INVALID_FUNCTION_ADDRESS);

                    Method method(
                        CreateNameFromDynamicString(memberNameStr),
                        &TypeInfo::ForType<BoxedValue>(), // TypeId(memberTypeIdValue),
                        &TypeInfo::ForType<ObjectBase>(), // TypeId(targetTypeIdValue),
                        functionAddress,
                        funcVmData->func.m_flags | flags, // combine flags
                        attrs.ToSpan());

                    uint8 nargs = funcVmData->func.m_nargs;

                    if (flags & (uint8)MethodFlags::VARIADIC)
                    {
                        AssertDebug(nargs > 0);

                        --nargs;
                    }

                    method.GetParameters().Reserve(nargs);

                    for (uint8 j = 0; j < nargs; j++)
                    {
                        method.GetParameters().PushBack(MethodParameter { &TypeInfo::ForType<BoxedValue>() });
                    }

                    members.PushBack(MemberVariant(std::move(method)));

                    break;
                }
                default:
                    HYP_NOT_IMPLEMENTED();
                    break;
                }
            }
        }

        Assert(hitEnd);

        // if *not* ANONYMOUS, check if the class is already registered and use that instead.
        // @TODO: Write the end stream offset so we can just read the flags and skip the stream that amount,
        // rather than needing to read everything up to this point.
        if (!(flags & ClassFlags::ANONYMOUS))
        {
            // @FIXME: What if another thread removes it from the registry before we can add the reference?
            // Currnetly we only use script VM on a single thread, but this should be looked at for the future
            const Class* classPtr = ClassRegistry::GetInstance().GetClass(TypeId(typeIdValue));
            if (classPtr != nullptr)
            {
                instance->thread.m_regs[reg] = MakeValue(ClassRef(classPtr));
                return;
            }
        }

        // Read parent class register
        BoxedValue& parentClassValue = instance->thread.m_regs[reg];

        const Class* parentClass = nullptr;

        // Check if value in register is non-null
        if (parentClassValue.ToRef().GetPointer() != nullptr)
        {
            parentClass = parentClassValue.Get<ClassRef>();
            Assert(parentClass != nullptr);
        }

        // some type needs to be set
        Assert((flags & (ClassFlags::CLASS_TYPE | ClassFlags::STRUCT_TYPE | ClassFlags::ENUM_TYPE)));

        Class* newClass = nullptr;

        if (flags & (ClassFlags::CLASS_TYPE | ClassFlags::ENUM_TYPE)) // enum is here temporarily
        {
            newClass = new DynamicClassInstance(
                TypeId(typeIdValue),
                className,
                parentClass,
                Span<const ClassAttribute>(), // @TODO
                flags,
                members.ToSpan());
        }
        else if (flags & ClassFlags::STRUCT_TYPE)
        {
            DynamicStructInstanceFunctions functions {};
            functions.construct = [](void* ctx, void* dest) -> void
            {
                ubyte* ptrRaw = reinterpret_cast<ubyte*>(dest);

                const DynamicStructInstance* pStruct = static_cast<const DynamicStructInstance*>(ctx);

                for (const Field* field : pStruct->GetFields())
                {
                    if (field->GetTypeId().Value() == BoxedValueTypeId)
                    {
                        new (ptrRaw + field->GetOffset()) BoxedValue;
                    }
                }
            };
            functions.copy = [](void* ctx, const void* src) -> void*
            {
                const DynamicStructInstance* pStruct = static_cast<const DynamicStructInstance*>(ctx);

                void* dest = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(pStruct->GetSize(), pStruct->GetAlignment());

                const ubyte* srcRaw = reinterpret_cast<const ubyte*>(src);
                ubyte* destRaw = reinterpret_cast<ubyte*>(dest);

                for (const Field* field : pStruct->GetFields())
                {
                    if (field->GetTypeId().Value() == BoxedValueTypeId)
                    {
                        const auto* srcBoxed = reinterpret_cast<const BoxedValue*>(srcRaw + field->GetOffset());
                        new (destRaw + field->GetOffset()) BoxedValue(*srcBoxed);
                    }
                    else
                    {
                        Memory::Copy(destRaw + field->GetOffset(), srcRaw + field->GetOffset(), field->GetSize());
                    }
                }

                return dest;
            };
            functions.destruct = [](void* ctx, void* ptr) -> void
            {
                const DynamicStructInstance* pStruct = static_cast<const DynamicStructInstance*>(ctx);

                ubyte* ptrRaw = reinterpret_cast<ubyte*>(ptr);

                for (const Field* field : pStruct->GetFields())
                {
                    if (field->GetTypeId().Value() == BoxedValueTypeId)
                    {
                        reinterpret_cast<BoxedValue*>(ptrRaw + field->GetOffset())->~BoxedValue();
                    }
                }

                GetDefaultAllocatorInstance<DynamicAllocator>()->Free(ptr);
            };

            newClass = new DynamicStructInstance(
                TypeId(typeIdValue),
                className,
                Span<const ClassAttribute>(), // @TODO
                flags,
                members.ToSpan(),
                functions);
        }
        else
        {
            HYP_NOT_IMPLEMENTED();
        }

        // Only register if not anonymous
        if (!(flags & ClassFlags::ANONYMOUS))
        {
            bool wasRegistered = false;
            ClassRegistry::GetInstance().Register(newClass->GetTypeId(), newClass, &wasRegistered);

            if (!wasRegistered)
            {
                // Throw exception due to class registration failing.
                vm->ThrowException(instance, Exception("Invalid operation: The class was already registered with the class registry!"));

                // Delete as to not leak the memory.
                delete newClass;

                return;
            }
        }

        instance->thread.m_regs[reg] = MakeValue(ClassRef(newClass));
    }

    SCRIPT_INLINE void OpCmp(RegisterIndex lhsReg, RegisterIndex rhsReg)
    {
        // dropout early for comparing something against itself
        if (lhsReg == rhsReg)
        {
            instance->thread.m_regs.flags = CF_EQUAL;
            return;
        }

        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetSignedOrUnsigned(*lhs, &a) && GetSignedOrUnsigned(*rhs, &b))
        {
            if ((a.flags & Number::FLAG_SIGNED) && (b.flags & Number::FLAG_SIGNED))
            {
                instance->thread.m_regs.flags = (a.i == b.i) ? CF_EQUAL : ((a.i > b.i) ? CF_GREATER : CF_NONE);
            }
            else if ((a.flags & Number::FLAG_SIGNED) && (b.flags & Number::FLAG_UNSIGNED))
            {
                instance->thread.m_regs.flags = (a.i == b.u) ? CF_EQUAL : ((a.i > b.u) ? CF_GREATER : CF_NONE);
            }
            else if ((a.flags & Number::FLAG_UNSIGNED) && (b.flags & Number::FLAG_SIGNED))
            {
                instance->thread.m_regs.flags = (a.u == b.i) ? CF_EQUAL : ((a.u > b.i) ? CF_GREATER : CF_NONE);
            }
            else if ((a.flags & Number::FLAG_UNSIGNED) && (b.flags & Number::FLAG_UNSIGNED))
            {
                instance->thread.m_regs.flags = (a.u == b.u) ? CF_EQUAL : ((a.u > b.u) ? CF_GREATER : CF_NONE);
            }
        }
        else if (GetNumber(*lhs, &a.f) && GetNumber(*rhs, &b.f))
        {
            instance->thread.m_regs.flags = (a.f == b.f) ? CF_EQUAL : ((a.f > b.f) ? CF_GREATER : CF_NONE);
        }
        else
        {
            bool lhsBool;
            bool rhsBool;

            if (GetBoolean(*lhs, &lhsBool) && GetBoolean(*rhs, &rhsBool))
            {
                instance->thread.m_regs.flags = (lhsBool == rhsBool) ? CF_EQUAL : ((lhsBool > rhsBool) ? CF_GREATER : CF_NONE);
            }
            else if (lhs->Is<ScriptString>() && rhs->Is<ScriptString>())
            {
                const ScriptString& lstr = lhs->Get<ScriptString>();
                const ScriptString& rstr = rhs->Get<ScriptString>();

                if (lstr == rstr)
                    instance->thread.m_regs.flags = CF_EQUAL;
                else if (lstr > rstr)
                    instance->thread.m_regs.flags = CF_GREATER;
                else
                    instance->thread.m_regs.flags = CF_NONE;
            }
            else if (lhs->Is<Name>() && rhs->Is<Name>())
            {
                const Name lname = lhs->Get<Name>();
                const Name rname = rhs->Get<Name>();

                if (lname == rname)
                    instance->thread.m_regs.flags = CF_EQUAL;
                else if (lname < rname)
                    instance->thread.m_regs.flags = CF_GREATER;
                else
                    instance->thread.m_regs.flags = CF_NONE;
            }
            else
            {
                const int res = CompareAsPointers(*lhs, *rhs);

                if (res != -1)
                {
                    instance->thread.m_regs.flags = res;
                }
                else
                {
                    vm->ThrowException(instance, Exception::InvalidComparisonException(GetTypeString(*lhs), GetTypeString(*rhs)));
                }
            }
        }
    }

    SCRIPT_INLINE void OpCmpZ(RegisterIndex reg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[reg]);

        Number num;

        if (GetSignedOrUnsigned(*lhs, &num))
        {
            instance->thread.m_regs.flags = ((num.flags & Number::FLAG_SIGNED) ? !num.i : !num.u) ? CF_EQUAL : CF_NONE;
        }
        else if (GetFloatingPoint(*lhs, &num.f))
        {
            instance->thread.m_regs.flags = !num.f ? CF_EQUAL : CF_NONE;
        }
        else
        {
            bool boolValue;
            if (GetBoolean(*lhs, &boolValue))
            {
                instance->thread.m_regs.flags = !boolValue ? CF_EQUAL : CF_NONE;
            }
            else
            {
                void* ptrValue = lhs->ToRef().GetPointer();

                instance->thread.m_regs.flags = !ptrValue ? CF_EQUAL : CF_NONE;
            }
        }
    }

    SCRIPT_INLINE void OpAdd(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, +);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("ADD", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpSub(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, -);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("SUB", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpMul(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, *);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("MUL", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpDiv(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                vm->ThrowException(instance, Exception::DivisionByZeroException());

                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                vm->ThrowException(instance, Exception::DivisionByZeroException());

                return;
            }

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, /);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("DIV", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpMod(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            // custom handling for mod to allow floats to work
            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                vm->ThrowException(instance, Exception::DivisionByZeroException());

                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                vm->ThrowException(instance, Exception::DivisionByZeroException());

                return;
            }

            Number result { numericType };

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

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("MOD", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpAnd(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, &);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("AND", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpOr(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, |);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("OR", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpXor(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, ^);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("XOR", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpShl(RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = GetNumericType(*lhs);

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, <<);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("SHL", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpShr(RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        BoxedValue* lhs = Deref(instance->thread.m_regs[lhsReg]);
        BoxedValue* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = GetNumericType(*lhs);

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, >>);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("SHR", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpNot(RegisterIndex reg)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[reg]);

        Number num;

        // we only allow bitwise NOT on integers
        if (!GetNumber(value, &num) || !(num.flags & (Number::FLAG_SIGNED | Number::FLAG_UNSIGNED)))
        {
            vm->ThrowException(instance, Exception::InvalidBitwiseArgument());

            return;
        }

        const NumericType numericType = GetNumericType(value);

        Number result { numericType };
        result.u = ~num.u; // flip the bits, signedness doesn't matter for bitwise NOT

        instance->thread.m_regs[reg] = MakeValue(result);
    }

    SCRIPT_INLINE void OpThrow(RegisterIndex reg)
    {
        // load value from register
        [[maybe_unused]] BoxedValue* value = Deref(instance->thread.m_regs[reg]);

        /// \todo Allow throwing the arugment

        vm->ThrowException(instance, Exception("User exception"));
    }

    SCRIPT_INLINE void OpExportSymbol(RegisterIndex reg, uint64 hash)
    {
        BoxedValue& srcValue = *Deref(instance->thread.m_regs[reg]);

        BoxedValue newValue = PASS_AS_REF(srcValue)
            ? MakeTrackedRef(&srcValue, vm->GetGC())
            : ShallowCopy(srcValue, vm->GetGC());

        if (!instance->exportedSymbols.Store(hash, std::move(newValue)).second)
        {
            vm->ThrowException(instance, Exception::DuplicateExportException());
        }
    }

    SCRIPT_INLINE void OpNeg(RegisterIndex reg)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[reg]);

        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidOperationException("NEG", GetTypeString(value)));

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
            // handle unsigned wraparound correctly:
            // e.g. for uint8: 0 -> 0, 1 -> 255, 2 -> 254, ..., 255 -> 1
            switch (num.flags & Number::FLAG_BIT_WIDTH_MASK)
            {
            case Number::FLAG_8_BIT:
                result.u = uint8(~uint8(num.u) + 1);
                break;
            case Number::FLAG_16_BIT:
                result.u = uint16(~uint16(num.u) + 1);
                break;
            case Number::FLAG_32_BIT:
                result.u = uint32(~uint32(num.u) + 1);
                break;
            case Number::FLAG_64_BIT:
                result.u = uint64(~uint64(num.u) + 1);
                break;
            default:
                HYP_UNREACHABLE();
                break;
            }
        }
        else
        {
            result.f = -num.f;
        }

        instance->thread.m_regs[reg] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU8(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);

        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "uint8"));

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU16(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);

        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "uint16"));

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU32(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "uint32"));

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU64(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "uint64"));

            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI8(RegisterIndex dst, RegisterIndex src)
    {
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "int8"));

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI16(RegisterIndex dst, RegisterIndex src)
    {
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "int16"));

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI32(RegisterIndex dst, RegisterIndex src)
    {
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "int32"));

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI64(RegisterIndex dst, RegisterIndex src)
    {
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "int64"));

            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastF32(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "float32"));

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastF64(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "float64"));

            return;
        }

        Number result;
        result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;

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

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastBool(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);

        // use same logic as CmpZ to determine truthiness
        bool result = false;

        Number num;

        if (GetSignedOrUnsigned(value, &num))
        {
            result = (num.flags & Number::FLAG_SIGNED) ? (num.i != 0) : (num.u != 0);
        }
        else if (GetFloatingPoint(value, &num.f))
        {
            result = (num.f != 0.0);
        }
        else if (GetBoolean(value, &result))
        {
            // already a bool, do nothing
        }
        else
        {
            void* ptrValue = value.ToRef().GetPointer();
            result = (ptrValue != nullptr);
        }

        instance->thread.m_regs[dst] = MakeValue(result);
    }

    SCRIPT_INLINE void OpCastString(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        BoxedValue& value = *Deref(instance->thread.m_regs[src]);

        const ScriptString* pString = nullptr;

        if (!GetString(value, &pString))
        {
            vm->ThrowException(instance, Exception::InvalidCastException(GetTypeString(value), "string"));

            return;
        }

        instance->thread.m_regs[dst] = ShallowCopy(value, vm->GetGC());
    }

    SCRIPT_INLINE void OpCastDynamic(RegisterIndex dst, RegisterIndex src, uint64 typeNameHash)
    {
        BoxedValue& typeRefValue = *Deref(instance->thread.m_regs[dst]);

        BoxedValue& value = *Deref(instance->thread.m_regs[src]);

        if (value.ToRef().GetPointer() != nullptr)
        {
            if (ClassRef* classRef = typeRefValue.TryGet<ClassRef>().TryGet())
            {
                const Class* cls = nullptr;

                if (const Handle<ObjectBase>& object = GetObject(value))
                {
                    cls = object.ptr->InstanceClass();
                }
                else
                {
                    cls = GetClass(value.GetTypeId());
                }

                if (!cls || !cls->IsDerivedFrom(*classRef))
                {
                    instance->thread.m_regs[dst] = BoxedValue(Handle<ObjectBase>());

                    return;
                }
            }
            else
            {
                const char* typeName = GetTypeString(value);

                if (typeName == nullptr || HashCode::GetHashCode(typeName).Value() != typeNameHash)
                {
                    // Set Null.
                    instance->thread.m_regs[dst] = BoxedValue(Handle<ObjectBase>());

                    return;
                }
            }
        }

        instance->thread.m_regs[dst] = ShallowCopy(value, vm->GetGC());
    }

    SCRIPT_INLINE void OpIsInstance(RegisterIndex dst, RegisterIndex src, RegisterIndex typeRef, uint64 typeNameHash)
    {
        BoxedValue& typeRefValue = *Deref(instance->thread.m_regs[typeRef]);

        bool result = false;

        if (ClassRef* classRef = typeRefValue.TryGet<ClassRef>().TryGet())
        {
            BoxedValue& value = *Deref(instance->thread.m_regs[src]);

            if (value.ToRef().GetPointer() != nullptr)
            {
                const Class* cls = nullptr;

                if (const Handle<ObjectBase>& object = GetObject(value))
                {
                    cls = object.ptr->InstanceClass();
                }
                else
                {
                    cls = GetClass(value.GetTypeId());
                }

                if (cls && cls->IsDerivedFrom(*classRef))
                {
                    result = true;
                }
            }
        }
        else
        {
            BoxedValue& value = *Deref(instance->thread.m_regs[src]);

            if (value.ToRef().GetPointer() != nullptr)
            {
                const char* typeName = GetTypeString(value);

                if (typeName != nullptr)
                {
                    if (HashCode::GetHashCode(typeName).Value() == typeNameHash)
                    {
                        result = true;
                    }
                }
            }
        }

        instance->thread.m_regs[dst] = MakeValue(result);
    }
};

#if HYP_DEBUG_MODE
static void DiagnoseUnknownInstruction(BytecodeStream* bs, int64 errorPos, ubyte code)
{
    constexpr size_t windowBack = 512;

    const size_t savedPos = bs->Position();
    const size_t startPos = (errorPos > int64(windowBack)) ? (errorPos - windowBack) : 0;

    std::cerr << "=== Bytecode disassembly around unknown instruction 0x"
              << std::hex << (int)code << std::dec
              << " at position " << errorPos << std::hex << " (" << errorPos << ")" << " ===" << std::endl;

    bs->Seek(startPos);

    DecompilationUnit decompilationUnit;
    decompilationUnit.Decompile(*bs, &std::cerr);

    bs->Seek(savedPos);

    std::cerr << "=== End disassembly ===" << std::endl;
}
#endif

/// \todo : Make all instructions that have args emit aligned up to 1 word (or at least 32 bits)

SCRIPT_INLINE static void HandleInstruction(
    ScriptInstance* instance,
    InstructionHandler* handler,
    ubyte code)
{
    BytecodeStream* bs = &instance->stream;

    switch (code)
    {
    case LOAD_UNIFIED:
    {
        uint8 subcmd;
        bs->Read(&subcmd);

        RegisterIndex reg;
        bs->Read(&reg);

        const uint8 dataType = GET_LOAD_DTYPE(subcmd);
        const bool isRef = GET_LOAD_ISREF(subcmd);
        const uint8 srcType = GET_LOAD_SRCTYPE(subcmd);

        switch (srcType)
        {
        case LSRC_IMMEDIATE:
        {
            switch (dataType)
            {
            case DTYPE_I32:
            {
                int32 value;
                bs->Read(&value);

                handler->OpLoadI32(reg, value);

                break;
            }
            case DTYPE_I64:
            {
                int64 value;
                bs->Read(&value);

                handler->OpLoadI64(reg, value);

                break;
            }
            case DTYPE_U32:
            {
                uint32 value;
                bs->Read(&value);

                handler->OpLoadU32(reg, value);

                break;
            }
            case DTYPE_U64:
            {
                uint64 value;
                bs->Read(&value);

                handler->OpLoadU64(reg, value);

                break;
            }
            case DTYPE_F32:
            {
                float32 value;
                bs->Read(&value);

                handler->OpLoadF32(reg, value);

                break;
            }
            case DTYPE_F64:
            {
                float64 value;
                bs->Read(&value);

                handler->OpLoadF64(reg, value);

                break;
            }
            case DTYPE_BOOL:
            {
                uint8 value;
                bs->Read(&value);
                if (value)
                    handler->OpLoadTrue(reg);
                else
                    handler->OpLoadFalse(reg);

                break;
            }

            case DTYPE_OBJECT:
                // Load null for immediate object
                handler->OpLoadNull(reg);
                break;
            }

            break;
        } // LSRC_IMMEDIATE
        case LSRC_OFFSET:
        {
            uint16 offset;
            bs->Read(&offset);

            if (isRef)
                handler->OpLoadOffsetRef(reg, offset);
            else
                handler->OpLoadOffset(reg, offset);

            break;
        } // LSRC_OFFSET
        case LSRC_INDEX:
        {
            uint16 index;
            bs->Read(&index);

            if (isRef)
                handler->OpLoadIndexRef(reg, index);
            else
                handler->OpLoadIndex(reg, index);

            break;
        } // LSRC_INDEX
        case LSRC_STATIC:
        {
            uint16 index;
            bs->Read(&index);

            handler->OpLoadStatic(reg, index);

            break;
        } // LSRC_STATIC
        case LSRC_ARRAYIDX:
        {
            RegisterIndex arrayReg;
            bs->Read(&arrayReg);

            RegisterIndex indexReg;
            bs->Read(&indexReg);

            handler->OpLoadArrayIdx(reg, arrayReg, indexReg);

            break;
        } // LSRC_ARRAYIDX
        case LSRC_MEMBER:
        {
            RegisterIndex objReg;
            bs->Read(&objReg);

            uint64 hash;
            bs->Read(&hash);

            handler->OpGetMember(reg, objReg, hash);

            break;
        } // LSRC_MEMBER
        case LSRC_REGISTER:
        {
            RegisterIndex srcReg;
            bs->Read(&srcReg);

            if (isRef)
                handler->OpLoadRef(reg, srcReg);
            else
                handler->OpLoadDeref(reg, srcReg);

            break;
        } // LSRC_REGISTER
        case LSRC_ADDRESS:
        {
            BytecodeAddress addr;
            bs->Read(&addr);

            handler->OpLoadAddr(reg, addr);

            break;
        } // LSRC_ADDRESS
        }

        break;
    }
    case MOV_UNIFIED:
    {
        uint8 subcmd;
        bs->Read(&subcmd);

        const uint8 dstType = GET_MOV_DSTTYPE(subcmd);
        const uint8 srcType = GET_MOV_SRCTYPE(subcmd);
        const bool isArrayStore = GET_MOV_ARRAYSTORE(subcmd);
        const bool isDerefDst = GET_MOV_DEREFDST(subcmd);

        // Handle array store operations first
        if (isArrayStore)
        {
            RegisterIndex arrayReg;
            bs->Read(&arrayReg);
            uint32 index;
            bs->Read(&index);
            RegisterIndex srcReg;
            bs->Read(&srcReg);
            handler->OpMovArrayIdx(arrayReg, index, srcReg);
        }
        else
        {
            switch (dstType)
            {
            case MDST_OFFSET:
            {
                uint16 offset;
                bs->Read(&offset);
                RegisterIndex srcReg;
                bs->Read(&srcReg);
                handler->OpMovOffset(offset, srcReg);
            }
            break;

            case MDST_INDEX:
            {
                uint16 index;
                bs->Read(&index);
                RegisterIndex srcReg;
                bs->Read(&srcReg);
                handler->OpMovIndex(index, srcReg);
            }
            break;

            case MDST_STATIC:
            {
                uint16 index;
                bs->Read(&index);
                RegisterIndex srcReg;
                bs->Read(&srcReg);
                handler->OpMovStatic(index, srcReg);
            }
            break;

            case MDST_REGISTER:
                switch (srcType)
                {
                case MSRC_REGISTER:
                {
                    RegisterIndex dstReg;
                    bs->Read(&dstReg);
                    RegisterIndex srcReg;
                    bs->Read(&srcReg);

                    if (isDerefDst)
                        handler->OpStoreDeref(dstReg, srcReg);
                    else
                        handler->OpMov(dstReg, srcReg);
                }
                break;

                case MSRC_ARRAYIDX:
                {
                    RegisterIndex dstReg;
                    bs->Read(&dstReg);
                    uint32 index;
                    bs->Read(&index);
                    RegisterIndex srcReg;
                    bs->Read(&srcReg);
                    handler->OpMovArrayIdx(dstReg, index, srcReg);
                }
                break;

                case MSRC_ARRAYIDX_REG:
                {
                    RegisterIndex dstReg;
                    bs->Read(&dstReg);
                    RegisterIndex indexReg;
                    bs->Read(&indexReg);
                    RegisterIndex srcReg;
                    bs->Read(&srcReg);
                    handler->OpMovArrayIdxReg(dstReg, indexReg, srcReg);
                }
                break;

                case MSRC_MEMBER:
                {
                    RegisterIndex dstReg;
                    bs->Read(&dstReg);
                    uint64 hash;
                    bs->Read(&hash);
                    RegisterIndex srcReg;
                    bs->Read(&srcReg);
                    handler->OpSetField(dstReg, hash, srcReg);
                }
                break;
                }
                break;
            }
        }

        break;
    } // MOV_UNIFIED
    case CAST_UNIFIED:
    {
        uint8 subcmd;
        bs->Read(&subcmd);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        RegisterIndex srcReg;
        bs->Read(&srcReg);

        const uint8 castType = GET_CAST_TYPE(subcmd);

        switch (castType)
        {
        case CAST_TYPE_U8:
            handler->OpCastU8(dstReg, srcReg);
            break;
        case CAST_TYPE_U16:
            handler->OpCastU16(dstReg, srcReg);
            break;
        case CAST_TYPE_U32:
            handler->OpCastU32(dstReg, srcReg);
            break;
        case CAST_TYPE_U64:
            handler->OpCastU64(dstReg, srcReg);
            break;
        case CAST_TYPE_I8:
            handler->OpCastI8(dstReg, srcReg);
            break;
        case CAST_TYPE_I16:
            handler->OpCastI16(dstReg, srcReg);
            break;
        case CAST_TYPE_I32:
            handler->OpCastI32(dstReg, srcReg);
            break;
        case CAST_TYPE_I64:
            handler->OpCastI64(dstReg, srcReg);
            break;
        case CAST_TYPE_F32:
            handler->OpCastF32(dstReg, srcReg);
            break;
        case CAST_TYPE_F64:
            handler->OpCastF64(dstReg, srcReg);
            break;
        case CAST_TYPE_BOOL:
            handler->OpCastBool(dstReg, srcReg);
            break;
        case CAST_TYPE_STRING:
            handler->OpCastString(dstReg, srcReg);
            break;
        case CAST_TYPE_DYNAMIC:
        {
            uint64 typeNameHash;
            bs->Read(&typeNameHash);

            handler->OpCastDynamic(dstReg, srcReg, typeNameHash);

            break;
        }
        default:
            HYP_UNREACHABLE();
        }

        break;
    } // CAST_UNIFIED
    case LOAD_OFFSET:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint16 offset;
        bs->Read(&offset);

        handler->OpLoadOffset(reg, offset);

        break;
    } // LOAD_OFFSET
    case LOAD_STRING:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        // get string length
        uint32 len;
        bs->Read(&len);

        // read string based on length
        char* str = (char*)ScriptAlloc(len + 1);
        str[len] = '\0';
        bs->Read(str, len);

        handler->OpLoadConstantString(reg, len, str);

        ScriptFree(str);

        break;
    } // LOAD_STRING
    case LOAD_ARRAYIDX:
    {
        RegisterIndex dstReg;
        bs->Read(&dstReg);

        RegisterIndex srcReg;
        bs->Read(&srcReg);

        RegisterIndex indexReg;
        bs->Read(&indexReg);

        handler->OpLoadArrayIdx(dstReg, srcReg, indexReg);

        break;
    } // LOAD_ARRAYIDX
    case LOAD_OFFSET_REF:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint16 offset;
        bs->Read(&offset);

        handler->OpLoadOffsetRef(reg, offset);

        break;
    } // LOAD_OFFSET_REF
    case LOAD_FUNC:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        BytecodeAddress addr;
        bs->Read(&addr);

        uint8 nargs;
        bs->Read(&nargs);

        uint8 flags;
        bs->Read(&flags);

        handler->OpLoadFunc(reg, addr, nargs, flags);

        break;
    } // LOAD_FUNC
    case LOAD_CLASS:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint64 nameHash;
        bs->Read(&nameHash);

        handler->OpLoadClass(reg, nameHash);

        break;
    } // LOAD_CLASS
    case REF:
    {
        RegisterIndex dstReg;
        RegisterIndex srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler->OpLoadRef(dstReg, srcReg);

        break;
    } // REF
    case DEREF:
    {
        RegisterIndex dstReg;
        RegisterIndex srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler->OpLoadDeref(dstReg, srcReg);

        break;
    } // DEREF
    case MOV_OFFSET:
    {
        uint16 offset;
        bs->Read(&offset);

        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpMovOffset(offset, reg);

        break;
    } // MOV_OFFSET
    case MOV_INDEX:
    {
        uint16 index;
        bs->Read(&index);
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpMovIndex(index, reg);

        break;
    } // MOV_INDEX
    case MOV_STATIC:
    {
        uint16 index;
        bs->Read(&index);

        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpMovStatic(index, reg);

        break;
    } // MOV_STATIC
    case MOV_ARRAYIDX:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        uint32 index;
        bs->Read(&index);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpMovArrayIdx(dst, index, src);

        break;
    } // MOV_ARRAYIDX
    case MOV_ARRAYIDX_REG:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex indexReg;
        bs->Read(&indexReg);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpMovArrayIdxReg(dst, indexReg, src);

        break;
    } // MOV_ARRAYIDX_REG
    case MOV:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpMov(dst, src);

        break;
    } // MOV
    case CHECK_HAS_MEMBER:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex src;
        bs->Read(&src);

        uint64 hash;
        bs->Read(&hash);

        handler->OpCheckHasMember(dst, src, hash);

        break;
    } // CHECK_HAS_MEMBER
    case PUSH:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpPush(reg);

        break;
    } // PUSH
    case POP:
    {
        handler->OpPop();

        break;
    } // POP
    case PUSH_ARRAY:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpPushArray(dst, src);

        break;
    } // PUSH_ARRAY
    case ADD_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler->OpAddSp(val);

        break;
    } // ADD_SP
    case SUB_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler->OpSubSp(val);

        break;
    } // SUB_SP
    case JMP:
    {
        BytecodeAddress addr;
        bs->Read(&addr);

        handler->OpJmp(addr);

        break;
    } // JMP
    case JE:
    {
        BytecodeAddress addr;
        bs->Read(&addr);

        handler->OpJe(addr);

        break;
    } // JE
    case JNE:
    {
        BytecodeAddress addr;
        bs->Read(&addr);

        handler->OpJne(addr);

        break;
    } // JNE
    case JG:
    {
        BytecodeAddress addr;
        bs->Read(&addr);

        handler->OpJg(addr);

        break;
    } // JG
    case JGE:
    {
        BytecodeAddress addr;
        bs->Read(&addr);

        handler->OpJge(addr);

        break;
    } // JGE
    case CALL:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint8 nargs;
        bs->Read(&nargs);

        handler->OpCall(reg, nargs);

        break;
    } // CALL
    case RET:
    {
        handler->OpRet();

        break;
    } // RET
    case BEGIN_TRY:
    {
        BytecodeAddress catchAddress;
        bs->Read(&catchAddress);

        handler->OpBeginTry(catchAddress);

        break;
    } // BEGIN_TRY
    case END_TRY:
    {
        handler->OpEndTry();

        break;
    } // END_TRY
    case NEW:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpNew(dst, src);

        break;
    } // NEW
    case NEW_ARRAY:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        uint32 size;
        bs->Read(&size);

        handler->OpNewArray(dst, size);

        break;
    } // NEW_ARRAY
    case CMP:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        handler->OpCmp(lhsReg, rhsReg);

        break;
    } // CMP
    case BEGIN_CLASS:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpBeginClass(reg);

        break;
    } // BEGIN_CLASS
    case CMPZ:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpCmpZ(reg);

        break;
    } // CMPZ
    case ADD:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpAdd(lhsReg, rhsReg, dstReg);

        break;
    } // ADD
    case SUB:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpSub(lhsReg, rhsReg, dstReg);

        break;
    } // SUB
    case MUL:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpMul(lhsReg, rhsReg, dstReg);

        break;
    } // MUL
    case DIV:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpDiv(lhsReg, rhsReg, dstReg);

        break;
    } // DIV
    case MOD:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpMod(lhsReg, rhsReg, dstReg);

        break;
    } // MOD
    case AND:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpAnd(lhsReg, rhsReg, dstReg);

        break;
    } // AND
    case OR:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpOr(lhsReg, rhsReg, dstReg);

        break;
    } // OR
    case XOR:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpXor(lhsReg, rhsReg, dstReg);

        break;
    } // XOR
    case SHL:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpShl(lhsReg, rhsReg, dstReg);

        break;
    } // SHL
    case SHR:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        RegisterIndex dstReg;
        bs->Read(&dstReg);

        handler->OpShr(lhsReg, rhsReg, dstReg);

        break;
    } // SHR
    case NEG:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpNeg(reg);

        break;
    } // NEG
    case NOT:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpNot(reg);

        break;
    } // NOT
    case THROW:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpThrow(reg);

        break;
    } // THROW
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

        handler->vm->m_tracemap.Set(stringmap, linemap);

        break;
    } // TRACEMAP
    case REM:
    {
        uint32 len;
        bs->Read(&len);
        // just skip comment
        bs->Skip(len);

        break;
    } // REM
    case EXPORT:
    {
        RegisterIndex reg;
        bs->Read(&reg);
        uint64 hash;
        bs->Read(&hash);

        handler->OpExportSymbol(reg, hash);

        break;
    } // EXPORT
    case IS_INSTANCE:
    {
        RegisterIndex dstReg;
        bs->Read(&dstReg);

        RegisterIndex srcReg;
        bs->Read(&srcReg);

        RegisterIndex typeRefReg;
        bs->Read(&typeRefReg);

        uint64 typeNameHash;
        bs->Read(&typeNameHash);

        handler->OpIsInstance(dstReg, srcReg, typeRefReg, typeNameHash);

        break;
    } // IS_INSTANCE
    default:
    {
        int64 lastPos = int64(bs->Position()) - sizeof(ubyte);
#if HYP_DEBUG_MODE
        DiagnoseUnknownInstruction(bs, lastPos, code);
#endif
        HYP_FAIL("unknown instruction '{}' referenced at location {}", code, lastPos);
        // seek to end of bytecode stream
        instance->stream.Seek(bs->Size());

        return;
    }
    }
}

#pragma endregion InstructionHandler

#pragma region VirtualMachine

VirtualMachine::VirtualMachine()
    : m_unhandledException(nullptr)
{
    m_gc = (GarbageCollector*)ScriptAlloc(sizeof(GarbageCollector), alignof(GarbageCollector));
    new (m_gc) GarbageCollector;
}

VirtualMachine::~VirtualMachine()
{
    if (m_gc)
    {
        m_gc->~GarbageCollector();
        ScriptFree(m_gc);
    }

    if (m_unhandledException)
    {
        m_unhandledException->~Exception();
        ScriptFree(m_unhandledException);
    }
}

void VirtualMachine::CollectGarbage(Span<ScriptInstance*> instances)
{
    if (m_gc == nullptr)
    {
        return;
    }

    m_gc->ClearMarks();

    for (ScriptInstance* instance : instances)
    {
        if (instance == nullptr)
        {
            continue;
        }

        Script_StackMemory& stack = instance->thread.GetStack();
        m_gc->MarkReachable(Span<BoxedValue>(stack.GetData(), stack.GetStackPointer()));

        Script_RegisterMemory& regs = instance->thread.GetRegisters();
        m_gc->MarkReachable(Span<BoxedValue>(regs.values.GetPointer(), Script_RegisterMemory::NumRegisters));
    }

    m_gc->Collect();
}

void VirtualMachine::ThrowException(ScriptInstance* instance, const Exception& exception)
{
    ++instance->thread.m_exceptionState.m_exceptionDepth;

    // go to catch
    if (HandleException(instance))
    {
        return;
    }

    // Move bytestream to end so loops that check if EOF is reached stop.
    instance->stream.Seek(instance->stream.Size());

    // exception cannot be handled, no try block found
    if (instance->thread.m_id == 0)
    {
        DebugLog(LogType::Error, "unhandled exception in main thread: %s", exception.ToString());
    }
    else
    {
        DebugLog(LogType::Error, "unhandled exception in thread %d: %s", instance->thread.m_id, exception.ToString());
    }

    if (!m_unhandledException)
    {
        m_unhandledException = (Exception*)ScriptAlloc(sizeof(Exception), alignof(Exception));
    }
    else
    {
        m_unhandledException->~Exception();
    }

    new (m_unhandledException) Exception(exception);
}

void VirtualMachine::Invoke(ScriptInstance* instance, BoxedValue&& value, uint8 nargs)
{
    BytecodeStream* bs = &instance->stream;

    BoxedValue& deref = *Deref(value);

    if (IsFunction(deref))
    {
        if (IsNativeFunction(deref))
        {
            BoxedValue** argsBoxed = (BoxedValue**)StackAlloc((nargs > 0 ? nargs : 1) * sizeof(BoxedValue*));

            for (int argIndex = 0; argIndex < nargs; argIndex++)
            {
                BoxedValue& srcValue = *Deref(instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - int(nargs) + argIndex]);

                argsBoxed[argIndex] = &srcValue;
            }

            /// \todo : Implement
            // disable auto gc so no collections happen during a native function
            //            enableAutoGc = false;

            // call the native function
            ScriptObjectData* data = GetVMData(deref);
            Assert(data != nullptr && data->nativeFunc != nullptr);

            if (data->nativeFunc->GetFlags() & MethodFlags::MEMBER)
            {
                AssertDebug(nargs >= 1);

                if (argsBoxed[0]->IsNull())
                {
                    // throw exception if target is null
                    ThrowException(instance, Exception::NullReferenceException());
                    return;
                }
            }

            // set register 0 to the result
            instance->thread.GetRegisters()[0] = data->nativeFunc->Invoke(Span<BoxedValue*>(argsBoxed, nargs));

            // re-enable auto gc
            //            enableAutoGc = ENABLE_GC;

            return;
        }

        // non-native function here
        ScriptObjectData* data = GetVMData(deref);
        Assert(data != nullptr && data->type == ScriptObjectData::Type::ScriptFunction);

        if ((data->func.m_flags & (uint8)MethodFlags::VARIADIC) && nargs < data->func.m_nargs - 1)
        {
            // if variadic, make sure the arg count is /at least/ what is required
            ThrowException(instance, Exception::InvalidArgsException(data->func.m_nargs, nargs, true));
        }
        else if (!(data->func.m_flags & (uint8)MethodFlags::VARIADIC) && data->func.m_nargs != nargs)
        {
            ThrowException(instance, Exception::InvalidArgsException(data->func.m_nargs, nargs));
        }
        else
        {
            ScriptObjectData previousAddr;
            previousAddr.type = ScriptObjectData::Type::StackFrame;
            previousAddr.call.varargsPush = 0;
            previousAddr.call.returnAddress = (BytecodeAddress)bs->Position();

            if (data->func.m_flags & (uint8)MethodFlags::VARIADIC)
            {
                // for each argument that is over the expected size, we must pop it from
                // the stack and add it to a new array.
                int varargsAmt = nargs - data->func.m_nargs + 1;
                if (varargsAmt < 0)
                {
                    varargsAmt = 0;
                }

                // set varargsPush value so we know how to get back to the stack size before.
                previousAddr.call.varargsPush = varargsAmt - 1;

                // create an array to hold variadic args
                ScriptArray arr;
                arr.Resize(varargsAmt);

                for (int i = varargsAmt - 1; i >= 0; i--)
                {
                    // push to array
                    arr[i] = std::move(instance->thread.GetStack().Top());
                    instance->thread.GetStack().Pop();
                }

                // push the array to the stack
                instance->thread.GetStack().Push(MakeValue(std::move(arr)));
            }

            // push the address
            instance->thread.GetStack().Push(MakeValue(previousAddr));

            // seek to the new address
            instance->stream.Seek((uint32)data->func.m_addr);

            // increase function depth
            instance->thread.m_funcDepth++;
        }

        return;
    }

    char buffer[256];
    std::snprintf(
        buffer,
        HYP_ARRAY_SIZE(buffer),
        "cannot invoke type '%s' as a function",
        GetTypeString(value));

    ThrowException(instance, Exception(buffer));
}

void VirtualMachine::InvokeImmediate(ScriptInstance* instance, BoxedValue&& value, uint8 nargs)
{
    BytecodeStream* bs = &instance->stream;

    const size_t positionBefore = bs->Position();
    const uint32 originalFunctionDepth = instance->thread.m_funcDepth;
    const size_t stackSizeBefore = instance->thread.GetStack().GetStackPointer();

    InstructionHandler handler(this, instance);

    BoxedValue* deref = Deref(value);
    Assert(deref != nullptr);

    ScriptObjectData* pData = GetVMData(*deref);
    Assert(pData != nullptr);
    Assert(pData->type == ScriptObjectData::Type::ScriptFunction || pData->type == ScriptObjectData::Type::NativeFunction);

    ScriptObjectData data = *pData;

    Invoke(instance, std::move(value), nargs);

    if (data.type == ScriptObjectData::Type::ScriptFunction && !handler.instance->thread.GetExceptionState().HasExceptionOccurred())
    { // don't do this for native function calls
        ubyte code;

        while (!bs->Eof())
        {
            bs->Read(&code);

            HandleInstruction(instance, &handler, code);

            if (code == RET)
            {
                if (instance->thread.m_funcDepth == originalFunctionDepth)
                {
                    break;
                }
            }
        }
    }

    // Unhandled exception - we need to reset the stack to what it was before.
    if (handler.instance->thread.GetExceptionState().HasExceptionOccurred())
    {
        instance->thread.m_exceptionState.m_exceptionDepth = 0;

        Assert(instance->thread.GetStack().GetStackPointer() >= stackSizeBefore);
        instance->thread.GetStack().Pop(instance->thread.GetStack().GetStackPointer() - stackSizeBefore);

        bs->SetPosition(positionBefore);
    }
    else if (bs->Position() != positionBefore)
    {
        // Exception was handled inside a try block -- HandleException already
        // sought the stream to the catch address. Do NOT reset to positionBefore.
        // The stream is now positioned past the catch block if it was already
        // consumed by the loop above, or at the catch address if the loop was
        // skipped. Either way, let the caller continue from the current position.
    }
    else
    {
        bs->SetPosition(positionBefore);
    }
}

void VirtualMachine::CreateTrace(ScriptInstance* instance, Script_Trace* outTrace)
{
    const size_t maxStackTraceSize = std::size(outTrace->callAddresses);

    for (int& callAddress : outTrace->callAddresses)
    {
        callAddress = -1;
    }

    size_t numRecordedCallAddresses = 0;

    for (size_t sp = instance->thread.m_stack.GetStackPointer(); sp != 0; sp--)
    {
        if (numRecordedCallAddresses >= maxStackTraceSize)
        {
            break;
        }

        const BoxedValue& top = instance->thread.m_stack[sp - 1];

        const ScriptObjectData* topVmData = GetVMData(top);

        if (topVmData && topVmData->type == ScriptObjectData::Type::StackFrame)
        {
            outTrace->callAddresses[numRecordedCallAddresses++] = int(topVmData->call.returnAddress);
        }
    }
}

bool VirtualMachine::HandleException(ScriptInstance* instance)
{
    if (instance->thread.m_exceptionState.m_tryCounter != 0)
    {
        // handle exception
        --instance->thread.m_exceptionState.m_tryCounter;

        Assert(instance->thread.m_exceptionState.m_exceptionDepth != 0);
        --instance->thread.m_exceptionState.m_exceptionDepth;

        BoxedValue* top = &instance->thread.m_stack.Top();
        ScriptObjectData* topVmData = GetVMData(*top);

        while (topVmData && topVmData->type != ScriptObjectData::Type::ExceptionState)
        {
            if (topVmData->type == ScriptObjectData::Type::StackFrame)
            {
                --instance->thread.m_funcDepth;
            }

            instance->thread.m_stack.Pop();

            top = &instance->thread.m_stack.Top();
            topVmData = GetVMData(*top);
        }

        // top should be exception data
        Assert(topVmData && topVmData->type == ScriptObjectData::Type::ExceptionState);

        // jump to the catch block
        instance->stream.Seek((uint32)topVmData->exceptionState.catchAddress);

        // pop exception data from stack
        instance->thread.m_stack.Pop();

        return true;
    }
    else
    {
        Script_Trace trace;
        CreateTrace(instance, &trace);

        std::cout << "trace = \n";

        for (auto callAddress : trace.callAddresses)
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

void VirtualMachine::Execute(ScriptInstance* instance)
{
    Assert(instance != nullptr);

    InstructionHandler handler(this, instance);

    BytecodeStream* bs = &instance->stream;

    ubyte code;

    while (!bs->Eof())
    {
        bs->Read(&code);

        HandleInstruction(instance, &handler, code);

        if (handler.instance->thread.GetExceptionState().HasExceptionOccurred())
        {
            HandleException(instance);

            if (m_unhandledException)
            {
                DebugLog(LogType::Error, "Unhandled exception, stopping execution...\n");

                break;
            }
        }
    }
}

#pragma endregion VirtualMachine

} // namespace Hyperion

#ifdef SCRIPT_INLINE
#undef SCRIPT_INLINE
#endif

#ifdef HYP_SCRIPT_NOOPT
#undef HYP_SCRIPT_NOOPT
#endif
