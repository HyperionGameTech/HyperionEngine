#include <script/vm/Interpreter.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/String.hpp>
#include <script/vm/HashMap.hpp>
#include <script/vm/GC.hpp>
#include <script/vm/Exception.hpp>

#include <core/reflection/HypData.hpp>
#include <core/reflection/Class.hpp>
#include <core/reflection/HypMember.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/Property.hpp>
#include <core/reflection/Method.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/reflection/TypeInfo.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/debug/Debug.hpp>
#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <script/Instructions.hpp>

#include <iostream>

// Enable to disable optimizations in script operations.
// Makes it easier to debug scripts, but slower.
#define HYP_SCRIPT_NOOPT 0

#ifndef HYP_DEBUG_MODE
#ifdef HYP_SCRIPT_NOOPT
#undef HYP_SCRIPT_NOOPT // disable noopt in release builds
#endif
#endif

#if defined(HYP_SCRIPT_NOOPT) && HYP_SCRIPT_NOOPT
#define SCRIPT_INLINE
#else
#define SCRIPT_INLINE HYP_FORCE_INLINE
#endif

#include <script/vm/inl/Interpreter.inl>

namespace hyperion {

using ScriptArray = Array<HypData, DynamicAllocator>;

extern const char* LookupTypeName(const TypeId& typeId);

using RegisterIndex = uint8;

HYP_API extern Pool* g_scriptPool;

static inline void* ScriptAlloc(SizeType size, SizeType alignment = 1)
{
    return g_scriptPool->Allocate(size, alignment);
}

static inline void ScriptFree(void* ptr)
{
    g_scriptPool->Free(ptr);
}

#pragma region ScriptApi

static const Name s_nameToString = NAME("ToString");

static const String s_nullString = "null";
static const String s_boolStrings[2] = { "false", "true" };

static const TypeId s_typeIdI8 = TypeId::ForType<int8>();
static const TypeId s_typeIdI16 = TypeId::ForType<int16>();
static const TypeId s_typeIdI32 = TypeId::ForType<int32>();
static const TypeId s_typeIdI64 = TypeId::ForType<int64>();
static const TypeId s_typeIdU8 = TypeId::ForType<uint8>();
static const TypeId s_typeIdU16 = TypeId::ForType<uint16>();
static const TypeId s_typeIdU32 = TypeId::ForType<uint32>();
static const TypeId s_typeIdU64 = TypeId::ForType<uint64>();
static const TypeId s_typeIdF32 = TypeId::ForType<float32>();
static const TypeId s_typeIdF64 = TypeId::ForType<float64>();
static const TypeId s_typeIdBool = TypeId::ForType<bool>();
static const TypeId s_typeIdString = TypeId::ForType<Script_String>();

// clang-format off
static const HashMap<TypeId, String (*)(const void*)> s_builtinToStringFunctions = {
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
    { s_typeIdString, [](const void* p) -> String { return *reinterpret_cast<const Script_String*>(p); } }
};
// clang-format on

static inline Script_VMData* GetVMData(HypData& data)
{
    return reinterpret_cast<Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet());
}

static inline const Script_VMData* GetVMData(const HypData& data)
{
    return reinterpret_cast<const Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet());
}

template <class T, typename = std::enable_if_t<!std::is_same_v<Script_VMData, NormalizedType<T>> && !std::is_same_v<Number, NormalizedType<T>> && !std::is_same_v<HypData, NormalizedType<T>>>>
static inline HypData ScriptApi_MakeValue(T&& data)
{
    return HypData(HypData(std::forward<T>(data)));
}

HypData ScriptApi_MakeValue(HypData&& data)
{
    return HypData(std::move(data));
}

HypData ScriptApi_MakeValue(const Script_VMData& data)
{
    static_assert(sizeof(Script_VMData) <= sizeof(HypData_UserData128), "Script_VMData must fit inside HypData_UserData128");
    static_assert(alignof(Script_VMData) <= alignof(HypData_UserData128), "Script_VMData must have alignment less than or equal to HypData_UserData128");

    HypData_UserData128 ud;
    Memory::MemCpy(&ud, &data, sizeof(Script_VMData));

    return HypData(ud);
}

HypData ScriptApi_MakeValue(const Number& number)
{
    ValueStorage<HypData> resultStorage;
    HypData* ptr = resultStorage.GetPointer();

    if (number.flags & Number::FLAG_FLOATING_POINT)
    {
        if (number.flags & Number::FLAG_32_BIT)
        {
            new (ptr) HypData(static_cast<float>(number.f));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (ptr) HypData(number.f);
        }
    }
    else if (number.flags & Number::FLAG_SIGNED)
    {
        if (number.flags & Number::FLAG_8_BIT)
        {
            new (ptr) HypData(static_cast<int8>(number.i));
        }
        else if (number.flags & Number::FLAG_16_BIT)
        {
            new (ptr) HypData(static_cast<int16>(number.i));
        }
        else if (number.flags & Number::FLAG_32_BIT)
        {
            new (ptr) HypData(static_cast<int32>(number.i));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (ptr) HypData(number.i);
        }
    }
    else if (number.flags & Number::FLAG_UNSIGNED)
    {
        if (number.flags & Number::FLAG_8_BIT)
        {
            new (ptr) HypData(static_cast<uint8>(number.u));
        }
        else if (number.flags & Number::FLAG_16_BIT)
        {
            new (ptr) HypData(static_cast<uint16>(number.u));
        }
        else if (number.flags & Number::FLAG_32_BIT)
        {
            new (ptr) HypData(static_cast<uint32>(number.u));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (ptr) HypData(number.u);
        }
    }
    else
    {
        HYP_UNREACHABLE();
    }

    return reinterpret_cast<HypData&&>(*ptr);
}

/*! \brief Use for loading into registers - does not promote to tracked memory so the lifetime of `refValue` must be managed by the caller */
HypData ScriptApi_MakeRef(HypData* pValue)
{
    Assert(pValue != nullptr);

    Script_VMData vmData;
    vmData.type = Script_VMData::VALUE_REF;
    vmData.valueRef = pValue;

    Assert(vmData.valueRef != nullptr);
    Assert(!IsGarbage(*vmData.valueRef), "Creating a reference to garbage value");

    return ScriptApi_MakeValue(vmData);
}

HypData ScriptApi_MakeTrackedRef(HypData* pValue, Script_GC* gc)
{
    Assert(gc != nullptr);
    Assert(pValue != nullptr);

    if (pValue->extData.scriptGcIndex != INVALID_GC_INDEX)
    {
        // already in tracked memory, make a reference to this value
        return ScriptApi_MakeRef(pValue);
    }

    const TypeId originalTypeId = pValue->GetTypeId();

    gc->MoveToTrackedMemory(*pValue);

    return *pValue;
}

#define PASS_AS_REF(value) ((value).Is<Any>())

// Performs a shallow copy of the value. Numeric and primitive types are copied as-is.
HypData ScriptApi_ShallowCopy(HypData& refValue, Script_GC* gc)
{
    if (IsRef(refValue))
    {
        return refValue; // already a reference, return as-is
    }

    if (refValue.extData.scriptGcIndex != INVALID_GC_INDEX)
    {
        // in tracked memory, make a reference to it
        return ScriptApi_MakeRef(&refValue);
    }

    HypData newHypData;

    Visit(refValue.value, [&newHypData](const auto& val)
        {
            newHypData.value.Set<NormalizedType<decltype(val)>>(val);
        });

    return newHypData;
}

bool ScriptApi_ShouldValuePassByRef(const HypData& value)
{
    if (!value.IsValid())
    {
        return false;
    }

    return PASS_AS_REF(value);
}

static const char s_unknownTypeString[] = "<Unknown type>";

const char* ScriptApi_GetTypeString(TypeId typeId)
{
    if (typeId == TypeId::ForType<int8>())
    {
        return "int8";
    }
    else if (typeId == TypeId::ForType<int16>())
    {
        return "int16";
    }
    else if (typeId == TypeId::ForType<int32>())
    {
        return "int32";
    }
    else if (typeId == TypeId::ForType<int64>())
    {
        return "int64";
    }
    else if (typeId == TypeId::ForType<uint8>())
    {
        return "uint8";
    }
    else if (typeId == TypeId::ForType<uint16>())
    {
        return "uint16";
    }
    else if (typeId == TypeId::ForType<uint32>())
    {
        return "uint32";
    }
    else if (typeId == TypeId::ForType<uint64>())
    {
        return "uint64";
    }
    else if (typeId == TypeId::ForType<float32>())
    {
        return "float";
    }
    else if (typeId == TypeId::ForType<float64>())
    {
        return "double";
    }
    else if (typeId == TypeId::ForType<bool>())
    {
        return "bool";
    }
    else if (typeId == TypeId::ForType<Script_String>())
    {
        return "string";
    }
    else if (typeId == TypeId::ForType<ScriptArray>())
    {
        return "array";
    }

    const char* typeName = LookupTypeName(typeId);

    if (typeName != nullptr)
    {
        return typeName;
    }

    return s_unknownTypeString;
}

const char* ScriptApi_GetTypeString(const HypData& data)
{
    if (!data.IsValid())
    {
        return "<Uninitialized data>";
    }

#if 0
    if (const Script_VMData* vmData = reinterpret_cast<const Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet()))
    {
        switch (vmData->type)
        {
        case Script_VMData::FUNCTION: // fallthrough
        case Script_VMData::NATIVE_FUNCTION:
            return "Function";
        case Script_VMData::ADDRESS:
            return "<Function address>";
        case Script_VMData::FUNCTION_CALL:
            return "<Stack frame>";
        case Script_VMData::TRY_CATCH_INFO:
            return "<Try catch info>";
        case Script_VMData::USER_DATA:
            return "UserData";
        case Script_VMData::VALUE_REF:
            return "Reference";
        default:
            HYP_UNREACHABLE();
        }
    }
#endif

    const TypeId typeId = data.GetTypeId();

    const char* typeIdString = ScriptApi_GetTypeString(typeId);

    if (typeIdString && typeIdString != s_unknownTypeString)
    {
        return typeIdString;
    }

    return s_unknownTypeString;
}

bool ScriptApi_StringifyData(const HypData& data, Script_String& outString, int maxDepth, int currDepth);

bool ScriptApi_StringifyData(const HypData& data, Script_String& outString, int maxDepth, int currDepth)
{
    if (currDepth >= maxDepth && maxDepth >= 0)
    {
        outString = Script_String("...");
        return true;
    }

    constexpr SizeType StringBufferSize = 256;

    char buf[StringBufferSize] = { 0 };

    if (!data.IsValid())
    {
        outString = Script_String(s_nullString);

        return true;
    }

    auto formatIt = s_builtinToStringFunctions.Find(data.GetTypeId());
    if (formatIt != s_builtinToStringFunctions.End())
    {
        outString = Script_String(formatIt->second(data.ToRef().GetPointer()));

        return true;
    }

    if (GenericArrayWrapper* pArray = data.TryGet<GenericArrayWrapper>().TryGet())
    {
        if (pArray->CanGetElementByIndex())
        {
            outString = "[";

            for (SizeType i = 0; i < pArray->Size(); i++)
            {
                if (i > 0)
                {
                    outString += Script_String(", ");
                }

                HypData element;
                if (pArray->GetElementAt(i, element))
                {
                    outString += ScriptApi_ValueToString(element, currDepth + 1);
                }
                else
                {
                    outString += Script_String("<Error accessing element>");
                }
            }

            outString += "]";
        }
        else
        {
            outString = Script_String("Array" + HYP_FORMAT(" (size = {})", pArray->Size()));
        }

        return true;
    }

#if 0
    if (const Script_HashMap* pMap = data.TryGet<Script_HashMap>().TryGet())
    {
        auto& map = pMap->GetMap();

        outString = "{";

        int i = 0;

        for (auto& kv : map)
        {
            if (i > 0)
            {
                outString += Script_String(", ");
            }
            outString += ScriptApi_ValueToString(*kv.first.key.GetHypData(), currDepth + 1);
            outString += " => ";
            outString += ScriptApi_ValueToString(*kv.second.GetHypData(), currDepth + 1);
            i++;
        }

        return true;
    }
#endif

    if (const Class* cls = GetClass(data.GetTypeId()))
    {
        const Method* toStringMethod = cls->GetMethod(s_nameToString);

        if (toStringMethod != nullptr)
        {
            HypData result = toStringMethod->Invoke(Span<HypData> { const_cast<HypData*>(&data), 1 });

            if (const Script_String* str = result.TryGet<Script_String>().TryGet())
            {
                outString = *str;

                return true;
            }

            if (const String* str = result.TryGet<String>().TryGet())
            {
                outString = Script_String(*str);

                return true;
            }

            // not a string, try again recursively
            if (ScriptApi_StringifyData(result, outString, maxDepth, currDepth + 1))
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
            data.ToRef().GetPointer());

        // if the class name is too long, dynamically allocate a larger buffer
        if (n < 0)
        {
            outString = Script_String("<Error formatting object>");

            return true;
        }

        if (SizeType(n) >= StringBufferSize)
        {
            const SizeType newBufSize = SizeType(n) + 1;

            char* newBuf = static_cast<char*>(Memory::Allocate(newBufSize));
            Assert(newBuf != nullptr);

            n = std::snprintf(
                newBuf,
                newBufSize,
                ObjectFormatString,
                cls->GetName().LookupString(),
                data.ToRef().GetPointer());

            if (n < 0 || static_cast<SizeType>(n) >= newBufSize)
            {
                Memory::Free(newBuf);

                outString = Script_String("<Error formatting object>");

                return true;
            }

            Script_String result(newBuf);
            Memory::Free(newBuf);

            outString = result;

            return true;
        }

        outString = Script_String(buf);

        return true;
    }

    return false;
}

String ScriptApi_ValueToString(const HypData& data, int currDepth)
{
    static constexpr int MaxDepth = 3;

    Script_String result("<error>");
    if (ScriptApi_StringifyData(data, result, MaxDepth, currDepth))
    {
        return result;
    }

    // internal data
    if (const Script_VMData* vmData = reinterpret_cast<const Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet()))
    {
        constexpr SizeType bufSize = 256;
        char buf[bufSize] = { 0 };

        switch (vmData->type)
        {
        case Script_VMData::VALUE_REF:
            return Script_String("<Reference>");
        case Script_VMData::FUNCTION:
            return Script_String("<Function>");
        case Script_VMData::NATIVE_FUNCTION:
            return Script_String("<Native Function>");
        case Script_VMData::ADDRESS:
            std::snprintf(buf, bufSize, "<Function address @ %p>", (void*)vmData->func.m_addr);
            return Script_String(buf);
        case Script_VMData::FUNCTION_CALL:
            return Script_String("<Stack frame>");
        case Script_VMData::TRY_CATCH_INFO:
            return Script_String("<Try catch info>");
        case Script_VMData::INVALID_STATE_OBJECT:
            return Script_String("<Invalid>");
        default:
            HYP_UNREACHABLE();
        }
    }

    return Script_String(HYP_FORMAT("<{} @ {}>", LookupTypeName(data.GetTypeId()), data.ToRef().GetPointer()));
}

#pragma endregion ScriptApi

#pragma region Script_RegisterMemory

Script_RegisterMemory::Script_RegisterMemory()
{
}

#pragma endregion Script_RegisterMemory

#pragma region Script_StaticMemory

const uint16 Script_StaticMemory::staticSize = 2048;

Script_StaticMemory::Script_StaticMemory()
    : m_data(new HypData[staticSize])
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
    Script_Interpreter* vm;
    Script_Instance* instance;

    InstructionHandler(
        Script_Interpreter* vm,
        Script_Instance* instance)
        : vm(vm),
          instance(instance)
    {
        Assert(vm != nullptr);
        Assert(instance != nullptr);
    }

    SCRIPT_INLINE void OpLoadI32(RegisterIndex reg, int32 i32)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(i32);
    }

    SCRIPT_INLINE void OpLoadI64(RegisterIndex reg, int64 i64)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(i64);
    }

    SCRIPT_INLINE void OpLoadU32(RegisterIndex reg, uint32 u32)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(u32);
    }

    SCRIPT_INLINE void OpLoadU64(RegisterIndex reg, uint64 u64)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(u64);
    }

    SCRIPT_INLINE void OpLoadF32(RegisterIndex reg, float32 f32)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(f32);
    }

    SCRIPT_INLINE void OpLoadF64(RegisterIndex reg, float64 f64)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(f64);
    }

    SCRIPT_INLINE void OpLoadOffset(RegisterIndex reg, uint16 offset)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            offset <= stackMemory.GetStackPointer(),
            "Stack offset out of bounds (%u)",
            offset);

        HypData& srcValue = stackMemory[stackMemory.GetStackPointer() - offset];

        // read value from stack at (sp - offset)
        // into the the register
        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadIndex(RegisterIndex reg, uint16 index)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stackMemory.GetStackPointer());

        HypData& srcValue = stackMemory[index];

        // read value from stack at the index into the the register
        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadStatic(RegisterIndex reg, uint16 index)
    {
        // read value from static memory
        // at the index into the the register
        HypData& srcValue = vm->m_staticMemory[index];

        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadConstantString(RegisterIndex reg, uint32 len, const char* str)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(str != nullptr ? Script_String(str, str + len) : Script_String());
    }

    SCRIPT_INLINE void OpLoadAddr(RegisterIndex reg, Script_FunctionAddress addr)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::ADDRESS;
        vmData.addr = addr;

        instance->thread.m_regs[reg] = ScriptApi_MakeValue(vmData);
    }

    SCRIPT_INLINE void OpLoadFunc(RegisterIndex reg, Script_FunctionAddress addr, uint8 nargs, uint8 flags)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::FUNCTION;
        vmData.func.m_addr = addr;
        vmData.func.m_nargs = nargs;
        vmData.func.m_flags = flags;

        instance->thread.m_regs[reg] = ScriptApi_MakeValue(vmData);
    }

    SCRIPT_INLINE void OpLoadArrayIdx(RegisterIndex dstReg, RegisterIndex srcReg, RegisterIndex indexReg)
    {
        HypData& src = *Deref(instance->thread.m_regs[srcReg]);

        Number key;

        if (!GetSignedOrUnsigned(instance->thread.m_regs[indexReg], &key))
        {
            vm->ThrowException(instance, Script_Exception("Array index must be an integral type"));

            return;
        }

        if (ScriptArray* array = src.TryGet<ScriptArray>().TryGet())
        {
            if (key.flags & Number::FLAG_SIGNED)
            {
                if (key.i < 0)
                {
                    // wrap around (python style)
                    key.u = SizeType(array->Size() - SizeType(-key.i));
                    if (key.u >= array->Size())
                    {
                        vm->ThrowException(instance, Script_Exception::OutOfBoundsException(key.u, array->Size()));

                        return;
                    }
                }

                if (SizeType(key.i) >= array->Size())
                {
                    vm->ThrowException(instance, Script_Exception::OutOfBoundsException(SizeType(key.i), array->Size()));
                    return;
                }

                HypData& srcValue = (*array)[key.i];

                instance->thread.m_regs[dstReg] = PASS_AS_REF(srcValue)
                    ? ScriptApi_MakeRef(&srcValue)
                    : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
            }
            else if (key.flags & Number::FLAG_UNSIGNED)
            {
                if (key.u >= array->Size())
                {
                    vm->ThrowException(instance, Script_Exception::OutOfBoundsException(key.u, array->Size()));

                    return;
                }

                HypData& srcValue = (*array)[key.u];

                instance->thread.m_regs[dstReg] = PASS_AS_REF(srcValue)
                    ? ScriptApi_MakeRef(&srcValue)
                    : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
            }

            return;
        }

        // throw an exception
        vm->ThrowException(instance, Script_Exception::InvalidOperationException("Indexing", GetTypeString(src)));
    }

    SCRIPT_INLINE void OpLoadOffsetRef(RegisterIndex reg, uint16 offset)
    {
        // load reference to stack value at (sp - offset) into the register
        HypData newRef = ScriptApi_MakeTrackedRef(Deref(instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - offset]), vm->GetGC());
        instance->thread.m_regs[reg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadIndexRef(RegisterIndex reg, uint16 index)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stackMemory.GetStackPointer());

        HypData newRef = ScriptApi_MakeTrackedRef(Deref(stackMemory[index]), vm->GetGC());
        instance->thread.m_regs[reg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadRef(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        HypData newRef = ScriptApi_MakeTrackedRef(Deref(instance->thread.m_regs[srcReg]), vm->GetGC());
        instance->thread.m_regs[dstReg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadDeref(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        HypData& src = *Deref(instance->thread.m_regs[srcReg]); // double deref to get the actual value
        instance->thread.m_regs[dstReg] = ScriptApi_ShallowCopy(*Deref(src), vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadNull(RegisterIndex reg)
    {
        instance->thread.m_regs[reg] = HypData();
    }

    SCRIPT_INLINE void OpLoadTrue(RegisterIndex reg)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(true);
    }

    SCRIPT_INLINE void OpLoadFalse(RegisterIndex reg)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(false);
    }

    SCRIPT_INLINE void OpLoadClass(RegisterIndex reg, uint64 nameHash)
    {
        Name name = Name(StringHash(nameHash));
        const Class* cls = ClassRegistry::GetInstance().GetClass(name);
        if (!cls)
        {
            vm->ThrowException(instance, Script_Exception::ClassNotFoundException(name.LookupString()));

            return;
        }

        HypData classValue = ScriptApi_MakeValue(HypData(ClassRef(cls)));

        instance->thread.m_regs[reg] = std::move(classValue);
    }

    SCRIPT_INLINE void OpMovOffset(uint16 offset, RegisterIndex reg)
    {
        // copy value from register to stack value at (sp - offset)
        AssignValue(instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - offset], ScriptApi_ShallowCopy(*Deref(instance->thread.m_regs[reg]), vm->GetGC()), true);
    }

    SCRIPT_INLINE void OpMovIndex(uint16 index, RegisterIndex reg)
    {
        // copy value from register to stack value at index
        AssignValue(instance->thread.m_stack[index], ScriptApi_ShallowCopy(*Deref(instance->thread.m_regs[reg]), vm->GetGC()), true);
    }

    SCRIPT_INLINE void OpMovStatic(uint16 index, RegisterIndex reg)
    {
        Assert(index < vm->m_staticMemory.staticSize);

        vm->m_staticMemory[index] = std::move(instance->thread.m_regs[reg]);
    }

    SCRIPT_INLINE void OpMovArrayIdx(RegisterIndex dstReg, uint32 index, RegisterIndex srcReg)
    {
        HypData& src = *Deref(instance->thread.m_regs[dstReg]);

        if (!src.Is<ScriptArray>())
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("Indexing", GetTypeString(src)));
            return;
        }

        ScriptArray& array = src.Get<ScriptArray>();

        if (index >= array.Size())
        {
            vm->ThrowException(instance, Script_Exception::OutOfBoundsException(SizeType(index), array.Size()));

            return;
        }

        HypData& srcValue = *Deref(instance->thread.m_regs[srcReg]);
        HypData& dstValue = array[index];

        dstValue = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpMovArrayIdxReg(RegisterIndex dstReg, RegisterIndex indexReg, RegisterIndex srcReg)
    {
        HypData& src = *Deref(instance->thread.m_regs[dstReg]);

        if (!src.Is<ScriptArray>())
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("Indexing", GetTypeString(src)));
            return;
        }

        ScriptArray& array = src.Get<ScriptArray>();

        Number index;
        HypData& indexRegisterValue = instance->thread.m_regs[indexReg];

        if (!GetSignedOrUnsigned(indexRegisterValue, &index))
        {
            vm->ThrowException(instance, Script_Exception::InvalidArgsException("integer"));

            return;
        }

        if (index.flags & Number::FLAG_SIGNED)
        {
            int64 indexValue = index.i;

            if (indexValue < 0)
            {
                // wrap around (python style)
                SizeType uIndexValue = SizeType(array.Size() - SizeType(-indexValue));

                if (uIndexValue >= array.Size())
                {
                    vm->ThrowException(instance, Script_Exception::OutOfBoundsException(uIndexValue, array.Size()));

                    return;
                }
            }

            if (SizeType(indexValue) >= array.Size())
            {
                vm->ThrowException(instance, Script_Exception::OutOfBoundsException(SizeType(indexValue), array.Size()));

                return;
            }

            HypData& srcValue = *Deref(instance->thread.m_regs[srcReg]);
            HypData& dstValue = array[indexValue];

            dstValue = PASS_AS_REF(srcValue)
                ? ScriptApi_MakeRef(&srcValue)
                : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
        }
        else
        { // unsigned
            const uint64 indexValue = index.u;

            if (SizeType(indexValue) >= array.Size())
            {
                vm->ThrowException(instance, Script_Exception::OutOfBoundsException(indexValue, array.Size()));

                return;
            }

            HypData& srcValue = *Deref(instance->thread.m_regs[srcReg]);
            HypData& dstValue = array[indexValue];

            dstValue = PASS_AS_REF(srcValue)
                ? ScriptApi_MakeRef(&srcValue)
                : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
        }
    }

    SCRIPT_INLINE void OpMov(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        instance->thread.m_regs[dstReg] = std::move(instance->thread.m_regs[srcReg]);
    }

    SCRIPT_INLINE void OpCheckHasMember(RegisterIndex dstReg, RegisterIndex srcReg, uint64 hash)
    {
        HypData& src = *Deref(instance->thread.m_regs[srcReg]);
        HypData& result = instance->thread.m_regs[dstReg];

        const Class* cls = nullptr;

        if (const AnyHandle& object = ScriptApi_GetObject(src))
        {
            cls = object.ptr->InstanceClass();
        }
        else
        {
            cls = GetClass(src.GetTypeId());
        }

        if (cls != nullptr)
        {
            IHypMember* member = cls->GetMember(StringHash(NameID(hash)));
            result = ScriptApi_MakeValue(member != nullptr);

            return;
        }

        result = ScriptApi_MakeValue(false);
    }

    SCRIPT_INLINE void OpSetField(RegisterIndex dstReg, uint64 hash, RegisterIndex srcReg)
    {
        HypData* pValue = Deref(instance->thread.m_regs[dstReg]);

        const Class* cls = nullptr;

        if (const AnyHandle& object = ScriptApi_GetObject(*pValue))
        {
            cls = object.ptr->InstanceClass();
        }
        else
        {
            cls = GetClass(pValue->GetTypeId());
        }

        if (!cls)
        {
            vm->ThrowException(instance, Script_Exception::InvalidMemberAccessException(pValue));

            return;
        }

        Field* field = cls->GetField(StringHash(NameID(hash)));

        if (!field)
        {
            vm->ThrowException(instance, Script_Exception::MemberNotFoundException(pValue, hash));

            return;
        }

        field->Set(*pValue, *Deref(instance->thread.m_regs[srcReg]));
    }

    SCRIPT_INLINE void OpGetMember(RegisterIndex dstReg, RegisterIndex srcReg, uint64 hash)
    {
        HypData& src = *Deref(instance->thread.m_regs[srcReg]);

        const Class* cls = nullptr;

        if (const AnyHandle& object = ScriptApi_GetObject(src))
        {
            // instance member access
            cls = object.ptr->InstanceClass();
        }
        else if (ClassRef* classRef = src.TryGet<ClassRef>().TryGet())
        {
            // static member access on class reference
            cls = *classRef;
        }
        // temp special case for arrays
        else if (GenericArrayWrapper* array = src.TryGet<GenericArrayWrapper>().TryGet())
        {
            cls = GetClass(TypeId::ForType<ScriptArray>());
        }
        else
        {
            cls = GetClass(src.GetTypeId());
        }

        if (!cls)
        {
            vm->ThrowException(instance, Script_Exception::InvalidMemberAccessException(&src));

            return;
        }

        IHypMember* member = cls->GetMember(StringHash(NameID(hash)));
        if (!member)
        {
            vm->ThrowException(instance, Script_Exception::MemberNotFoundException(&src, hash));

            return;
        }

        if (member->GetMemberType() == HypMemberType::TYPE_FIELD)
        {
            Field* field = static_cast<Field*>(member);

            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(field->Get(src));
        }
        else if (member->GetMemberType() == HypMemberType::TYPE_STATIC_FIELD)
        {
            StaticField* staticField = static_cast<StaticField*>(member);

            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(staticField->Get());
        }
        else if (member->GetMemberType() == HypMemberType::TYPE_METHOD)
        {
            Method* method = static_cast<Method*>(member);

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

            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(vmData);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception("Member is not a field or method"));
        }
    }

    SCRIPT_INLINE void OpPush(RegisterIndex reg)
    {
        // Move value from register to top of stack
        instance->thread.m_stack.Push(ScriptApi_ShallowCopy(*Deref(instance->thread.m_regs[reg]), vm->GetGC()));
    }

    SCRIPT_INLINE void OpPop()
    {
        instance->thread.m_stack.Pop();
    }

    SCRIPT_INLINE void OpPushArray(RegisterIndex dstReg, RegisterIndex srcReg)
    {
        HypData& dst = *Deref(instance->thread.m_regs[dstReg]);

        if (!dst.Is<ScriptArray>())
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("PUSH_ARRAY", GetTypeString(dst)));
            return;
        }

        ScriptArray& array = dst.Get<ScriptArray>();

        array.PushBack(ScriptApi_ShallowCopy(*Deref(instance->thread.m_regs[srcReg]), vm->GetGC()));
    }

    SCRIPT_INLINE void OpAddSp(uint16 n)
    {
        instance->thread.m_stack.m_sp += n;
    }

    SCRIPT_INLINE void OpSubSp(uint16 n)
    {
        instance->thread.m_stack.Pop(n);
    }

    SCRIPT_INLINE void OpJmp(Script_FunctionAddress addr)
    {
        instance->stream.Seek((uint32)addr);
    }

    SCRIPT_INLINE void OpJe(Script_FunctionAddress addr)
    {
        if (instance->thread.m_regs.flags & CF_EQUAL)
        {
            instance->stream.Seek((uint32)addr);
        }
    }

    SCRIPT_INLINE void OpJne(Script_FunctionAddress addr)
    {
        if (!(instance->thread.m_regs.flags & CF_EQUAL))
        {
            instance->stream.Seek((uint32)addr);
        }
    }

    SCRIPT_INLINE void OpJg(Script_FunctionAddress addr)
    {
        if (instance->thread.m_regs.flags & CF_GREATER)
        {
            instance->stream.Seek((uint32)addr);
        }
    }

    SCRIPT_INLINE void OpJge(Script_FunctionAddress addr)
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
        HypData& top = instance->thread.GetStack().Top();

        Script_VMData* vmData = GetVMData(top);
        Assert(vmData != nullptr);
        Assert(vmData->type == Script_VMData::FUNCTION_CALL);

        auto& callInfo = vmData->call;

        // leave function and return to previous position
        instance->stream.Seek((uint32)callInfo.returnAddress);

        // increase stack size by the amount required by the call
        instance->thread.GetStack().m_sp += callInfo.varargsPush - 1;
        // NOTE: the -1 is because we will be popping the FUNCTION_CALL
        // object from the stack anyway...

        // decrease function depth
        instance->thread.m_funcDepth--;
    }

    SCRIPT_INLINE void OpBeginTry(Script_FunctionAddress addr)
    {
        ++instance->thread.m_exceptionState.m_tryCounter;

        // increase stack size to store data about this try block
        Script_VMData vmData;
        vmData.type = Script_VMData::TRY_CATCH_INFO;
        vmData.tryCatchInfo.catchAddress = addr;

        // store the info
        instance->thread.m_stack.Push(ScriptApi_MakeValue(vmData));
    }

    SCRIPT_INLINE void OpEndTry()
    {
        // pop the try catch info from the stack
        HypData& top = instance->thread.m_stack.Top();

        Script_VMData* vmData = GetVMData(top);
        Assert(vmData != nullptr);
        Assert(vmData->type == Script_VMData::TRY_CATCH_INFO);

        Assert(instance->thread.m_exceptionState.m_tryCounter != 0);

        // pop try catch info
        instance->thread.m_stack.Pop();
        --instance->thread.m_exceptionState.m_tryCounter;
    }

    SCRIPT_INLINE void OpNew(RegisterIndex dst, RegisterIndex src) // come back to this
    {
        // read value from register
        HypData& classValue = *Deref(instance->thread.m_regs[src]);

        const ClassRef& classRef = classValue.Get<ClassRef>();
        Assert(classRef.IsValid());

        HypData hypData;
        if (!classRef->CreateInstance(hypData))
        {
            vm->ThrowException(
                instance,
                Script_Exception::InvalidOperationException(
                    "NEW",
                    "Could not create instance of type",
                    classRef->GetName().LookupString()));

            return;
        }

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(std::move(hypData));
    }

    SCRIPT_INLINE void OpNewArray(RegisterIndex dst, uint32 size)
    {
        // assign register value to the allocated object
        instance->thread.m_regs[dst] = ScriptApi_MakeValue(ScriptArray(size));
    }

    SCRIPT_INLINE void OpBeginClass(RegisterIndex reg)
    {
        Script_Stream* bs = &instance->stream;

        // Read class name length and name
        uint16 nameLen;
        bs->Read(&nameLen);

        char* nameStr = (char*)ScriptAlloc(nameLen + 1);
        nameStr[nameLen] = '\0';
        bs->Read(nameStr, nameLen);

        // Create a new class with the given name
        Name className = CreateNameFromDynamicString(nameStr);
        ScriptFree(nameStr);

        // Read type id
        TypeId::ValueType typeIdValue;
        bs->Read(&typeIdValue);

        uint8 flags;
        bs->Read(&flags);

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

                char* memberNameStr = (char*)ScriptAlloc(memberNameLen + 1);
                memberNameStr[memberNameLen] = '\0';
                bs->Read(memberNameStr, memberNameLen);

                // Read attributes
                uint16 numAttrs;
                bs->Read(&numAttrs);

                Array<ClassAttribute> attrs;
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

                        Array<char> strData;
                        strData.Resize(strLen + 1);
                        strData[strLen] = '\0';

                        bs->Read(strData.Data(), strLen);

                        attr.value = ClassAttributeValue(String(strData.Begin(), strData.End()));

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
                case HypMemberType::TYPE_STATIC_FIELD:
                {
                    // static field

                    uint32 size;
                    bs->Read(&size);

                    // Create constant
                    members.PushBack(HypMember(StaticField(
                        CreateNameFromDynamicString(memberNameStr),
                        &TypeInfo::ForType<HypData>(), // TypeId(memberTypeIdValue),
                        size,
                        attrs.ToSpan())));

                    break;
                }
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
                    members.PushBack(HypMember(Field(
                        CreateNameFromDynamicString(memberNameStr),
                        &TypeInfo::ForType<HypData>(),    // TypeId(memberTypeIdValue),
                        &TypeInfo::ForType<ObjectBase>(), // TypeId(targetTypeIdValue),
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
                    Assert(stackOffset <= instance->thread.GetStack().GetStackPointer(), "Stack offset out of bounds!");
                    HypData& funcValue = instance->thread.GetStack()[instance->thread.GetStack().GetStackPointer() - stackOffset];

                    Script_VMData* funcVmData = GetVMData(funcValue);
                    Assert(funcVmData != nullptr);
                    Assert(funcVmData->type == Script_VMData::FUNCTION);

                    Script_FunctionAddress functionAddress = funcVmData->func.m_addr;
                    Assert(functionAddress != INVALID_FUNCTION_ADDRESS);

                    Method method(
                        CreateNameFromDynamicString(memberNameStr),
                        &TypeInfo::ForType<HypData>(),    // TypeId(memberTypeIdValue),
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
                        method.GetParameters().PushBack(MethodParameter { &TypeInfo::ForType<HypData>() });
                    }

                    members.PushBack(HypMember(std::move(method)));

                    break;
                }
                default:
                    HYP_NOT_IMPLEMENTED();
                    break;
                }

                ScriptFree(memberNameStr);
            }
        }

        Assert(hitEnd);

        // Read parent class register
        HypData& parentClassValue = instance->thread.m_regs[reg];

        const Class* parentClass = nullptr;

        if (parentClassValue.IsValid())
        {
            parentClass = parentClassValue.Get<ClassRef>();
            Assert(parentClass != nullptr);
        }

        // some type needs to be set
        Assert((flags & (uint8)(ClassFlags::CLASS_TYPE | ClassFlags::STRUCT_TYPE | ClassFlags::ENUM_TYPE)) != 0);

        DynamicClassInstance* newClass = new DynamicClassInstance(
            TypeId(typeIdValue),
            className,
            parentClass,
            Span<const ClassAttribute>(), // @TODO
            (ClassFlags)flags,
            members.ToSpan());

        ClassRegistry::GetInstance().RegisterClass(newClass->GetTypeId(), newClass);

        HypData classValue = ScriptApi_MakeValue(ClassRef(newClass));

        // promote the class object to tracked gc memory so it doesn't instantly get destroyed
        instance->thread.m_regs[reg] = ScriptApi_MakeTrackedRef(&classValue, vm->GetGC());
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
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

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
            else
            {
                const int res = CompareAsPointers(*lhs, *rhs);

                if (res != -1)
                {
                    instance->thread.m_regs.flags = res;
                }
                else
                {
                    vm->ThrowException(instance, Script_Exception::InvalidComparisonException(GetTypeString(*lhs), GetTypeString(*rhs)));
                }
            }
        }
    }

    SCRIPT_INLINE void OpCmpZ(RegisterIndex reg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[reg]);

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
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, +);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("ADD", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpSub(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, -);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("SUB", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpMul(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, *);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("MUL", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpDiv(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                vm->ThrowException(instance, Script_Exception::DivisionByZeroException());

                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                vm->ThrowException(instance, Script_Exception::DivisionByZeroException());

                return;
            }

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, /);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("DIV", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpMod(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            // custom handling for mod to allow floats to work
            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                vm->ThrowException(instance, Script_Exception::DivisionByZeroException());

                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                vm->ThrowException(instance, Script_Exception::DivisionByZeroException());

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
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("MOD", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpAnd(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, &);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("AND", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpOr(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, |);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("OR", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpXor(
        RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = MATCH_TYPES(GetNumericType(*lhs), GetNumericType(*rhs));

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, ^);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("XOR", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpShl(RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = GetNumericType(*lhs);

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, <<);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("SHL", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpShr(RegisterIndex lhsReg,
        RegisterIndex rhsReg,
        RegisterIndex dstReg)
    {
        // load values from registers
        HypData* lhs = Deref(instance->thread.m_regs[lhsReg]);
        HypData* rhs = Deref(instance->thread.m_regs[rhsReg]);

        Number a, b;

        if (GetNumber(*lhs, &a) && GetNumber(*rhs, &b))
        {
            const NumericType numericType = GetNumericType(*lhs);

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, >>);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("SHR", GetTypeString(*lhs), GetTypeString(*rhs)));
        }
    }

    SCRIPT_INLINE void OpNot(RegisterIndex reg)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[reg]);

        Number num;

        // we only allow bitwise NOT on integers
        if (GetNumber(value, &num) && (num.flags & (Number::FLAG_SIGNED | Number::FLAG_UNSIGNED)))
        {
            // signedness and bitwidth don't change result
            num.u = ~num.u;
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidBitwiseArgument());

            return;
        }

        instance->thread.m_regs[reg] = ScriptApi_MakeValue(num);
    }

    SCRIPT_INLINE void OpThrow(RegisterIndex reg)
    {
        // load value from register
        HypData* value = Deref(instance->thread.m_regs[reg]);

        // @TODO Allow throwing the arugment

        vm->ThrowException(instance, Script_Exception("User exception"));
    }

    SCRIPT_INLINE void OpExportSymbol(RegisterIndex reg, uint64 hash)
    {
        HypData& srcValue = *Deref(instance->thread.m_regs[reg]);

        HypData newValue = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeTrackedRef(&srcValue, vm->GetGC())
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());

        if (!instance->exportedSymbols.Store(hash, std::move(newValue)).second)
        {
            vm->ThrowException(instance, Script_Exception::DuplicateExportException());
        }
    }

    SCRIPT_INLINE void OpNeg(RegisterIndex reg)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[reg]);

        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("NEG", GetTypeString(value)));

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

        instance->thread.m_regs[reg] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU8(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);

        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "uint8"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU16(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);

        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "uint16"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU32(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "uint32"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastU64(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "uint64"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI8(RegisterIndex dst, RegisterIndex src)
    {
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "int8"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI16(RegisterIndex dst, RegisterIndex src)
    {
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "int16"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI32(RegisterIndex dst, RegisterIndex src)
    {
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "int32"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastI64(RegisterIndex dst, RegisterIndex src)
    {
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "int64"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastF32(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "float32"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastF64(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);
        Number num;

        if (!GetNumber(value, &num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "float64"));

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastBool(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastString(RegisterIndex dst, RegisterIndex src)
    {
        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);

        const Script_String* pString = nullptr;

        if (!GetString(value, &pString))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), "string"));

            return;
        }

        instance->thread.m_regs[dst] = ScriptApi_ShallowCopy(value, vm->GetGC());
    }

    SCRIPT_INLINE void OpCastDynamic(RegisterIndex dst, RegisterIndex src)
    {
        // dst register holds ClassRef object
        HypData& classValue = *Deref(instance->thread.m_regs[dst]);

        const ClassRef& classRef = classValue.Get<ClassRef>();
        Assert(classRef.IsValid());

        // load value from register
        HypData& value = *Deref(instance->thread.m_regs[src]);

        const Class* cls = nullptr;

        if (const AnyHandle& object = ScriptApi_GetObject(value))
        {
            cls = object.ptr->InstanceClass();
        }
        else
        {
            cls = GetClass(value.GetTypeId());
        }

        if (!cls || !cls->IsDerivedFrom(classRef))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(GetTypeString(value), classRef->GetName().LookupString()));

            return;
        }

        instance->thread.m_regs[dst] = ScriptApi_ShallowCopy(value, vm->GetGC());
    }
};

SCRIPT_INLINE static void HandleInstruction(
    Script_Instance* instance,
    InstructionHandler* handler,
    ubyte code)
{
    Script_Stream* bs = &instance->stream;

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
            switch (dataType)
            {
            case DTYPE_I32:
            {
                int32_t value;
                bs->Read(&value);
                handler->OpLoadI32(reg, value);
            }
            break;

            case DTYPE_I64:
            {
                int64_t value;
                bs->Read(&value);
                handler->OpLoadI64(reg, value);
            }
            break;

            case DTYPE_U32:
            {
                uint32 value;
                bs->Read(&value);
                handler->OpLoadU32(reg, value);
            }
            break;

            case DTYPE_U64:
            {
                uint64 value;
                bs->Read(&value);
                handler->OpLoadU64(reg, value);
            }
            break;

            case DTYPE_F32:
            {
                float32 value;
                bs->Read(&value);
                handler->OpLoadF32(reg, value);
            }
            break;

            case DTYPE_F64:
            {
                float64 value;
                bs->Read(&value);
                handler->OpLoadF64(reg, value);
            }
            break;

            case DTYPE_BOOL:
            {
                uint8 value;
                bs->Read(&value);
                if (value)
                    handler->OpLoadTrue(reg);
                else
                    handler->OpLoadFalse(reg);
            }
            break;

            case DTYPE_OBJECT:
                // Load null for immediate object
                handler->OpLoadNull(reg);
                break;
            }
            break;

        case LSRC_OFFSET:
        {
            uint16 offset;
            bs->Read(&offset);

            if (isRef)
                handler->OpLoadOffsetRef(reg, offset);
            else
                handler->OpLoadOffset(reg, offset);
        }
        break;

        case LSRC_INDEX:
        {
            uint16 index;
            bs->Read(&index);

            if (isRef)
                handler->OpLoadIndexRef(reg, index);
            else
                handler->OpLoadIndex(reg, index);
        }
        break;

        case LSRC_STATIC:
        {
            uint16 index;
            bs->Read(&index);
            handler->OpLoadStatic(reg, index);
        }
        break;

        case LSRC_ARRAYIDX:
        {
            RegisterIndex arrayReg;
            bs->Read(&arrayReg);
            RegisterIndex indexReg;
            bs->Read(&indexReg);
            handler->OpLoadArrayIdx(reg, arrayReg, indexReg);
        }
        break;

        case LSRC_MEMBER:
        {
            RegisterIndex objReg;
            bs->Read(&objReg);
            uint64 hash;
            bs->Read(&hash);
            handler->OpGetMember(reg, objReg, hash);
        }
        break;

        case LSRC_REGISTER:
        {
            RegisterIndex srcReg;
            bs->Read(&srcReg);

            if (isRef)
                handler->OpLoadRef(reg, srcReg);
            else
                handler->OpLoadDeref(reg, srcReg);
        }
        break;

        case LSRC_ADDRESS:
        {
            Script_FunctionAddress addr;
            bs->Read(&addr);

            handler->OpLoadAddr(reg, addr);
        }
        break;
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
    }

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
            handler->OpCastDynamic(dstReg, srcReg);
            break;
        default:
            HYP_UNREACHABLE();
        }

        break;
    }
    case LOAD_OFFSET:
    {
        RegisterIndex reg;
        bs->Read(&reg);
        uint16 offset;
        bs->Read(&offset);

        handler->OpLoadOffset(
            reg,
            offset);

        break;
    }
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

        handler->OpLoadConstantString(
            reg,
            len,
            str);

        ScriptFree(str);

        break;
    }
    case LOAD_ARRAYIDX:
    {
        RegisterIndex dstReg;
        bs->Read(&dstReg);

        RegisterIndex srcReg;
        bs->Read(&srcReg);

        RegisterIndex indexReg;
        bs->Read(&indexReg);

        handler->OpLoadArrayIdx(
            dstReg,
            srcReg,
            indexReg);

        break;
    }
    case LOAD_OFFSET_REF:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint16 offset;
        bs->Read(&offset);

        handler->OpLoadOffsetRef(reg, offset);

        break;
    }
    case LOAD_FUNC:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        Script_FunctionAddress addr;
        bs->Read(&addr);

        uint8 nargs;
        bs->Read(&nargs);

        uint8 flags;
        bs->Read(&flags);

        handler->OpLoadFunc(reg, addr, nargs, flags);

        break;
    }
    case LOAD_CLASS:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint64 nameHash;
        bs->Read(&nameHash);

        handler->OpLoadClass(reg, nameHash);

        break;
    }
    case REF:
    {
        RegisterIndex dstReg;
        RegisterIndex srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler->OpLoadRef(dstReg, srcReg);

        break;
    }
    case DEREF:
    {
        RegisterIndex dstReg;
        RegisterIndex srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler->OpLoadDeref(dstReg, srcReg);

        break;
    }
    case MOV_OFFSET:
    {
        uint16 offset;
        bs->Read(&offset);

        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpMovOffset(offset, reg);

        break;
    }
    case MOV_INDEX:
    {
        uint16 index;
        bs->Read(&index);
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpMovIndex(index, reg);

        break;
    }
    case MOV_STATIC:
    {
        uint16 index;
        bs->Read(&index);

        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpMovStatic(index, reg);

        break;
    }
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
    }
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
    }
    case MOV:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpMov(dst, src);

        break;
    }
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
    }
    case PUSH:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpPush(reg);

        break;
    }
    case POP:
    {
        handler->OpPop();

        break;
    }
    case PUSH_ARRAY:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpPushArray(dst, src);

        break;
    }
    case ADD_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler->OpAddSp(val);

        break;
    }
    case SUB_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler->OpSubSp(val);

        break;
    }
    case JMP:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler->OpJmp(addr);

        break;
    }
    case JE:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler->OpJe(addr);

        break;
    }
    case JNE:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler->OpJne(addr);

        break;
    }
    case JG:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler->OpJg(addr);

        break;
    }
    case JGE:
    {
        Script_FunctionAddress addr;
        bs->Read(&addr);

        handler->OpJge(addr);

        break;
    }
    case CALL:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint8 nargs;
        bs->Read(&nargs);

        handler->OpCall(reg, nargs);

        break;
    }
    case RET:
    {
        handler->OpRet();

        break;
    }
    case BEGIN_TRY:
    {
        Script_FunctionAddress catchAddress;
        bs->Read(&catchAddress);

        handler->OpBeginTry(catchAddress);

        break;
    }
    case END_TRY:
    {
        handler->OpEndTry();

        break;
    }
    case NEW:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        RegisterIndex src;
        bs->Read(&src);

        handler->OpNew(dst, src);

        break;
    }
    case NEW_ARRAY:
    {
        RegisterIndex dst;
        bs->Read(&dst);

        uint32 size;
        bs->Read(&size);

        handler->OpNewArray(dst, size);

        break;
    }
    case CMP:
    {
        RegisterIndex lhsReg;
        bs->Read(&lhsReg);

        RegisterIndex rhsReg;
        bs->Read(&rhsReg);

        handler->OpCmp(lhsReg, rhsReg);

        break;
    }
    case BEGIN_CLASS:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpBeginClass(reg);

        break;
    }
    case CMPZ:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpCmpZ(reg);

        break;
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
    case NEG:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpNeg(reg);

        break;
    }
    case NOT:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpNot(reg);

        break;
    }
    case THROW:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        handler->OpThrow(reg);

        break;
    }
    case TRACEMAP:
    {
        uint32 len;
        bs->Read(&len);

        uint32 stringmapCount;
        bs->Read(&stringmapCount);

        Script_Tracemap::StringmapEntry* stringmap = nullptr;

        if (stringmapCount != 0)
        {
            stringmap = new Script_Tracemap::StringmapEntry[stringmapCount];

            for (uint32 i = 0; i < stringmapCount; i++)
            {
                bs->Read(&stringmap[i].entryType);
                bs->ReadZeroTerminatedString(stringmap[i].data);
            }
        }

        uint32 linemapCount;
        bs->Read(&linemapCount);

        Script_Tracemap::LinemapEntry* linemap = nullptr;

        if (linemapCount != 0)
        {
            linemap = new Script_Tracemap::LinemapEntry[linemapCount];
            bs->Read(linemap, sizeof(Script_Tracemap::LinemapEntry) * linemapCount);
        }

        handler->vm->m_tracemap.Set(stringmap, linemap);

        break;
    }
    case BINDATA:
    {
        RegisterIndex reg;
        bs->Read(&reg);

        uint32 len;
        bs->Read(&len);

        ByteBuffer buffer(len, /* zeroize */ false);
        bs->Read(buffer.Data(), len);

        FBOMReader reader { FBOMReaderConfig {} };
        FBOMLoadContext ctx;

        MemoryBufferedReaderSource source { buffer };
        BufferedReader bufferedReader { &source };

        HypData result;
        if (FBOMResult err = reader.Deserialize(ctx, bufferedReader, result))
        {
            // throw exception for invalid data:
            handler->vm->ThrowException(instance, Script_Exception(err.message.Data()));

            break;
        }

        handler->instance->thread.m_regs[reg] = ScriptApi_MakeValue(std::move(result));

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
        RegisterIndex reg;
        bs->Read(&reg);
        uint64 hash;
        bs->Read(&hash);

        handler->OpExportSymbol(reg, hash);

        break;
    }
    default:
    {
        int64 lastPos = int64(bs->Position()) - sizeof(ubyte);
        HYP_FAIL("unknown instruction '{}' referenced at location {}", code, lastPos);
        // seek to end of bytecode stream
        instance->stream.Seek(bs->Size());

        return;
    }
    }
}

#pragma endregion InstructionHandler

#pragma region Script_Interpreter

Script_Interpreter::Script_Interpreter()
    : m_unhandledException(nullptr)
{
    m_gc = new Script_GC();
}

Script_Interpreter::~Script_Interpreter()
{
    delete m_unhandledException;
    delete m_gc;
}

void Script_Interpreter::ThrowException(Script_Instance* instance, const Script_Exception& exception)
{
    ++instance->thread.m_exceptionState.m_exceptionDepth;

    if (instance->thread.m_exceptionState.m_tryCounter == 0)
    {
        // exception cannot be handled, no try block found
        if (instance->thread.m_id == 0)
        {
            DebugLog(LogType::Error, "unhandled exception in main thread: %s", exception.ToString());
        }
        else
        {
            DebugLog(LogType::Error, "unhandled exception in thread %d: %s", instance->thread.m_id, exception.ToString());
        }

        m_unhandledException = new Script_Exception(exception);
    }
}

void Script_Interpreter::Invoke(Script_Instance* instance, HypData&& value, uint8 nargs)
{
    Script_ExecutionThread* thread = &instance->thread;
    Script_Stream* bs = &instance->stream;

    HypData& deref = *Deref(value);

    if (IsFunction(deref))
    {
        if (IsNativeFunction(deref))
        {
            HypData** argsHypData = (HypData**)StackAlloc((nargs > 0 ? nargs : 1) * sizeof(HypData*));

            for (int argIndex = 0; argIndex < nargs; argIndex++)
            {
                HypData& srcValue = *Deref(instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - int(nargs) + argIndex]);

                argsHypData[argIndex] = &srcValue;
            }

            // @TODO: Implement
            // disable auto gc so no collections happen during a native function
            //            enableAutoGc = false;

            // call the native function
            Script_VMData* vmData = GetVMData(deref);
            Assert(vmData != nullptr && vmData->nativeFunc != nullptr);

            if (vmData->nativeFunc->GetFlags() & MethodFlags::MEMBER)
            {
                AssertDebug(nargs >= 1);

                if (argsHypData[0]->IsNull())
                {
                    // throw exception if target is null
                    ThrowException(instance, Script_Exception::NullReferenceException());
                    return;
                }
            }

            HypData resultHypData = vmData->nativeFunc->Invoke(Span<HypData*>(argsHypData, nargs));

            // set register 0 to the result
            instance->thread.GetRegisters()[0] = ScriptApi_MakeValue(std::move(resultHypData));

            // re-enable auto gc
            //            enableAutoGc = ENABLE_GC;

            return;
        }

        // non-native function here
        Script_VMData* vmData = GetVMData(deref);
        Assert(vmData != nullptr && vmData->type == Script_VMData::FUNCTION);

        if ((vmData->func.m_flags & (uint8)MethodFlags::VARIADIC) && nargs < vmData->func.m_nargs - 1)
        {
            // if variadic, make sure the arg count is /at least/ what is required
            ThrowException(instance, Script_Exception::InvalidArgsException(vmData->func.m_nargs, nargs, true));
        }
        else if (!(vmData->func.m_flags & (uint8)MethodFlags::VARIADIC) && vmData->func.m_nargs != nargs)
        {
            ThrowException(instance, Script_Exception::InvalidArgsException(vmData->func.m_nargs, nargs));
        }
        else
        {
            Script_VMData previousAddr;
            previousAddr.type = Script_VMData::FUNCTION_CALL;
            previousAddr.call.varargsPush = 0;
            previousAddr.call.returnAddress = (Script_FunctionAddress)bs->Position();

            if (vmData->func.m_flags & (uint8)MethodFlags::VARIADIC)
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
                instance->thread.GetStack().Push(ScriptApi_MakeValue(std::move(arr)));
            }

            // push the address
            instance->thread.GetStack().Push(ScriptApi_MakeValue(previousAddr));

            // seek to the new address
            instance->stream.Seek((uint32)vmData->func.m_addr);

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

    ThrowException(instance, Script_Exception(buffer));
}

void Script_Interpreter::InvokeNow(Script_Instance* instance, HypData&& value, uint8 nargs)
{
    Script_ExecutionThread* thread = &instance->thread;
    Script_Stream* bs = &instance->stream;

    const SizeType positionBefore = bs->Position();
    const uint32 originalFunctionDepth = instance->thread.m_funcDepth;
    const SizeType stackSizeBefore = instance->thread.GetStack().GetStackPointer();

    InstructionHandler handler(this, instance);

    HypData* deref = Deref(value);
    Assert(deref != nullptr);

    Script_VMData* pVmData = GetVMData(*deref);
    Assert(pVmData != nullptr);
    Assert(pVmData->type == Script_VMData::FUNCTION || pVmData->type == Script_VMData::NATIVE_FUNCTION);

    Script_VMData vmData = *pVmData;

    Invoke(instance, std::move(value), nargs);

    if (handler.instance->thread.GetExceptionState().HasExceptionOccurred())
    {
        if (!HandleException(instance))
        {
            instance->thread.m_exceptionState.m_exceptionDepth = 0;

            Assert(instance->thread.GetStack().GetStackPointer() >= stackSizeBefore);
            instance->thread.GetStack().Pop(instance->thread.GetStack().GetStackPointer() - stackSizeBefore);

            return;
        }
    }

    if (vmData.type == Script_VMData::FUNCTION)
    { // don't do this for native function calls
        ubyte code;

        while (!bs->Eof())
        {
            bs->Read(&code);

            HandleInstruction(instance, &handler, code);

            if (handler.instance->thread.GetExceptionState().HasExceptionOccurred())
            {
                if (!HandleException(instance))
                {
                    instance->thread.m_exceptionState.m_exceptionDepth = 0;

                    Assert(instance->thread.GetStack().GetStackPointer() >= stackSizeBefore);
                    instance->thread.GetStack().Pop(instance->thread.GetStack().GetStackPointer() - stackSizeBefore);

                    break;
                }
            }

            if (code == RET)
            {
                if (instance->thread.m_funcDepth == originalFunctionDepth)
                {
                    break;
                }
            }
        }
    }

    bs->SetPosition(positionBefore);
}

void Script_Interpreter::CreateTrace(Script_Instance* instance, Script_Trace* outTrace)
{
    const SizeType maxStackTraceSize = std::size(outTrace->callAddresses);

    for (int& callAddress : outTrace->callAddresses)
    {
        callAddress = -1;
    }

    SizeType numRecordedCallAddresses = 0;

    for (SizeType sp = instance->thread.m_stack.GetStackPointer(); sp != 0; sp--)
    {
        if (numRecordedCallAddresses >= maxStackTraceSize)
        {
            break;
        }

        const HypData& top = instance->thread.m_stack[sp - 1];

        const Script_VMData* topVmData = GetVMData(top);

        if (topVmData && topVmData->type == Script_VMData::FUNCTION_CALL)
        {
            outTrace->callAddresses[numRecordedCallAddresses++] = int(topVmData->call.returnAddress);
        }
    }
}

bool Script_Interpreter::HandleException(Script_Instance* instance)
{
    Script_ExecutionThread* thread = &instance->thread;
    Script_Stream* bs = &instance->stream;

    if (instance->thread.m_exceptionState.m_tryCounter != 0)
    {
        // handle exception
        --instance->thread.m_exceptionState.m_tryCounter;

        Assert(instance->thread.m_exceptionState.m_exceptionDepth != 0);
        --instance->thread.m_exceptionState.m_exceptionDepth;

        HypData* top = &instance->thread.m_stack.Top();
        Script_VMData* topVmData = GetVMData(*top);

        while (topVmData && topVmData->type != Script_VMData::TRY_CATCH_INFO)
        {
            instance->thread.m_stack.Pop();

            top = &instance->thread.m_stack.Top();
            topVmData = GetVMData(*top);
        }

        // top should be exception data
        Assert(topVmData && topVmData->type != Script_VMData::TRY_CATCH_INFO);

        // jump to the catch block
        instance->stream.Seek((uint32)topVmData->tryCatchInfo.catchAddress);

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

void Script_Interpreter::Execute(Script_Instance* instance)
{
    Assert(instance != nullptr);

    InstructionHandler handler(this, instance);

    Script_Stream* bs = &instance->stream;

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

#pragma endregion Script_Interpreter

} // namespace hyperion

#ifdef SCRIPT_INLINE
#undef SCRIPT_INLINE
#endif

#ifdef HYP_SCRIPT_NOOPT
#undef HYP_SCRIPT_NOOPT
#endif