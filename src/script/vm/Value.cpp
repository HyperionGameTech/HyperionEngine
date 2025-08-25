#include <script/vm/Value.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMMemoryBuffer.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMMap.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/Hasher.hpp>

#include <core/object/HypData.hpp>

#include <core/debug/Debug.hpp>

#include <stdio.h>
#include <cinttypes>
#include <iostream>

namespace hyperion {

extern HYP_API const char* LookupTypeName(TypeId typeId);

namespace vm {

static const VMString NULL_STRING = VMString("null");
static const VMString BOOLEAN_STRINGS[2] = { VMString("false"), VMString("true") };

static const TypeId typeIdI8 = TypeId::ForType<int8>();
static const TypeId typeIdI16 = TypeId::ForType<int16>();
static const TypeId typeIdI32 = TypeId::ForType<int32>();
static const TypeId typeIdI64 = TypeId::ForType<int64>();
static const TypeId typeIdU8 = TypeId::ForType<uint8>();
static const TypeId typeIdU16 = TypeId::ForType<uint16>();
static const TypeId typeIdU32 = TypeId::ForType<uint32>();
static const TypeId typeIdU64 = TypeId::ForType<uint64>();

TypeId GetTypeIdForHeapValue(const HeapValue* heapValue)
{
    if (!heapValue)
    {
        return TypeId::ForType<void>();
    }

    return heapValue->GetTypeId();
}

void* GetRawPointerForHeapValue(HeapValue* heapValue)
{
    if (!heapValue)
    {
        return nullptr;
    }

    return heapValue->GetRawPointer();
}

const void* GetRawPointerForHeapValue(const HeapValue* heapValue)
{
    if (!heapValue)
    {
        return nullptr;
    }

    return heapValue->GetRawPointer();
}

Value::Value()
{
    new (m_internal) HypData();
}

Value::Value(HypData&& data)
{
    new (m_internal) HypData(std::move(data));
}

Value::Value(Number number)
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

Value::Value(const Script_VMData& vmData)
{
    static_assert(sizeof(Script_VMData) == sizeof(HypData_UserData128));
    static_assert(alignof(Script_VMData) <= alignof(HypData_UserData128));

    HypData_UserData128 userData;
    Memory::MemCpy(&userData, &vmData, sizeof(Script_VMData));

    new (m_internal) HypData(userData);
}

Value::Value(Value&& other) noexcept
{
    new (m_internal) HypData(std::move(*other.GetHypData()));
}

Value& Value::operator=(Value&& other) noexcept
{
    if (this != &other)
    {
        GetHypData()->~HypData();
        new (m_internal) HypData(std::move(*other.GetHypData()));

        other.GetHypData()->~HypData();
        new (other.m_internal) HypData();
    }

    return *this;
}

Value::~Value()
{
    // have to manually call destructor because we used placement new
    GetHypData()->~HypData();

    // set all bytes to 0xFF to indicate garbage for debugging purposes
    Memory::MemSet(m_internal, 0xFFu, sizeof(m_internal));
}

HypData* Value::GetHypData()
{
    static_assert(sizeof(m_internal) == sizeof(HypData), "Size of m_internal must match size of HypData");
    static_assert(alignof(decltype(m_internal)) <= alignof(HypData), "Alignment of m_internal must be less than or equal to alignment of HypData");

    return reinterpret_cast<HypData*>(m_internal);
}

const HypData* Value::GetHypData() const
{
    static_assert(sizeof(m_internal) == sizeof(HypData), "Size of m_internal must match size of HypData");
    static_assert(alignof(decltype(m_internal)) <= alignof(HypData), "Alignment of m_internal must be less than or equal to alignment of HypData");

    return reinterpret_cast<const HypData*>(m_internal);
}

void Value::Mark()
{
    if (Value* ref = GetRef())
    {
        ref->Mark();

        return;
    }

    // @TODO: Mark heap pointer
}

Script_VMData* Value::GetVMData() const
{
    const HypData& data = *GetHypData();

    return data.TryGet<Script_VMData>().TryGet();
}

bool Value::IsValid() const
{
    return GetHypData()->IsValid();
}

bool Value::IsGarbage() const
{
    // if all bytes are 0xFF it is considered garbage
    const uint8* bytes = reinterpret_cast<const uint8*>(m_internal);
    for (SizeType i = 0; i < sizeof(m_internal); i++)
    {
        if (bytes[i] != 0xFF)
        {
            return false;
        }
    }

    return true;
}

bool Value::IsFunction() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr)
    {
        return false;
    }

    return vmData->type == Script_VMData::FUNCTION || vmData->type == Script_VMData::NATIVE_FUNCTION;
}

bool Value::IsNativeFunction() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr)
    {
        return false;
    }

    return vmData->type == Script_VMData::NATIVE_FUNCTION;
}

bool Value::IsRef() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr)
    {
        return false;
    }

    return vmData->type == Script_VMData::VALUE_REF;
}

Value* Value::GetRef() const
{
    const Script_VMData* vmData = GetVMData();

    if (vmData == nullptr || vmData->type != Script_VMData::VALUE_REF)
    {
        return nullptr;
    }

    return vmData->valueRef;
}

void Value::AssignValue(Value&& other, bool assignRef)
{
    Value* ref;

    if (assignRef && (ref = GetRef()) != nullptr)
    {
        *ref = std::move(other);
    }
    else
    {
        *this = std::move(other);
    }
}

bool Value::GetUnsigned(uint64* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<uint64>(/* strict */ false))
    {
        return false;
    }

    *out = data.Get<uint64>();

    return true;
}

bool Value::GetInteger(int64* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<int64>(/* strict */ false))
    {
        return false;
    }

    *out = data.Get<int64>();

    return true;
}

bool Value::GetSignedOrUnsigned(Number* out) const
{
    const HypData& data = *GetHypData();

    const TypeId typeId = data.GetTypeId();

    if (typeId == typeIdI8)
    {
        out->i = static_cast<int64>(data.Get<int8>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == typeIdU8)
    {
        out->u = static_cast<uint64>(data.Get<uint8>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == typeIdI16)
    {
        out->i = static_cast<int64>(data.Get<int16>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == typeIdU16)
    {
        out->u = static_cast<uint64>(data.Get<uint16>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == typeIdI32)
    {
        out->i = static_cast<int64>(data.Get<int32>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == typeIdU32)
    {
        out->u = static_cast<uint64>(data.Get<uint32>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == typeIdI64)
    {
        out->i = data.Get<int64>();
        out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == typeIdU64)
    {
        out->u = data.Get<uint64>();
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;
        return true;
    }

    return false;
}

bool Value::GetFloatingPoint(double* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<double>(/* strict */ true) && !data.Is<float>(/* strict */ true))
    {
        return false;
    }

    *out = data.Get<double>();

    return true;
}

bool Value::GetFloatingPointCoerce(double* out) const
{
    // alias for backwards compatibility
    return GetNumber(out);
}

bool Value::GetNumber(double* out) const
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

bool Value::GetNumber(Number* out) const
{
    const HypData& data = *GetHypData();

    const TypeId typeId = data.GetTypeId();

    if (typeId == TypeId::ForType<float>())
    {
        out->f = static_cast<double>(data.Get<float>());
        out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == TypeId::ForType<double>())
    {
        out->f = data.Get<double>();
        out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == typeIdI8)
    {
        out->i = static_cast<int64>(data.Get<int8>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == typeIdU8)
    {
        out->u = static_cast<uint64>(data.Get<uint8>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;
        return true;
    }

    if (typeId == typeIdI16)
    {
        out->i = static_cast<int64>(data.Get<int16>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == typeIdU16)
    {
        out->u = static_cast<uint64>(data.Get<uint16>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;
        return true;
    }

    if (typeId == typeIdI32)
    {
        out->i = static_cast<int64>(data.Get<int32>());
        out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == typeIdU32)
    {
        out->u = static_cast<uint64>(data.Get<uint32>());
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;
        return true;
    }

    if (typeId == typeIdI64)
    {
        out->i = data.Get<int64>();
        out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;
        return true;
    }

    if (typeId == typeIdU64)
    {
        out->u = data.Get<uint64>();
        out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;
        return true;
    }

    return false;
}

NumericType Value::GetNumericType() const
{
    const HypData& data = *GetHypData();
    const TypeId typeId = data.GetTypeId();

    if (typeId == typeIdI8)
    {
        return NT_I8;
    }
    if (typeId == typeIdU8)
    {
        return NT_U8;
    }
    if (typeId == typeIdI16)
    {
        return NT_I16;
    }
    if (typeId == typeIdU16)
    {
        return NT_U16;
    }
    if (typeId == typeIdI32)
    {
        return NT_I32;
    }
    if (typeId == typeIdU32)
    {
        return NT_U32;
    }
    if (typeId == typeIdI64)
    {
        return NT_I64;
    }
    if (typeId == typeIdU64)
    {
        return NT_U64;
    }
    if (typeId == TypeId::ForType<float>())
    {
        return NT_F32;
    }
    if (typeId == TypeId::ForType<double>())
    {
        return NT_F64;
    }

    return NT_NONE;
}

bool Value::GetBoolean(bool* out) const
{
    const HypData& data = *GetHypData();

    if (!data.Is<bool>())
    {
        return false;
    }

    *out = data.Get<bool>();
    return true;
}

VMObject* Value::GetObject() const
{
    const HypData& data = *GetHypData();

    if (!data.Is<VMObject>())
    {
        return nullptr;
    }

    return &data.Get<VMObject>();
}

VMArray* Value::GetArray() const
{
    const HypData& data = *GetHypData();

    if (!data.Is<VMArray>())
    {
        return nullptr;
    }
    return &data.Get<VMArray>();
}

AnyRef Value::ToRef() const
{
    const HypData& data = *GetHypData();
    return data.ToRef();
}

Script_UserData Value::GetUserData() const
{
    const Script_VMData* vmData = GetVMData();

    if (!vmData || vmData->type != Script_VMData::USER_DATA)
    {
        return nullptr;
    }

    return vmData->userData;
}

int Value::CompareAsPointers(Value* lhs, Value* rhs)
{
    void* a = lhs->GetHypData()->ToRef().GetPointer();
    void* b = rhs->GetHypData()->ToRef().GetPointer();

    if (a == b)
    {
        // pointers equal, drop out early.
        return CompareFlags::EQUAL;
    }
    else if (a == nullptr || b == nullptr)
    {
        return CompareFlags::NONE;
    }
    else
    {
        return CompareFlags::NONE;
    }
}

int Value::CompareAsFunctions(Value* lhs, Value* rhs)
{
    Script_VMData* lhsVmData = lhs->GetVMData();
    Script_VMData* rhsVmData = rhs->GetVMData();

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CompareFlags::EQUAL : CompareFlags::NONE;
    }

    return (lhsVmData->func.m_addr == rhsVmData->func.m_addr)
        ? CompareFlags::EQUAL
        : CompareFlags::NONE;
}

int Value::CompareAsNativeFunctions(Value* lhs, Value* rhs)
{
    Script_VMData* lhsVmData = lhs->GetVMData();
    Script_VMData* rhsVmData = rhs->GetVMData();

    if (lhsVmData == nullptr || rhsVmData == nullptr)
    {
        return lhsVmData == rhsVmData ? CompareFlags::EQUAL : CompareFlags::NONE;
    }

    return (lhsVmData->nativeFunc == rhsVmData->nativeFunc)
        ? CompareFlags::EQUAL
        : CompareFlags::NONE;
}

const char* Value::GetTypeString() const
{
    const HypData& data = *GetHypData();

    if (!data.IsValid())
    {
        return "<Uninitialized data>";
    }

    const TypeId typeId = data.GetTypeId();

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
    else if (typeId == TypeId::ForType<float>())
    {
        return "float";
    }
    else if (typeId == TypeId::ForType<double>())
    {
        return "double";
    }
    else if (typeId == TypeId::ForType<bool>())
    {
        return "bool";
    }
    else if (const Script_VMData* vmData = GetVMData())
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
        default:
            HYP_UNREACHABLE();
        }
    }

    const char* typeName = LookupTypeName(typeId);

    if (typeName != nullptr)
    {
        return typeName;
    }

    return "<Unknown type>";
}

VMString Value::ToString() const
{
    const SizeType bufSize = 256;
    char buf[bufSize] = { 0 };

    const int depth = 3;

    const HypData& data = *GetHypData();

    if (!data.IsValid())
    {
        return NULL_STRING;
    }

    const TypeId typeId = data.GetTypeId();

    if (typeId == TypeId::ForType<int8>())
    {
        snprintf(buf, bufSize, "%" PRId8, data.Get<int8>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<int16>())
    {
        snprintf(buf, bufSize, "%" PRId16, data.Get<int16>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<int32>())
    {
        snprintf(buf, bufSize, "%" PRId32, data.Get<int32>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<int64>())
    {
        snprintf(buf, bufSize, "%" PRId64, data.Get<int64>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<uint8>())
    {
        snprintf(buf, bufSize, "%" PRIu8, data.Get<uint8>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<uint16>())
    {
        snprintf(buf, bufSize, "%" PRIu16, data.Get<uint16>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<uint32>())
    {
        snprintf(buf, bufSize, "%" PRIu32, data.Get<uint32>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<uint64>())
    {
        snprintf(buf, bufSize, "%" PRIu64, data.Get<uint64>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<float>())
    {
        snprintf(buf, bufSize, "%f", data.Get<float>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<double>())
    {
        snprintf(buf, bufSize, "%lf", data.Get<double>());
        return VMString(buf);
    }
    else if (typeId == TypeId::ForType<bool>())
    {
        return BOOLEAN_STRINGS[data.Get<bool>() ? 1 : 0];
    }
    else if (const Script_VMData* vmData = GetVMData())
    {
        switch (vmData->type)
        {
        case Script_VMData::FUNCTION:
            return VMString("<Function>");
        case Script_VMData::NATIVE_FUNCTION:
            return VMString("<Native Function>");
        case Script_VMData::ADDRESS:
            snprintf(buf, bufSize, "<Function address @ %p>", (void*)vmData->func.m_addr);
            return VMString(buf);
        case Script_VMData::FUNCTION_CALL:
            return VMString("<Stack frame>");
        case Script_VMData::TRY_CATCH_INFO:
            return VMString("<Try catch info>");
        case Script_VMData::USER_DATA:
            snprintf(buf, bufSize, "<User data @ %p>", vmData->userData);
            return VMString(buf);
        case Script_VMData::VALUE_REF:
            Assert(vmData->valueRef != nullptr);
            if (vmData->valueRef == this)
            {
                return VMString("<Circular Reference>");
            }
            else
            {
                return vmData->valueRef->ToString();
            }
        default:
            HYP_UNREACHABLE();
            return VMString("<Unknown VM data>");
        }
    }

    return VMString(String("<Object of type ") + String(GetTypeString()) + String(">"));
}

void Value::ToRepresentation(
    std::stringstream& ss,
    bool addTypeName,
    int depth) const
{
    // just use ToString for now
    if (addTypeName)
    {
        ss << GetTypeString() << "(";
    }

    ss << ToString().GetData();

    if (addTypeName)
    {
        ss << ")";
    }
}

} // namespace vm
} // namespace hyperion
