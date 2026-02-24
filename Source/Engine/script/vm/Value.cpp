#include <script/vm/Value.hpp>
#include <script/vm/String.hpp>
#include <script/vm/Map.hpp>

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/Method.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/utilities/Format.hpp>

#include <stdio.h>
#include <cinttypes>
#include <iostream>

namespace Hyperion {

extern String ValueToString(const BoxedValue& value, int currDepth = 0);

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
static const TypeId g_typeIdString = TypeId::ForType<ScriptString>();

static inline ValueStorage<BoxedValue> MakeGarbageValue()
{
    ValueStorage<BoxedValue> storage;
    Memory::Fill(&storage, 0xFFu, sizeof(BoxedValue));

    return storage;
}

static const ValueStorage<BoxedValue> s_uninitializedValue = MakeGarbageValue();

static inline ScriptObjectData* GetVMData(BoxedValue& data)
{
    return reinterpret_cast<ScriptObjectData*>(data.TryGet<BoxedValue::InlineData>().TryGet());
}

static inline const ScriptObjectData* GetVMData(const BoxedValue& value)
{
    return reinterpret_cast<const ScriptObjectData*>(value.TryGet<BoxedValue::InlineData>().TryGet());
}

bool IsGarbage(const BoxedValue& value)
{
    return value.extData.gcIndex == GARBAGE_GC_INDEX;
}

bool IsFunction(const BoxedValue& value)
{
    const ScriptObjectData* data = GetVMData(value);

    if (!data)
    {
        return false;
    }

    return data->type == ScriptObjectData::Type::ScriptFunction || data->type == ScriptObjectData::Type::NativeFunction;
}

bool IsNativeFunction(const BoxedValue& value)
{
    const ScriptObjectData* data = GetVMData(value);

    if (!data)
    {
        return false;
    }

    return data->type == ScriptObjectData::Type::NativeFunction;
}

bool IsRef(const BoxedValue& value)
{
    const ScriptObjectData* data = GetVMData(value);

    if (!data)
    {
        return false;
    }

    return data->type == ScriptObjectData::Type::Reference;
}

BoxedValue* GetRef(const BoxedValue& value)
{
    const ScriptObjectData* data = GetVMData(value);

    if (!data || data->type != ScriptObjectData::Type::Reference)
    {
        return nullptr;
    }

    return data->valueRef;
}

BoxedValue* Deref(BoxedValue& value)
{
    BoxedValue* deref = GetRef(value);

    if (deref != nullptr)
    {
        AssertDebug(!IsGarbage(*deref));

        return deref;
    }

    return &value;
}

const BoxedValue* Deref(const BoxedValue& value)
{
    BoxedValue* deref = GetRef(value);

    if (deref != nullptr)
    {
        AssertDebug(!IsGarbage(*deref));

        return deref;
    }

    return &value;
}

void AssignValue(BoxedValue& value, BoxedValue&& other, bool assignRef)
{
    BoxedValue* ref;

    AssertDebug(other.extData.gcIndex == INVALID_GC_INDEX);

    if (assignRef && (ref = Deref(value)) != nullptr)
    {
        GCIndex prevGCIndex = ref->extData.gcIndex;
        ref->extData.gcIndex = INVALID_GC_INDEX;

        ref->~BoxedValue();
        new (ref) BoxedValue(std::move(other));

        ref->extData.gcIndex = prevGCIndex;
    }
    else
    {
        value.~BoxedValue();

        new (&value) BoxedValue(std::move(other));
    }
}

bool GetUnsigned(const BoxedValue& value, uint64* out)
{
    if (!value.Is<uint64>(/* strict */ false))
    {
        return false;
    }

    *out = value.Get<uint64>();

    return true;
}

bool GetInteger(const BoxedValue& value, int64* out)
{
    if (!value.Is<int64>(/* strict */ false))
    {
        return false;
    }

    *out = value.Get<int64>();

    return true;
}

bool GetSignedOrUnsigned(const BoxedValue& value, Number* out)
{
    const TypeId typeId = value.GetTypeId();

    if (typeId == g_typeIdI8)
    {
        out->i = static_cast<int64>(value.Get<int8>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdU8)
    {
        out->u = static_cast<uint64>(value.Get<uint8>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdI16)
    {
        out->i = static_cast<int64>(value.Get<int16>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdU16)
    {
        out->u = static_cast<uint64>(value.Get<uint16>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdI32)
    {
        out->i = static_cast<int64>(value.Get<int32>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdU32)
    {
        out->u = static_cast<uint64>(value.Get<uint32>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdI64)
    {
        out->i = value.Get<int64>();
        out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == g_typeIdU64)
    {
        out->u = value.Get<uint64>();
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;
        return true;
    }

    return false;
}

bool GetFloatingPoint(const BoxedValue& value, double* out)
{
    if (!value.Is<double>(/* strict */ true) && !value.Is<float>(/* strict */ true))
    {
        return false;
    }

    *out = value.Get<double>();

    return true;
}

bool GetNumber(const BoxedValue& value, double* out)
{
    Number number;
    if (!GetNumber(value, &number))
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

bool GetNumber(const BoxedValue& value, Number* out)
{
    const TypeId typeId = value.GetTypeId();

    if (typeId == g_typeIdF32)
    {
        out->f = static_cast<double>(value.Get<float>());
        out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdF64)
    {
        out->f = value.Get<double>();
        out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == g_typeIdI8)
    {
        out->i = static_cast<int64>(value.Get<int8>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdU8)
    {
        out->u = static_cast<uint64>(value.Get<uint8>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == g_typeIdI16)
    {
        out->i = static_cast<int64>(value.Get<int16>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdU16)
    {
        out->u = static_cast<uint64>(value.Get<uint16>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == g_typeIdI32)
    {
        out->i = static_cast<int64>(value.Get<int32>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdU32)
    {
        out->u = static_cast<uint64>(value.Get<uint32>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == g_typeIdI64)
    {
        out->i = value.Get<int64>();
        out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == g_typeIdU64)
    {
        out->u = value.Get<uint64>();
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;
        return true;
    }

    return false;
}

NumericType GetNumericType(const BoxedValue& value)
{
    const TypeId typeId = value.GetTypeId();

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

bool GetBoolean(const BoxedValue& value, bool* out)
{
    if (!value.Is<bool>())
    {
        return false;
    }

    *out = value.Get<bool>();
    return true;
}

bool GetString(const BoxedValue& value, const ScriptString** out)
{
    AssertDebug(out != nullptr);

    if (!value.Is<ScriptString>(true))
    {
        return false;
    }

    *out = &value.Get<ScriptString>();

    return true;
}

const Handle<ObjectBase>& GetObject(const BoxedValue& value)
{
    if (!value.Is<Handle<ObjectBase>>())
    {
        return Handle<ObjectBase>::empty;
    }

    return value.Get<Handle<ObjectBase>>();
}

int CompareAsPointers(const BoxedValue& lhs, const BoxedValue& rhs)
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

int CompareAsFunctions(const BoxedValue& lhs, const BoxedValue& rhs)
{
    const ScriptObjectData* lhsVmData = GetVMData(lhs);
    const ScriptObjectData* rhsVmData = GetVMData(rhs);

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CF_EQUAL : CF_NONE;
    }

    return (lhsVmData->func.m_addr == rhsVmData->func.m_addr)
        ? CF_EQUAL
        : CF_NONE;
}

int CompareAsNativeFunctions(const BoxedValue& lhs, const BoxedValue& rhs)
{
    const ScriptObjectData* lhsVmData = GetVMData(lhs);
    const ScriptObjectData* rhsVmData = GetVMData(rhs);

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CF_EQUAL : CF_NONE;
    }

    return (lhsVmData->nativeFunc == rhsVmData->nativeFunc)
        ? CF_EQUAL
        : CF_NONE;
}

String ToString(const BoxedValue& value)
{
    if (IsRef(value))
    {
        if (BoxedValue* ref = GetRef(value))
        {
            if (IsRef(*ref))
            {
                return ScriptString(g_referenceString);
            }

            return ToString(*ref);
        }

        return ScriptString(g_nullString);
    }

    return ValueToString(value);
}

} // namespace Hyperion
