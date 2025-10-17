#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/String.hpp>
#include <script/vm/HashMap.hpp>

#include <core/reflection/HypData.hpp>
#include <core/reflection/HypClass.hpp>
#include <core/reflection/HypMethod.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

#include <stdio.h>
#include <cinttypes>
#include <iostream>

namespace hyperion {

extern HYP_API const char* LookupTypeName(TypeId typeId);

extern const char* ScriptApi_GetTypeString(const HypData& data);
extern String ScriptApi_ValueToString(const HypData& data, int currDepth = 0);

static const String g_nullString = "null";
static const String g_referenceString = "<reference>";

static const TypeId g_typeIdI8 = TypeId::ForType<int8>();
static const TypeId g_typeIdI16 = TypeId::ForType<int16>();
static const TypeId g_typeIdI32 = TypeId::ForType<int32>();
static const TypeId g_typeIdI64 = TypeId::ForType<int64>();
static const TypeId g_typeIdU8 = TypeId::ForType<uint8>();
static const TypeId g_typeIdU16 = TypeId::ForType<uint16>();
static const TypeId g_typeIdU32 = TypeId::ForType<uint32>();
static const TypeId g_typeIdU64 = TypeId::ForType<uint64>();
static const TypeId g_typeIdF32 = TypeId::ForType<float32>();
static const TypeId g_typeIdF64 = TypeId::ForType<float64>();
static const TypeId g_typeIdBool = TypeId::ForType<bool>();
static const TypeId g_typeIdString = TypeId::ForType<Script_String>();

static inline ValueStorage<HypData> MakeGarbageValue()
{
    ValueStorage<HypData> storage;
    Memory::MemSet(&storage, 0xFFu, sizeof(HypData));

    return storage;
}

static const ValueStorage<HypData> s_uninitializedValue = MakeGarbageValue();

static inline Script_VMData* GetVMData(HypData& data)
{
    return reinterpret_cast<Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet());
}

static inline const Script_VMData* GetVMData(const HypData& data)
{
    return reinterpret_cast<const Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet());
}

HYP_API bool IsGarbage(const HypData& data)
{
    return data.extData.scriptGcIndex == GARBAGE_GC_INDEX;
}

HYP_API bool IsFunction(const HypData& data)
{
    const Script_VMData* vmData = GetVMData(data);

    if (!vmData)
    {
        return false;
    }

    return vmData->type == Script_VMData::FUNCTION || vmData->type == Script_VMData::NATIVE_FUNCTION;
}

HYP_API bool IsNativeFunction(const HypData& data)
{
    const Script_VMData* vmData = GetVMData(data);

    if (!vmData)
    {
        return false;
    }

    return vmData->type == Script_VMData::NATIVE_FUNCTION;
}

HYP_API bool IsRef(const HypData& data)
{
    const Script_VMData* vmData = GetVMData(data);

    if (!vmData)
    {
        return false;
    }

    return vmData->type == Script_VMData::VALUE_REF;
}

HYP_API HypData* GetRef(const HypData& data)
{
    const Script_VMData* vmData = GetVMData(data);

    if (!vmData || vmData->type != Script_VMData::VALUE_REF)
    {
        return nullptr;
    }

    return vmData->valueRef;
}

HYP_API HypData* Deref(HypData& data)
{
    HypData* deref = GetRef(data);

    if (deref != nullptr)
    {
        AssertDebug(!IsGarbage(*deref));

        return deref;
    }

    return &data;
}

const HypData* Deref(const HypData& data)
{
    HypData* deref = GetRef(data);

    if (deref != nullptr)
    {
        AssertDebug(!IsGarbage(*deref));

        return deref;
    }

    return &data;
}

HYP_API void AssignValue(HypData& data, HypData&& other, bool assignRef)
{
    HypData* ref;

    AssertDebug(other.extData.scriptGcIndex == INVALID_GC_INDEX);

    if (assignRef && (ref = Deref(data)) != nullptr)
    {
        GCIndex prevGcIndex = ref->extData.scriptGcIndex;
        ref->extData.scriptGcIndex = INVALID_GC_INDEX;

        ref->~HypData();
        new (ref) HypData(std::move(other));

        ref->extData.scriptGcIndex = prevGcIndex;
    }
    else
    {
        data.~HypData();

        new (&data) HypData(std::move(other));
    }
}

HYP_API bool GetUnsigned(const HypData& data, uint64* out)
{
    if (!data.Is<uint64>(/* strict */ false))
    {
        return false;
    }

    *out = data.Get<uint64>();

    return true;
}

HYP_API bool GetInteger(const HypData& data, int64* out)
{
    if (!data.Is<int64>(/* strict */ false))
    {
        return false;
    }

    *out = data.Get<int64>();

    return true;
}

HYP_API bool GetSignedOrUnsigned(const HypData& data, Number* out)
{
    const TypeId typeId = data.GetTypeId();

    if (typeId == g_typeIdI8)
    {
        out->i = static_cast<int64>(data.Get<int8>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdU8)
    {
        out->u = static_cast<uint64>(data.Get<uint8>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdI16)
    {
        out->i = static_cast<int64>(data.Get<int16>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdU16)
    {
        out->u = static_cast<uint64>(data.Get<uint16>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdI32)
    {
        out->i = static_cast<int64>(data.Get<int32>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdU32)
    {
        out->u = static_cast<uint64>(data.Get<uint32>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdI64)
    {
        out->i = data.Get<int64>();
        out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == g_typeIdU64)
    {
        out->u = data.Get<uint64>();
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;
        return true;
    }

    return false;
}

HYP_API bool GetFloatingPoint(const HypData& data, double* out)
{
    if (!data.Is<double>(/* strict */ true) && !data.Is<float>(/* strict */ true))
    {
        return false;
    }

    *out = data.Get<double>();

    return true;
}

HYP_API bool GetNumber(const HypData& data, double* out)
{
    Number number;
    if (!GetNumber(data, &number))
    {
        return false;
    }

    if (number.flags & Number::FLAG_FLOATING_POINT)
    {
        *out = number.f;

        return true;
    }

    if (number.flags & Number::FLAG_SIGNED)
    {
        *out = static_cast<double>(number.i);

        return true;
    }

    if (number.flags & Number::FLAG_UNSIGNED)
    {
        *out = static_cast<double>(number.u);

        return true;
    }

    return false;
}

HYP_API bool GetNumber(const HypData& data, Number* out)
{
    const TypeId typeId = data.GetTypeId();

    if (typeId == g_typeIdF32)
    {
        out->f = static_cast<double>(data.Get<float>());
        out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdF64)
    {
        out->f = data.Get<double>();
        out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == g_typeIdI8)
    {
        out->i = static_cast<int64>(data.Get<int8>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdU8)
    {
        out->u = static_cast<uint64>(data.Get<uint8>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdI16)
    {
        out->i = static_cast<int64>(data.Get<int16>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdU16)
    {
        out->u = static_cast<uint64>(data.Get<uint16>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdI32)
    {
        out->i = static_cast<int64>(data.Get<int32>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdU32)
    {
        out->u = static_cast<uint64>(data.Get<uint32>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdI64)
    {
        out->i = data.Get<int64>();
        out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == g_typeIdU64)
    {
        out->u = data.Get<uint64>();
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;
        return true;
    }

    return false;
}

HYP_API NumericType GetNumericType(const HypData& data)
{
    const TypeId typeId = data.GetTypeId();

    if (typeId == g_typeIdI8)
    {
        return NT_I8;
    }
    if (typeId == g_typeIdU8)
    {
        return NT_U8;
    }
    if (typeId == g_typeIdI16)
    {
        return NT_I16;
    }
    if (typeId == g_typeIdU16)
    {
        return NT_U16;
    }
    if (typeId == g_typeIdI32)
    {
        return NT_I32;
    }
    if (typeId == g_typeIdU32)
    {
        return NT_U32;
    }
    if (typeId == g_typeIdI64)
    {
        return NT_I64;
    }
    if (typeId == g_typeIdU64)
    {
        return NT_U64;
    }
    if (typeId == g_typeIdF32)
    {
        return NT_F32;
    }
    if (typeId == g_typeIdF64)
    {
        return NT_F64;
    }

    return NT_INVALID;
}

HYP_API bool GetBoolean(const HypData& data, bool* out)
{
    if (!data.Is<bool>())
    {
        return false;
    }

    *out = data.Get<bool>();
    return true;
}

HYP_API bool GetString(const HypData& data, const Script_String** out)
{
    AssertDebug(out != nullptr);

    if (!data.Is<Script_String>(true))
    {
        return false;
    }

    *out = &data.Get<Script_String>();

    return true;
}

HYP_API const AnyHandle& ScriptApi_GetObject(const HypData& data)
{
    if (!data.Is<AnyHandle>())
    {
        return AnyHandle::empty;
    }

    return data.Get<AnyHandle>();
}

HYP_API int CompareAsPointers(const HypData& lhs, const HypData& rhs)
{
    void* a = lhs.ToRef().GetPointer();
    void* b = rhs.ToRef().GetPointer();

    if (a == b)
    {
        // pointers equal, drop out early.
        return CF_EQUAL;
    }
    else if (a == nullptr || b == nullptr)
    {
        return CF_NONE;
    }
    else
    {
        return CF_NONE;
    }
}

HYP_API int CompareAsFunctions(const HypData& lhs, const HypData& rhs)
{
    const Script_VMData* lhsVmData = GetVMData(lhs);
    const Script_VMData* rhsVmData = GetVMData(rhs);

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CF_EQUAL : CF_NONE;
    }

    return (lhsVmData->func.m_addr == rhsVmData->func.m_addr)
        ? CF_EQUAL
        : CF_NONE;
}

HYP_API int CompareAsNativeFunctions(const HypData& lhs, const HypData& rhs)
{
    const Script_VMData* lhsVmData = GetVMData(lhs);
    const Script_VMData* rhsVmData = GetVMData(rhs);

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CF_EQUAL : CF_NONE;
    }

    return (lhsVmData->nativeFunc == rhsVmData->nativeFunc)
        ? CF_EQUAL
        : CF_NONE;
}

HYP_API const char* GetTypeString(const HypData& data)
{
    return ScriptApi_GetTypeString(data);
}

HYP_API String ToString(const HypData& data)
{
    if (IsRef(data))
    {
        if (HypData* ref = GetRef(data))
        {
            if (IsRef(*ref))
            {
                return Script_String(g_referenceString);
            }

            return ToString(*ref);
        }

        return Script_String(g_nullString);
    }

    return ScriptApi_ValueToString(data);
}

} // namespace hyperion
