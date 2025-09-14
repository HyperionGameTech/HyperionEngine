#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/String.hpp>
#include <script/vm/HashMap.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypMethod.hpp>

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

const Script_Value Script_Value::s_uninitializedValue = Script_Value(MAKE_GARBAGE_TAG);

Script_Value::Script_Value(MakeGarbageTag)
{
    Memory::MemSet(m_internal, 0xFFu, sizeof(m_internal));
    m_gcIndex = INVALID_GC_INDEX;
}

Script_Value::Script_Value()
    : m_gcIndex(INVALID_GC_INDEX)
{
    static_assert(sizeof(m_internal) == sizeof(HypData), "Size of m_internal must match size of HypData");
    static_assert(alignof(decltype(m_internal)) <= alignof(HypData), "Alignment of m_internal must be less than or equal to alignment of HypData");

    new (m_internal) HypData();
}

Script_Value::Script_Value(HypData&& data)
    : m_gcIndex(INVALID_GC_INDEX)
{
    new (m_internal) HypData(std::move(data));
}

Script_Value::Script_Value(Number number)
    : m_gcIndex(INVALID_GC_INDEX)
{
    if (number.flags & Number::FLAG_FLOATING_POINT)
    {
        if (number.flags & Number::FLAG_32_BIT)
        {
            new (m_internal) HypData(static_cast<float>(number.f));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (m_internal) HypData(number.f);
        }
    }
    else if (number.flags & Number::FLAG_SIGNED)
    {
        if (number.flags & Number::FLAG_8_BIT)
        {
            new (m_internal) HypData(static_cast<int8>(number.i));
        }
        else if (number.flags & Number::FLAG_16_BIT)
        {
            new (m_internal) HypData(static_cast<int16>(number.i));
        }
        else if (number.flags & Number::FLAG_32_BIT)
        {
            new (m_internal) HypData(static_cast<int32>(number.i));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (m_internal) HypData(number.i);
        }
    }
    else if (number.flags & Number::FLAG_UNSIGNED)
    {
        if (number.flags & Number::FLAG_8_BIT)
        {
            new (m_internal) HypData(static_cast<uint8>(number.u));
        }
        else if (number.flags & Number::FLAG_16_BIT)
        {
            new (m_internal) HypData(static_cast<uint16>(number.u));
        }
        else if (number.flags & Number::FLAG_32_BIT)
        {
            new (m_internal) HypData(static_cast<uint32>(number.u));
        }
        else // if (number.flags & Number::FLAG_64_BIT)
        {
            new (m_internal) HypData(number.u);
        }
    }
    else
    {
        HYP_UNREACHABLE();
    }
}

Script_Value::Script_Value(const Script_VMData& vmData)
    : m_gcIndex(INVALID_GC_INDEX)
{
    static_assert(sizeof(Script_VMData) == sizeof(HypData_UserData128));
    static_assert(alignof(Script_VMData) <= alignof(HypData_UserData128));

    HypData_UserData128 userData;
    Memory::MemCpy(&userData, &vmData, sizeof(Script_VMData));

    new (m_internal) HypData(userData);
}

Script_Value::Script_Value(const Script_Value& other)
    : m_gcIndex(INVALID_GC_INDEX)
{
    new (m_internal) HypData(*other.GetHypData());
}

Script_Value::Script_Value(Script_Value&& other) noexcept
    : m_gcIndex(INVALID_GC_INDEX)
{
    new (m_internal) HypData(std::move(*other.GetHypData()));
}

Script_Value& Script_Value::operator=(const Script_Value& other)
{
    if (this != &other)
    {
        GetHypData()->~HypData();
        new (m_internal) HypData(*other.GetHypData());
    }

    return *this;
}

Script_Value& Script_Value::operator=(Script_Value&& other) noexcept
{
    if (this != &other)
    {
        GetHypData()->~HypData();
        new (m_internal) HypData(std::move(*other.GetHypData()));
    }

    return *this;
}

Script_Value::~Script_Value()
{
    Assert(m_gcIndex == INVALID_GC_INDEX); // should not be destroyed if it is tracked by the Script_GC

    // have to manually call destructor because we used placement new
    GetHypData()->~HypData();

    // set all bytes to 0xFF to indicate garbage for debugging purposes
    Memory::MemSet(m_internal, 0xFFu, sizeof(m_internal));
}

void Script_Value::Mark()
{
    if (Script_Value* ref = GetRef())
    {
        ref->Mark();

        return;
    }

    // @TODO: Mark heap pointer
}

Script_VMData* Script_Value::GetVMData()
{
    HypData& data = *GetHypData();

    return reinterpret_cast<Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet());
}

const Script_VMData* Script_Value::GetVMData() const
{
    const HypData& data = *GetHypData();

    return reinterpret_cast<const Script_VMData*>(data.TryGet<HypData_UserData128>().TryGet());
}

bool Script_Value::IsValid() const
{
    return GetHypData()->IsValid();
}

bool Script_Value::IsGarbage() const
{
    return Memory::MemCmp(this, &s_uninitializedValue, sizeof(Script_Value)) == 0;
}

bool Script_Value::IsFunction() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr)
    {
        return false;
    }

    return vmData->type == Script_VMData::FUNCTION || vmData->type == Script_VMData::NATIVE_FUNCTION;
}

bool Script_Value::IsNativeFunction() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr)
    {
        return false;
    }

    return vmData->type == Script_VMData::NATIVE_FUNCTION;
}

bool Script_Value::IsRef() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr)
    {
        return false;
    }

    return vmData->type == Script_VMData::VALUE_REF;
}

Script_Value* Script_Value::GetRef() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr || vmData->type != Script_VMData::VALUE_REF)
    {
        return nullptr;
    }

    return vmData->valueRef; // shouldn't be a reference itself so no need to deref
}

Script_Value* Script_Value::Deref()
{
    Script_Value* deref = GetRef();
    if (deref != nullptr)
    {
        Assert(!deref->IsGarbage());

        return deref;
    }

    return this;
}

const Script_Value* Script_Value::Deref() const
{
    const Script_Value* deref = GetRef();

    if (deref != nullptr)
    {
        Assert(!deref->IsGarbage());

        return deref;
    }

    return this;
}

void Script_Value::AssignValue(Script_Value&& other, bool assignRef)
{
    Script_Value* ref;

    Assert(other.GetGCIndex() == INVALID_GC_INDEX);

    if (assignRef && (ref = Deref()) != nullptr)
    {
        HypData* hypData = ref->GetHypData();
        hypData->~HypData();

        new (hypData) HypData(std::move(*other.GetHypData()));
    }
    else
    {
        HypData* hypData = GetHypData();
        hypData->~HypData();

        new (hypData) HypData(std::move(*other.GetHypData()));
    }
}

bool Script_Value::GetUnsigned(uint64* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<uint64>(/* strict */ false))
    {
        return false;
    }

    *out = data.Get<uint64>();

    return true;
}

bool Script_Value::GetInteger(int64* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<int64>(/* strict */ false))
    {
        return false;
    }

    *out = data.Get<int64>();

    return true;
}

bool Script_Value::GetSignedOrUnsigned(Number* out) const
{
    const HypData& data = *GetHypData();

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

bool Script_Value::GetFloatingPoint(double* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<double>(/* strict */ true) && !data.Is<float>(/* strict */ true))
    {
        return false;
    }

    *out = data.Get<double>();

    return true;
}

bool Script_Value::GetFloatingPointCoerce(double* out) const
{
    // alias for backwards compatibility
    return GetNumber(out);
}

bool Script_Value::GetNumber(double* out) const
{
    Number number;
    if (!GetNumber(&number))
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

bool Script_Value::GetNumber(Number* out) const
{
    const HypData& data = *GetHypData();

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

NumericType Script_Value::GetNumericType() const
{
    const HypData& data = *GetHypData();
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

bool Script_Value::GetBoolean(bool* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<bool>())
    {
        return false;
    }

    *out = data.Get<bool>();
    return true;
}

bool Script_Value::GetString(const Script_String** out) const
{
    AssertDebug(out != nullptr);

    const HypData& data = *GetHypData();

    if (!data.Is<Script_String>(true))
    {
        return false;
    }

    *out = &data.Get<Script_String>();

    return true;
}

const AnyHandle& Script_Value::GetObject() const
{
    const HypData& data = *GetHypData();

    if (!data.Is<AnyHandle>())
    {
        return AnyHandle::empty;
    }

    return data.Get<AnyHandle>();
}

AnyRef Script_Value::ToRef() const
{
    const HypData& data = *GetHypData();
    return data.ToRef();
}

Script_UserData Script_Value::GetUserData() const
{
    const Script_VMData* vmData = GetVMData();

    if (!vmData || vmData->type != Script_VMData::USER_DATA)
    {
        return nullptr;
    }

    return vmData->userData;
}

int Script_Value::CompareAsPointers(Script_Value* lhs, Script_Value* rhs)
{
    void* a = lhs->GetHypData()->ToRef().GetPointer();
    void* b = rhs->GetHypData()->ToRef().GetPointer();

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

int Script_Value::CompareAsFunctions(Script_Value* lhs, Script_Value* rhs)
{
    Script_VMData* lhsVmData = lhs->GetVMData();
    Script_VMData* rhsVmData = rhs->GetVMData();

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CF_EQUAL : CF_NONE;
    }

    return (lhsVmData->func.m_addr == rhsVmData->func.m_addr)
        ? CF_EQUAL
        : CF_NONE;
}

int Script_Value::CompareAsNativeFunctions(Script_Value* lhs, Script_Value* rhs)
{
    Script_VMData* lhsVmData = lhs->GetVMData();
    Script_VMData* rhsVmData = rhs->GetVMData();

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CF_EQUAL : CF_NONE;
    }

    return (lhsVmData->nativeFunc == rhsVmData->nativeFunc)
        ? CF_EQUAL
        : CF_NONE;
}

const char* Script_Value::GetTypeString() const
{
    return ScriptApi_GetTypeString(*GetHypData());
}

String Script_Value::ToString() const
{
    if (IsRef())
    {
        if (Script_Value* ref = GetRef())
        {
            if (ref->IsRef())
            {
                return Script_String(g_referenceString);
            }

            return ref->ToString();
        }

        return Script_String(g_nullString);
    }

    const HypData& hypData = *GetHypData();

    return ScriptApi_ValueToString(hypData);
}

} // namespace hyperion
