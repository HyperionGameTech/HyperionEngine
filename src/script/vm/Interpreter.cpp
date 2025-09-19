#include <script/vm/Interpreter.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/String.hpp>
#include <script/vm/HashMap.hpp>
#include <script/vm/GC.hpp>
#include <script/vm/Exception.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypMember.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/debug/Debug.hpp>
#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <script/Instructions.hpp>

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

#define HYP_NUMERIC_OPERATION(a, b, oper)                                         \
    do                                                                            \
    {                                                                             \
        switch (numericType)                                                      \
        {                                                                         \
        case NT_I8:                                                               \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int8>(a.u);                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int8>(b.u);                         \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            break;                                                                \
        case NT_I16:                                                              \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int16>(a.u);                               \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int16>(b.u);                        \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            break;                                                                \
        case NT_I32:                                                              \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int32>(a.u);                               \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int32>(b.u);                        \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            break;                                                                \
        case NT_I64:                                                              \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int64>(a.u);                               \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int64>(b.u);                        \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
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

#define HYP_NUMERIC_OPERATION_BITWISE(a, b, oper)                                     \
    do                                                                                \
    {                                                                                 \
        switch (numericType)                                                          \
        {                                                                             \
        case NT_I8:                                                                   \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int8>(a.u);                                    \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int8>(b.u);                             \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            break;                                                                    \
        case NT_I16:                                                                  \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int16>(a.u);                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int16>(b.u);                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            break;                                                                    \
        case NT_I32:                                                                  \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int32>(a.u);                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int32>(b.u);                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            break;                                                                    \
        case NT_I64:                                                                  \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int64>(a.u);                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int64>(b.u);                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            break;                                                                    \
        case NT_U8:                                                                   \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint8>(a.i);                                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = static_cast<uint8>(a.u);                                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint8>(b.i);                            \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = static_cast<uint8>(b.u);                            \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            break;                                                                    \
        case NT_U16:                                                                  \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint16>(a.i);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = static_cast<uint16>(a.u);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint16>(b.i);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = static_cast<uint16>(b.u);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            break;                                                                    \
        case NT_U32:                                                                  \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint32>(a.i);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = static_cast<uint32>(a.u);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint32>(b.i);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = static_cast<uint32>(b.u);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            break;                                                                    \
        case NT_U64:                                                                  \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint64>(a.i);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = a.u;                                                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint64>(b.i);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = b.u;                                                \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            break;                                                                    \
        default:                                                                      \
            vm->ThrowException(instance, Script_Exception::InvalidBitwiseArgument()); \
            break;                                                                    \
        }                                                                             \
    }                                                                                 \
    while (0)

namespace hyperion {

extern const char* LookupTypeName(TypeId typeId);

using BCRegister = uint8;

#pragma region ScriptApi

static const String g_nullString = "null";
static const String g_boolStrings[2] = { "false", "true" };

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

// clang-format off
static const HashMap<TypeId, String (*)(const void*)> g_builtinToStringFunctions = {
    { g_typeIdI8, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int8*>(p)); } },
    { g_typeIdI16, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int16*>(p)); } },
    { g_typeIdI32, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int32*>(p)); } },
    { g_typeIdI64, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const int64*>(p)); } },
    { g_typeIdU8, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint8*>(p)); } },
    { g_typeIdU16, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint16*>(p)); } },
    { g_typeIdU32, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint32*>(p)); } },
    { g_typeIdU64, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const uint64*>(p)); } },
    { g_typeIdF32, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const float*>(p)); } },
    { g_typeIdF64, [](const void* p) -> String { return HYP_FORMAT("{}", *reinterpret_cast<const double*>(p)); } },
    { g_typeIdBool, [](const void* p) -> String { return g_boolStrings[*reinterpret_cast<const bool*>(p) ? 1 : 0]; } },
    { g_typeIdString, [](const void* p) -> String { return *reinterpret_cast<const Script_String*>(p); } }
};
// clang-format on

template <class T, typename = std::enable_if_t<!std::is_same_v<Script_VMData, NormalizedType<T>> && !std::is_same_v<Number, NormalizedType<T>> && !std::is_same_v<HypData, NormalizedType<T>>>>
static inline Script_Value ScriptApi_MakeValue(T&& data)
{
    return Script_Value(HypData(std::forward<T>(data)));
}

Script_Value ScriptApi_MakeValue(HypData&& data)
{
    return Script_Value(std::move(data));
}

Script_Value ScriptApi_MakeValue(const Script_VMData& data)
{
    return Script_Value(data);
}

Script_Value ScriptApi_MakeValue(const Number& number)
{
    return Script_Value(number);
}

/*! \brief Use for loading into registers - does not promote to tracked memory so the lifetime of `refValue` must be managed by the caller */
Script_Value ScriptApi_MakeRef(Script_Value* pValue)
{
    Assert(pValue != nullptr);

    Script_VMData vmData;
    vmData.type = Script_VMData::VALUE_REF;
    vmData.valueRef = pValue;

    Assert(vmData.valueRef != nullptr);
    Assert(!vmData.valueRef->IsGarbage(), "Creating a reference to garbage value");

    return Script_Value(vmData);
}

Script_Value ScriptApi_MakeTrackedRef(Script_Value* pValue, Script_GC* gc)
{
    Assert(gc != nullptr);
    Assert(pValue != nullptr);

    if (pValue->GetHypData()->extData.scriptGcIndex != INVALID_GC_INDEX)
    {
        // already in tracked memory, make a reference to this value
        return ScriptApi_MakeRef(pValue);
    }

    const TypeId originalTypeId = pValue->GetHypData()->GetTypeId();

    gc->MoveToTrackedMemory(*pValue);

    return *pValue;
}

#define PASS_AS_REF(value) ((value).GetHypData()->Is<Any>())
#define PASS_AS_REF_DATA(data) ((data).Is<Any>())

// Performs a shallow copy of the value. Numeric and primitive types are copied as-is.
Script_Value ScriptApi_ShallowCopy(Script_Value& refValue, Script_GC* gc)
{
    if (refValue.IsRef())
    {
        return refValue; // already a reference, return as-is
    }

    if (refValue.GetHypData()->extData.scriptGcIndex != INVALID_GC_INDEX)
    {
        // in tracked memory, make a reference to it
        return ScriptApi_MakeRef(&refValue);
    }

    const HypData& hypData = *refValue.GetHypData();

    HypData newHypData;

    Visit(hypData.value, [&newHypData](const auto& val)
        {
            newHypData.value.Set<NormalizedType<decltype(val)>>(val);
        });

    return Script_Value(std::move(newHypData));
}

bool ScriptApi_ShouldValuePassByRef(const Script_Value& value)
{
    if (!value.IsValid())
    {
        return false;
    }

    return PASS_AS_REF(value);
}

static const const char* g_unknownTypeString = "<Unknown type>";

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
    else if (typeId == TypeId::ForType<Script_Array>())
    {
        return "array";
    }

    const char* typeName = LookupTypeName(typeId);

    if (typeName != nullptr)
    {
        return typeName;
    }

    return g_unknownTypeString;
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

    if (typeIdString && typeIdString != g_unknownTypeString)
    {
        return typeIdString;
    }

    return g_unknownTypeString;
}

bool ScriptApi_StringifyData(const HypData& data, Script_String& outString, int maxDepth, int currDepth);

bool ScriptApi_StringifyData(const HypData& data, Script_String& outString, int maxDepth, int currDepth)
{
    if (currDepth >= maxDepth && maxDepth >= 0)
    {
        outString = Script_String("...");
        return true;
    }

    constexpr SizeType bufSize = 256;

    char buf[bufSize] = { 0 };

    if (!data.IsValid())
    {
        outString = Script_String(g_nullString);

        return true;
    }

    auto formatIt = g_builtinToStringFunctions.Find(data.GetTypeId());
    if (formatIt != g_builtinToStringFunctions.End())
    {
        outString = Script_String(formatIt->second(data.ToRef().GetPointer()));

        return true;
    }

    constexpr int maxArrayDepth = 2;

    if (const HypDataArray* pArray = data.TryGet<HypDataArray>().TryGet())
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

                outString += ScriptApi_ValueToString(HypData(pArray->ElementAt(i)), currDepth + 1);
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

    if (const HypClass* hypClass = GetClass(data.GetTypeId()))
    {
        const HypMethod* toStringMethod = hypClass->GetMethod("ToString");

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

        constexpr const char* objectFormatString = "<%s @ %p>";

        int n = std::snprintf(
            buf,
            bufSize,
            objectFormatString,
            hypClass->GetName().LookupString(),
            data.ToRef().GetPointer());

        // if the class name is too long, dynamically allocate a larger buffer
        if (n < 0)
        {
            outString = Script_String("<Error formatting object>");

            return true;
        }

        if (static_cast<SizeType>(n) >= bufSize)
        {
            const SizeType newBufSize = static_cast<SizeType>(n) + 1;

            char* newBuf = static_cast<char*>(Memory::Allocate(newBufSize));
            Assert(newBuf != nullptr);

            n = std::snprintf(
                newBuf,
                newBufSize,
                objectFormatString,
                hypClass->GetName().LookupString(),
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
    static const int s_maxDepth = 3;

    Script_String result("<error>");
    if (ScriptApi_StringifyData(data, result, s_maxDepth, currDepth))
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
        case Script_VMData::USER_DATA:
            std::snprintf(buf, bufSize, "<User data @ %p>", vmData->userData);
            return Script_String(buf);
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

const uint16 Script_StaticMemory::staticSize = 65535;

Script_StaticMemory::Script_StaticMemory()
    : m_data(new Script_Value[staticSize])
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

    SCRIPT_INLINE void OpLoadI32(BCRegister reg, int32 i32)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(i32);
    }

    SCRIPT_INLINE void OpLoadI64(BCRegister reg, int64 i64)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(i64);
    }

    SCRIPT_INLINE void OpLoadU32(BCRegister reg, uint32 u32)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(u32);
    }

    SCRIPT_INLINE void OpLoadU64(BCRegister reg, uint64 u64)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(u64);
    }

    SCRIPT_INLINE void OpLoadF32(BCRegister reg, float32 f32)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(f32);
    }

    SCRIPT_INLINE void OpLoadF64(BCRegister reg, float64 f64)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(f64);
    }

    SCRIPT_INLINE void OpLoadOffset(BCRegister reg, uint16 offset)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            offset <= stackMemory.GetStackPointer(),
            "Stack offset out of bounds (%u)",
            offset);

        Script_Value& srcValue = stackMemory[stackMemory.GetStackPointer() - offset];

        // read value from stack at (sp - offset)
        // into the the register
        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadIndex(BCRegister reg, uint16 index)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stackMemory.GetStackPointer());

        Script_Value& srcValue = stackMemory[index];

        // read value from stack at the index into the the register
        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadStatic(BCRegister reg, uint16 index)
    {
        // read value from static memory
        // at the index into the the register
        Script_Value& srcValue = vm->m_staticMemory[index];

        instance->thread.m_regs[reg] = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadConstantString(BCRegister reg, uint32 len, const char* str)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(str != nullptr ? Script_String(str, str + len) : Script_String());
    }

    SCRIPT_INLINE void OpLoadAddr(BCRegister reg, Script_FunctionAddress addr)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::ADDRESS;
        vmData.addr = addr;

        instance->thread.m_regs[reg] = ScriptApi_MakeValue(vmData);
    }

    SCRIPT_INLINE void OpLoadFunc(BCRegister reg, Script_FunctionAddress addr, uint8 nargs, uint8 flags)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::FUNCTION;
        vmData.func.m_addr = addr;
        vmData.func.m_nargs = nargs;
        vmData.func.m_flags = flags;

        instance->thread.m_regs[reg] = ScriptApi_MakeValue(vmData);
    }

    SCRIPT_INLINE void OpLoadArrayIdx(BCRegister dstReg, BCRegister srcReg, BCRegister indexReg)
    {
        Script_Value& src = *instance->thread.m_regs[srcReg].Deref();

        Number key;

        if (!instance->thread.m_regs[indexReg].GetSignedOrUnsigned(&key))
        {
            vm->ThrowException(instance, Script_Exception("Array index must be an integral type"));

            return;
        }

        if (Script_Array* array = src.GetHypData()->TryGet<Script_Array>().TryGet())
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

                Script_Value& srcValue = (*array)[key.i];

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

                Script_Value& srcValue = (*array)[key.u];

                instance->thread.m_regs[dstReg] = PASS_AS_REF(srcValue)
                    ? ScriptApi_MakeRef(&srcValue)
                    : ScriptApi_ShallowCopy(srcValue, vm->GetGC());
            }

            return;
        }

        // throw an exception
        vm->ThrowException(instance, Script_Exception::InvalidOperationException("Indexing", src.GetTypeString()));
    }

    SCRIPT_INLINE void OpLoadOffsetRef(BCRegister reg, uint16 offset)
    {
        // load reference to stack value at (sp - offset) into the register
        Script_Value newRef = ScriptApi_MakeTrackedRef(instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - offset].Deref(), vm->GetGC());
        instance->thread.m_regs[reg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadIndexRef(BCRegister reg, uint16 index)
    {
        Script_StackMemory& stackMemory = instance->thread.m_stack;

        Assert(
            index < stackMemory.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stackMemory.GetStackPointer());

        Script_Value newRef = ScriptApi_MakeTrackedRef(stackMemory[index].Deref(), vm->GetGC());
        instance->thread.m_regs[reg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadRef(BCRegister dstReg, BCRegister srcReg)
    {
        Script_Value newRef = ScriptApi_MakeTrackedRef(instance->thread.m_regs[srcReg].Deref(), vm->GetGC());
        instance->thread.m_regs[dstReg] = std::move(newRef);
    }

    SCRIPT_INLINE void OpLoadDeref(BCRegister dstReg, BCRegister srcReg)
    {
        Script_Value& src = *instance->thread.m_regs[srcReg].Deref(); // double deref to get the actual value
        instance->thread.m_regs[dstReg] = ScriptApi_ShallowCopy(*src.Deref(), vm->GetGC());
    }

    SCRIPT_INLINE void OpLoadNull(BCRegister reg)
    {
        instance->thread.m_regs[reg] = Script_Value();
    }

    SCRIPT_INLINE void OpLoadTrue(BCRegister reg)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(true);
    }

    SCRIPT_INLINE void OpLoadFalse(BCRegister reg)
    {
        instance->thread.m_regs[reg] = ScriptApi_MakeValue(false);
    }

    SCRIPT_INLINE void OpLoadClass(BCRegister reg, uint64 nameHash)
    {
        Name name = Name(NameID(nameHash));
        const HypClass* hypClass = HypClassRegistry::GetInstance().GetClass(name);
        if (!hypClass)
        {
            vm->ThrowException(instance, Script_Exception::ClassNotFoundException(name.LookupString()));

            return;
        }

        Script_Value classValue = ScriptApi_MakeValue(HypData(HypClassRef(hypClass)));

        instance->thread.m_regs[reg] = std::move(classValue);
    }

    SCRIPT_INLINE void OpMovOffset(uint16 offset, BCRegister reg)
    {
        // copy value from register to stack value at (sp - offset)
        instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - offset].AssignValue(ScriptApi_ShallowCopy(*instance->thread.m_regs[reg].Deref(), vm->GetGC()), true);
    }

    SCRIPT_INLINE void OpMovIndex(uint16 index, BCRegister reg)
    {
        // copy value from register to stack value at index
        instance->thread.m_stack[index].AssignValue(ScriptApi_ShallowCopy(*instance->thread.m_regs[reg].Deref(), vm->GetGC()), true);
    }

    SCRIPT_INLINE void OpMovStatic(uint16 index, BCRegister reg)
    {
        Assert(index < vm->m_staticMemory.staticSize);

        vm->m_staticMemory[index] = std::move(instance->thread.m_regs[reg]);
    }

    SCRIPT_INLINE void OpMovArrayIdx(BCRegister dstReg, uint32 index, BCRegister srcReg)
    {
        Script_Value& src = *instance->thread.m_regs[dstReg].Deref();

        if (!src.GetHypData()->Is<Script_Array>())
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("Indexing", src.GetTypeString()));
            return;
        }

        Script_Array& array = src.GetHypData()->Get<Script_Array>();

        if (index >= array.Size())
        {
            vm->ThrowException(instance, Script_Exception::OutOfBoundsException(SizeType(index), array.Size()));

            return;
        }

        Script_Value& srcValue = *instance->thread.m_regs[srcReg].Deref();
        Script_Value& dstValue = array[index];

        dstValue = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeRef(&srcValue)
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());

        dstValue.Mark();
    }

    SCRIPT_INLINE void OpMovArrayIdxReg(BCRegister dstReg, BCRegister indexReg, BCRegister srcReg)
    {
        Script_Value& src = *instance->thread.m_regs[dstReg].Deref();

        if (!src.GetHypData()->Is<Script_Array>())
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("Indexing", src.GetTypeString()));
            return;
        }

        Script_Array& array = src.GetHypData()->Get<Script_Array>();

        Number index;
        Script_Value& indexRegisterValue = instance->thread.m_regs[indexReg];

        if (!indexRegisterValue.GetSignedOrUnsigned(&index))
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

            Script_Value& srcValue = *instance->thread.m_regs[srcReg].Deref();
            Script_Value& dstValue = array[indexValue];

            dstValue = PASS_AS_REF(srcValue)
                ? ScriptApi_MakeRef(&srcValue)
                : ScriptApi_ShallowCopy(srcValue, vm->GetGC());

            dstValue.Mark();
        }
        else
        { // unsigned
            const uint64 indexValue = index.u;

            if (SizeType(indexValue) >= array.Size())
            {
                vm->ThrowException(instance, Script_Exception::OutOfBoundsException(indexValue, array.Size()));

                return;
            }

            Script_Value& srcValue = *instance->thread.m_regs[srcReg].Deref();
            Script_Value& dstValue = array[indexValue];

            dstValue = PASS_AS_REF(srcValue)
                ? ScriptApi_MakeRef(&srcValue)
                : ScriptApi_ShallowCopy(srcValue, vm->GetGC());

            dstValue.Mark();
        }
    }

    SCRIPT_INLINE void OpMov(BCRegister dstReg, BCRegister srcReg)
    {
        instance->thread.m_regs[dstReg] = std::move(instance->thread.m_regs[srcReg]);
    }

    SCRIPT_INLINE void OpCheckHasMember(BCRegister dstReg, BCRegister srcReg, uint64 hash)
    {
        Script_Value& src = *instance->thread.m_regs[srcReg].Deref();
        Script_Value& result = instance->thread.m_regs[dstReg];

        const HypClass* hypClass = nullptr;

        if (const AnyHandle& object = src.GetObject())
        {
            hypClass = object.ptr->InstanceClass();
        }
        else
        {
            hypClass = GetClass(src.GetHypData()->GetTypeId());
        }

        if (hypClass != nullptr)
        {
            IHypMember* member = hypClass->GetMember(WeakName(NameID(hash)));
            result = ScriptApi_MakeValue(member != nullptr);

            return;
        }

        result = ScriptApi_MakeValue(false);
    }

    SCRIPT_INLINE void OpSetField(BCRegister dstReg, uint64 hash, BCRegister srcReg)
    {
        Script_Value* pValue = instance->thread.m_regs[dstReg].Deref();

        const HypClass* hypClass = nullptr;

        if (const AnyHandle& object = pValue->GetObject())
        {
            hypClass = object.ptr->InstanceClass();
        }
        else
        {
            hypClass = GetClass(pValue->GetHypData()->GetTypeId());
        }

        if (!hypClass)
        {
            vm->ThrowException(instance, Script_Exception::InvalidMemberAccessException(pValue));

            return;
        }

        HypField* field = hypClass->GetField(WeakName(NameID(hash)));

        if (!field)
        {
            vm->ThrowException(instance, Script_Exception::MemberNotFoundException(pValue, hash));

            return;
        }

        field->Set(*pValue->GetHypData(), *instance->thread.m_regs[srcReg].Deref()->GetHypData());
    }

    SCRIPT_INLINE void OpGetMember(BCRegister dstReg, BCRegister srcReg, uint64 hash)
    {
        Script_Value* pValue = instance->thread.m_regs[srcReg].Deref();

        const HypClass* hypClass = nullptr;

        if (const AnyHandle& object = pValue->GetObject())
        {
            // instance member access
            hypClass = object.ptr->InstanceClass();
        }
        else if (const HypClassRef* classRef = pValue->GetHypData()->TryGet<HypClassRef>().TryGet())
        {
            // static member access on class reference
            hypClass = *classRef;
        }
        // temp special case for arrays
        else if (const HypDataArray* array = pValue->GetHypData()->TryGet<HypDataArray>().TryGet())
        {
            hypClass = GetClass(TypeId::ForType<Script_Array>());
        }
        else
        {
            hypClass = GetClass(pValue->GetHypData()->GetTypeId());
        }

        if (!hypClass)
        {
            vm->ThrowException(instance, Script_Exception::InvalidMemberAccessException(pValue));

            return;
        }

        IHypMember* member = hypClass->GetMember(WeakName(NameID(hash)));
        if (!member)
        {
            vm->ThrowException(instance, Script_Exception::MemberNotFoundException(pValue, hash));

            return;
        }

        if (member->GetMemberType() == HypMemberType::TYPE_FIELD)
        {
            HypField* field = static_cast<HypField*>(member);

            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(field->Get(*pValue->GetHypData()));
        }
        else if (member->GetMemberType() == HypMemberType::TYPE_CONSTANT)
        {
            HypConstant* constant = static_cast<HypConstant*>(member);

            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(constant->Get());
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

            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(vmData);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception("Member is not a field or method"));
        }
    }

    SCRIPT_INLINE void OpPush(BCRegister reg)
    {
        // Move value from register to top of stack
        instance->thread.m_stack.Push(ScriptApi_ShallowCopy(*instance->thread.m_regs[reg].Deref(), vm->GetGC()));
    }

    SCRIPT_INLINE void OpPop()
    {
        instance->thread.m_stack.Pop();
    }

    SCRIPT_INLINE void OpPushArray(BCRegister dstReg, BCRegister srcReg)
    {
        Script_Value& dst = *instance->thread.m_regs[dstReg].Deref();

        if (!dst.GetHypData()->Is<Script_Array>())
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("PUSH_ARRAY", dst.GetTypeString()));
            return;
        }

        Script_Array& array = dst.GetHypData()->Get<Script_Array>();

        array.PushBack(ScriptApi_ShallowCopy(*instance->thread.m_regs[srcReg].Deref(), vm->GetGC()));
        array.Back().Mark();
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

    SCRIPT_INLINE void OpCall(BCRegister reg, uint8_t nargs)
    {
        vm->Invoke(instance, std::move(instance->thread.m_regs[reg]), nargs);
    }

    SCRIPT_INLINE void OpRet()
    {
        // get top of stack (should be the address before jumping)
        Script_Value& top = instance->thread.GetStack().Top();

        Script_VMData* vmData = top.GetVMData();
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
        Script_Value& top = instance->thread.m_stack.Top();

        Script_VMData* vmData = top.GetVMData();
        Assert(vmData != nullptr);
        Assert(vmData->type == Script_VMData::TRY_CATCH_INFO);

        Assert(instance->thread.m_exceptionState.m_tryCounter != 0);

        // pop try catch info
        instance->thread.m_stack.Pop();
        --instance->thread.m_exceptionState.m_tryCounter;
    }

    SCRIPT_INLINE void OpNew(BCRegister dst, BCRegister src) // come back to this
    {
        // read value from register
        Script_Value& classValue = *instance->thread.m_regs[src].Deref();

        const HypClassRef& classRef = classValue.GetHypData()->Get<HypClassRef>();
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

    SCRIPT_INLINE void OpNewArray(BCRegister dst, uint32 size)
    {
        // assign register value to the allocated object
        instance->thread.m_regs[dst] = ScriptApi_MakeValue(Script_Array(size));
    }

    SCRIPT_INLINE void OpBeginClass(BCRegister reg)
    {
        Script_Stream* bs = &instance->stream;

        // Read class name length and name
        uint16 nameLen;
        bs->Read(&nameLen);

        char* nameStr = (char*)std::malloc(nameLen + 1);
        nameStr[nameLen] = '\0';
        bs->Read(nameStr, nameLen);

        // Create a new class with the given name
        Name className = CreateNameFromDynamicString(nameStr);
        std::free(nameStr);

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
                case HypMemberType::TYPE_CONSTANT:
                {
                    // static field

                    uint32 size;
                    bs->Read(&size);

                    // Create constant
                    members.PushBack(HypMember(HypConstant(
                        CreateNameFromDynamicString(memberNameStr),
                        TypeId(memberTypeIdValue),
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
                    Assert(stackOffset <= instance->thread.GetStack().GetStackPointer(), "Stack offset out of bounds!");
                    Script_Value& funcValue = instance->thread.GetStack()[instance->thread.GetStack().GetStackPointer() - stackOffset];

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
                        funcVmData->func.m_flags | flags, // combine flags
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
        Script_Value& parentClassValue = instance->thread.m_regs[reg];

        const HypClass* parentClass = nullptr;

        if (parentClassValue.IsValid())
        {
            parentClass = parentClassValue.GetHypData()->Get<HypClassRef>();
            Assert(parentClass != nullptr);
        }

        // some type needs to be set
        Assert((flags & (uint8)(HypClassFlags::CLASS_TYPE | HypClassFlags::STRUCT_TYPE | HypClassFlags::ENUM_TYPE)) != 0);

        DynamicHypClassInstance* newClass = new DynamicHypClassInstance(
            TypeId(typeIdValue),
            className,
            parentClass,
            Span<const HypClassAttribute>(), // @TODO
            (HypClassFlags)flags,
            members.ToSpan());

        HypClassRegistry::GetInstance().RegisterClass(newClass->GetTypeId(), newClass);

        Script_Value classValue = ScriptApi_MakeValue(HypClassRef(newClass));

        // promote the class object to tracked gc memory so it doesn't instantly get destroyed
        instance->thread.m_regs[reg] = ScriptApi_MakeTrackedRef(&classValue, vm->GetGC());
    }

    SCRIPT_INLINE void OpCmp(BCRegister lhsReg, BCRegister rhsReg)
    {
        // dropout early for comparing something against itself
        if (lhsReg == rhsReg)
        {
            instance->thread.m_regs.flags = CF_EQUAL;
            return;
        }

        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetSignedOrUnsigned(&a) && rhs->GetSignedOrUnsigned(&b))
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
        else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f))
        {
            instance->thread.m_regs.flags = (a.f == b.f) ? CF_EQUAL : ((a.f > b.f) ? CF_GREATER : CF_NONE);
        }
        else
        {
            bool lhsBool;
            bool rhsBool;

            if (lhs->GetBoolean(&lhsBool) && rhs->GetBoolean(&rhsBool))
            {
                instance->thread.m_regs.flags = (lhsBool == rhsBool) ? CF_EQUAL : ((lhsBool > rhsBool) ? CF_GREATER : CF_NONE);
            }
            else
            {
                const int res = Script_Value::CompareAsPointers(lhs, rhs);

                if (res != -1)
                {
                    instance->thread.m_regs.flags = res;
                }
                else
                {
                    vm->ThrowException(instance, Script_Exception::InvalidComparisonException(lhs->GetTypeString(), rhs->GetTypeString()));
                }
            }
        }
    }

    SCRIPT_INLINE void OpCmpZ(BCRegister reg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[reg].Deref();

        Number num;

        if (lhs->GetSignedOrUnsigned(&num))
        {
            instance->thread.m_regs.flags = ((num.flags & Number::FLAG_SIGNED) ? !num.i : !num.u) ? CF_EQUAL : CF_NONE;
        }
        else if (lhs->GetFloatingPoint(&num.f))
        {
            instance->thread.m_regs.flags = !num.f ? CF_EQUAL : CF_NONE;
        }
        else
        {
            bool boolValue;
            if (lhs->GetBoolean(&boolValue))
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
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, +);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("ADD", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpSub(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, -);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("SUB", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpMul(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

            Number result { numericType };
            HYP_NUMERIC_OPERATION(a, b, *);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("MUL", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpDiv(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

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
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("DIV", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpMod(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

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
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("MOD", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpAnd(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, &);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("AND", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpOr(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, |);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("OR", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpXor(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, ^);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("XOR", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpShl(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = lhs->GetNumericType();

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, <<);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("SHL", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpShr(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Script_Value* lhs = instance->thread.m_regs[lhsReg].Deref();
        Script_Value* rhs = instance->thread.m_regs[rhsReg].Deref();

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            const NumericType numericType = lhs->GetNumericType();

            Number result { numericType };
            HYP_NUMERIC_OPERATION_BITWISE(a, b, >>);

            // set the destination register to be the result
            instance->thread.m_regs[dstReg] = ScriptApi_MakeValue(result);
        }
        else
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("SHR", lhs->GetTypeString(), rhs->GetTypeString()));
        }
    }

    SCRIPT_INLINE void OpNot(BCRegister reg)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[reg].Deref();

        Number num;

        // we only allow bitwise NOT on integers
        if (value.GetNumber(&num) && (num.flags & (Number::FLAG_SIGNED | Number::FLAG_UNSIGNED)))
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

    SCRIPT_INLINE void OpThrow(BCRegister reg)
    {
        // load value from register
        Script_Value* value = instance->thread.m_regs[reg].Deref();

        // @TODO Allow throwing the arugment

        vm->ThrowException(instance, Script_Exception("User exception"));
    }

    SCRIPT_INLINE void OpExportSymbol(BCRegister reg, uint64 hash)
    {
        Script_Value& srcValue = *instance->thread.m_regs[reg].Deref();

        Script_Value newValue = PASS_AS_REF(srcValue)
            ? ScriptApi_MakeTrackedRef(&srcValue, vm->GetGC())
            : ScriptApi_ShallowCopy(srcValue, vm->GetGC());

        if (!instance->exportedSymbols.Store(hash, std::move(newValue)).second)
        {
            vm->ThrowException(instance, Script_Exception::DuplicateExportException());
        }
    }

    SCRIPT_INLINE void OpNeg(BCRegister reg)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[reg].Deref();

        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidOperationException("NEG", value.GetTypeString()));

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

    SCRIPT_INLINE void OpCastU8(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();

        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "uint8"));

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

    SCRIPT_INLINE void OpCastU16(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();

        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "uint16"));

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

    SCRIPT_INLINE void OpCastU32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "uint32"));

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

    SCRIPT_INLINE void OpCastU64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "uint64"));

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

    SCRIPT_INLINE void OpCastI8(BCRegister dst, BCRegister src)
    {
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "int8"));

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

    SCRIPT_INLINE void OpCastI16(BCRegister dst, BCRegister src)
    {
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "int16"));

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

    SCRIPT_INLINE void OpCastI32(BCRegister dst, BCRegister src)
    {
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "int32"));

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

    SCRIPT_INLINE void OpCastI64(BCRegister dst, BCRegister src)
    {
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "int64"));

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

    SCRIPT_INLINE void OpCastF32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "float32"));

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

    SCRIPT_INLINE void OpCastF64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();
        Number num;

        if (!value.GetNumber(&num))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "float64"));

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

    SCRIPT_INLINE void OpCastBool(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();

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

        instance->thread.m_regs[dst] = ScriptApi_MakeValue(result);
    }

    SCRIPT_INLINE void OpCastString(BCRegister dst, BCRegister src)
    {
        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();

        const Script_String* pString = nullptr;

        if (!value.GetString(&pString))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), "string"));

            return;
        }

        instance->thread.m_regs[dst] = ScriptApi_ShallowCopy(value, vm->GetGC());
    }

    SCRIPT_INLINE void OpCastDynamic(BCRegister dst, BCRegister src)
    {
        // dst register holds HypClassRef object
        Script_Value& classValue = *instance->thread.m_regs[dst].Deref();

        const HypClassRef& classRef = classValue.GetHypData()->Get<HypClassRef>();
        Assert(classRef.IsValid());

        // load value from register
        Script_Value& value = *instance->thread.m_regs[src].Deref();

        const HypClass* hypClass = nullptr;

        if (const AnyHandle& object = value.GetObject())
        {
            hypClass = object.ptr->InstanceClass();
        }
        else
        {
            hypClass = GetClass(value.GetHypData()->GetTypeId());
        }

        if (!hypClass || !hypClass->IsDerivedFrom(classRef))
        {
            vm->ThrowException(instance, Script_Exception::InvalidCastException(value.GetTypeString(), classRef->GetName().LookupString()));

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

        BCRegister reg;
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
            BCRegister arrayReg;
            bs->Read(&arrayReg);
            BCRegister indexReg;
            bs->Read(&indexReg);
            handler->OpLoadArrayIdx(reg, arrayReg, indexReg);
        }
        break;

        case LSRC_MEMBER:
        {
            BCRegister objReg;
            bs->Read(&objReg);
            uint64 hash;
            bs->Read(&hash);
            handler->OpGetMember(reg, objReg, hash);
        }
        break;

        case LSRC_REGISTER:
        {
            BCRegister srcReg;
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
            BCRegister arrayReg;
            bs->Read(&arrayReg);
            uint32 index;
            bs->Read(&index);
            BCRegister srcReg;
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
                BCRegister srcReg;
                bs->Read(&srcReg);
                handler->OpMovOffset(offset, srcReg);
            }
            break;

            case MDST_INDEX:
            {
                uint16 index;
                bs->Read(&index);
                BCRegister srcReg;
                bs->Read(&srcReg);
                handler->OpMovIndex(index, srcReg);
            }
            break;

            case MDST_STATIC:
            {
                uint16 index;
                bs->Read(&index);
                BCRegister srcReg;
                bs->Read(&srcReg);
                handler->OpMovStatic(index, srcReg);
            }
            break;

            case MDST_REGISTER:
                switch (srcType)
                {
                case MSRC_REGISTER:
                {
                    BCRegister dstReg;
                    bs->Read(&dstReg);
                    BCRegister srcReg;
                    bs->Read(&srcReg);
                    handler->OpMov(dstReg, srcReg);
                }
                break;

                case MSRC_ARRAYIDX:
                {
                    BCRegister dstReg;
                    bs->Read(&dstReg);
                    uint32 index;
                    bs->Read(&index);
                    BCRegister srcReg;
                    bs->Read(&srcReg);
                    handler->OpMovArrayIdx(dstReg, index, srcReg);
                }
                break;

                case MSRC_ARRAYIDX_REG:
                {
                    BCRegister dstReg;
                    bs->Read(&dstReg);
                    BCRegister indexReg;
                    bs->Read(&indexReg);
                    BCRegister srcReg;
                    bs->Read(&srcReg);
                    handler->OpMovArrayIdxReg(dstReg, indexReg, srcReg);
                }
                break;

                case MSRC_MEMBER:
                {
                    BCRegister dstReg;
                    bs->Read(&dstReg);
                    uint64 hash;
                    bs->Read(&hash);
                    BCRegister srcReg;
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

        BCRegister dstReg;
        bs->Read(&dstReg);
        BCRegister srcReg;
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
        BCRegister reg;
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
        BCRegister reg;
        bs->Read(&reg);
        // get string length
        uint32 len;
        bs->Read(&len);

        // read string based on length
        char* str = new char[len + 1];
        str[len] = '\0';
        bs->Read(str, len);

        handler->OpLoadConstantString(
            reg,
            len,
            str);

        delete[] str;

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

        handler->OpLoadArrayIdx(
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

        handler->OpLoadOffsetRef(reg, offset);

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

        handler->OpLoadFunc(reg, addr, nargs, flags);

        break;
    }
    case LOAD_CLASS:
    {
        BCRegister reg;
        bs->Read(&reg);

        uint64 nameHash;
        bs->Read(&nameHash);

        handler->OpLoadClass(reg, nameHash);

        break;
    }
    case REF:
    {
        BCRegister dstReg;
        BCRegister srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler->OpLoadRef(dstReg, srcReg);

        break;
    }
    case DEREF:
    {
        BCRegister dstReg;
        BCRegister srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler->OpLoadDeref(dstReg, srcReg);

        break;
    }
    case MOV_OFFSET:
    {
        uint16 offset;
        bs->Read(&offset);

        BCRegister reg;
        bs->Read(&reg);

        handler->OpMovOffset(offset, reg);

        break;
    }
    case MOV_INDEX:
    {
        uint16 index;
        bs->Read(&index);
        BCRegister reg;
        bs->Read(&reg);

        handler->OpMovIndex(index, reg);

        break;
    }
    case MOV_STATIC:
    {
        uint16 index;
        bs->Read(&index);

        BCRegister reg;
        bs->Read(&reg);

        handler->OpMovStatic(index, reg);

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

        handler->OpMovArrayIdx(dst, index, src);

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

        handler->OpMovArrayIdxReg(dst, indexReg, src);

        break;
    }
    case MOV:
    {
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler->OpMov(dst, src);

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

        handler->OpCheckHasMember(dst, src, hash);

        break;
    }
    case PUSH:
    {
        BCRegister reg;
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
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
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
        BCRegister reg;
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
        BCRegister dst;
        bs->Read(&dst);

        BCRegister src;
        bs->Read(&src);

        handler->OpNew(dst, src);

        break;
    }
    case NEW_ARRAY:
    {
        BCRegister dst;
        bs->Read(&dst);

        uint32 size;
        bs->Read(&size);

        handler->OpNewArray(dst, size);

        break;
    }
    case CMP:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);

        BCRegister rhsReg;
        bs->Read(&rhsReg);

        handler->OpCmp(lhsReg, rhsReg);

        break;
    }
    case BEGIN_CLASS:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler->OpBeginClass(reg);

        break;
    }
    case CMPZ:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler->OpCmpZ(reg);

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

        handler->OpAdd(lhsReg, rhsReg, dstReg);

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

        handler->OpSub(lhsReg, rhsReg, dstReg);

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

        handler->OpMul(lhsReg, rhsReg, dstReg);

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

        handler->OpDiv(lhsReg, rhsReg, dstReg);

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

        handler->OpMod(lhsReg, rhsReg, dstReg);

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

        handler->OpAnd(lhsReg, rhsReg, dstReg);

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

        handler->OpOr(lhsReg, rhsReg, dstReg);

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

        handler->OpXor(lhsReg, rhsReg, dstReg);

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

        handler->OpShl(lhsReg, rhsReg, dstReg);

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

        handler->OpShr(lhsReg, rhsReg, dstReg);

        break;
    }
    case NEG:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler->OpNeg(reg);

        break;
    }
    case NOT:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler->OpNot(reg);

        break;
    }
    case THROW:
    {
        BCRegister reg;
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
        BCRegister reg;
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
        BCRegister reg;
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

void Script_Interpreter::Invoke(Script_Instance* instance, Script_Value&& value, uint8 nargs)
{
    Script_ExecutionThread* thread = &instance->thread;
    Script_Stream* bs = &instance->stream;

    Script_Value& deref = *value.Deref();

    if (deref.IsFunction())
    {
        if (deref.IsNativeFunction())
        {
            HypData** argsHypData = (HypData**)StackAlloc((nargs > 0 ? nargs : 1) * sizeof(HypData*));

            for (int argIndex = 0; argIndex < nargs; argIndex++)
            {
                Script_Value& srcValue = *instance->thread.m_stack[instance->thread.m_stack.GetStackPointer() - int(nargs) + argIndex].Deref();

                argsHypData[argIndex] = srcValue.GetHypData();
            }

            // @TODO: Implement
            // disable auto gc so no collections happen during a native function
            //            enableAutoGc = false;

            // call the native function
            Script_VMData* vmData = deref.GetVMData();
            Assert(vmData != nullptr && vmData->nativeFunc != nullptr);

            HypData resultHypData = vmData->nativeFunc->Invoke(Span<HypData*>(argsHypData, nargs));

            // set register 0 to the result
            instance->thread.GetRegisters()[0] = ScriptApi_MakeValue(std::move(resultHypData));

            // re-enable auto gc
            //            enableAutoGc = ENABLE_GC;

            return;
        }

        // non-native function here
        Script_VMData* vmData = deref.GetVMData();
        Assert(vmData != nullptr && vmData->type == Script_VMData::FUNCTION);

        if ((vmData->func.m_flags & (uint8)HypMethodFlags::VARIADIC) && nargs < vmData->func.m_nargs - 1)
        {
            // if variadic, make sure the arg count is /at least/ what is required
            ThrowException(instance, Script_Exception::InvalidArgsException(vmData->func.m_nargs, nargs, true));
        }
        else if (!(vmData->func.m_flags & (uint8)HypMethodFlags::VARIADIC) && vmData->func.m_nargs != nargs)
        {
            ThrowException(instance, Script_Exception::InvalidArgsException(vmData->func.m_nargs, nargs));
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

                // create an array to hold variadic args
                Script_Array arr;
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
        value.GetTypeString());

    ThrowException(instance, Script_Exception(buffer));
}

void Script_Interpreter::InvokeNow(Script_Instance* instance, Script_Value&& value, uint8 nargs)
{
    Script_ExecutionThread* thread = &instance->thread;
    Script_Stream* bs = &instance->stream;

    const SizeType positionBefore = bs->Position();
    const uint32 originalFunctionDepth = instance->thread.m_funcDepth;
    const SizeType stackSizeBefore = instance->thread.GetStack().GetStackPointer();

    InstructionHandler handler(this, instance);

    Script_Value* deref = value.Deref();
    Assert(deref != nullptr);

    Script_VMData* pVmData = deref->GetVMData();
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

        const Script_Value& top = instance->thread.m_stack[sp - 1];

        const Script_VMData* topVmData = top.GetVMData();

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

        Script_Value* top = &instance->thread.m_stack.Top();
        Script_VMData* topVmData = top->GetVMData();

        while (topVmData && topVmData->type != Script_VMData::TRY_CATCH_INFO)
        {
            instance->thread.m_stack.Pop();

            top = &instance->thread.m_stack.Top();
            topVmData = top->GetVMData();
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