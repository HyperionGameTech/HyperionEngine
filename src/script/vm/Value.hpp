#pragma once

#include <core/utilities/TypeId.hpp>
#include <core/containers/String.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/UniquePtr.hpp>

#include <script/vm/String.hpp>
#include <core/Types.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/Defines.hpp>

#include <core/HashCode.hpp>

#include <sstream>

namespace hyperion {

struct AnyHandle;
struct HypData;
struct HypMethod;

enum class Script_FunctionAddress : uint32;

#ifndef INVALID_FUNCTION_ADDRESS
#define INVALID_FUNCTION_ADDRESS Script_FunctionAddress(~0u)
#endif

struct Script_ExecutionThread;
class Script_Interpreter;
class Script_GC;
class Script_Exception;

enum NumericType : uint8
{
    NT_INVALID = (uint8)-1,

    NT_U8 = 0,
    NT_I8,
    NT_U16,
    NT_I16,
    NT_U32,
    NT_I32,
    NT_U64,
    NT_I64,
    NT_F32,
    NT_F64
};

struct Number
{
    using Flags = uint32;

    enum FlagBits : Flags
    {
        FLAG_NONE = 0x00,
        FLAG_SIGNED = 0x01,
        FLAG_UNSIGNED = 0x02,
        FLAG_FLOATING_POINT = 0x04,

        FLAG_8_BIT = 0x08,
        FLAG_16_BIT = 0x10,
        FLAG_32_BIT = 0x20,
        FLAG_64_BIT = 0x40,

        FLAG_BIT_WIDTH_MASK = FLAG_8_BIT | FLAG_16_BIT | FLAG_32_BIT | FLAG_64_BIT
    };

    union
    {
        int64 i;
        uint64 u;
        double f;
    };

    Flags flags;

    constexpr Number()
        : i(0),
          flags(FLAG_NONE)
    {
    }

    explicit constexpr Number(NumericType numericType)
    {
        switch (numericType)
        {
        case NT_U8:
            flags = FLAG_UNSIGNED | FLAG_8_BIT;
            break;
        case NT_I8:
            flags = FLAG_SIGNED | FLAG_8_BIT;
            break;
        case NT_U16:
            flags = FLAG_UNSIGNED | FLAG_16_BIT;
            break;
        case NT_I16:
            flags = FLAG_SIGNED | FLAG_16_BIT;
            break;
        case NT_U32:
            flags = FLAG_UNSIGNED | FLAG_32_BIT;
            break;
        case NT_I32:
            flags = FLAG_SIGNED | FLAG_32_BIT;
            break;
        case NT_U64:
            flags = FLAG_UNSIGNED | FLAG_64_BIT;
            break;
        case NT_I64:
            flags = FLAG_SIGNED | FLAG_64_BIT;
            break;
        case NT_F32:
            flags = FLAG_FLOATING_POINT | FLAG_32_BIT;
            f = 0.0;
            break;
        case NT_F64:
            flags = FLAG_FLOATING_POINT | FLAG_64_BIT;
            f = 0.0;
            break;
        default:
            flags = FLAG_NONE;
            break;
        }
    }
};

enum CompareFlags : uint8
{
    CF_NONE = 0x0,
    CF_EQUAL = 0x01,
    CF_GREATER = 0x02
};

class VMObject;

struct alignas(8) Script_VMData
{
    union
    {
        HypData* valueRef;

        struct
        {
            Script_FunctionAddress m_addr;
            uint8 m_nargs;
            uint8 m_flags;
        } func;

        HypMethod* nativeFunc;

        struct
        {
            Script_FunctionAddress returnAddress;
            int32 varargsPush;
        } call;

        Script_FunctionAddress addr;

        struct
        {
            Script_FunctionAddress catchAddress;
        } tryCatchInfo;

        struct
        {
            const char* errorMessage; // make sure it is a string literal, as it is not managed
        } invalidStateObject;
    };

    enum
    {
        VALUE_REF,
        HEAP_POINTER,
        FUNCTION,
        NATIVE_FUNCTION,
        USER_DATA,
        ADDRESS,
        FUNCTION_CALL,
        TRY_CATCH_INFO,
        INVALID_STATE_OBJECT // used for error handling in native functions
    } type;
};

enum class GCIndex : uint32;

extern HYP_API bool IsGarbage(const HypData& data);

extern HYP_API bool IsFunction(const HypData& data);
extern HYP_API bool IsNativeFunction(const HypData& data);

extern HYP_API bool IsRef(const HypData& data);
extern HYP_API HypData* GetRef(const HypData& data);

extern HYP_API HypData* Deref(HypData& data);
extern HYP_API const HypData* Deref(const HypData& data);

extern HYP_API void AssignValue(HypData& data, HypData&& other, bool assignRef);

extern HYP_API bool GetUnsigned(const HypData& data, uint64* out);
extern HYP_API bool GetInteger(const HypData& data, int64* out);
extern HYP_API bool GetSignedOrUnsigned(const HypData& data, Number* out);

extern HYP_API bool GetFloatingPoint(const HypData& data, double* out);

extern HYP_API bool GetNumber(const HypData& data, double* out);
extern HYP_API bool GetNumber(const HypData& data, Number* out);

extern HYP_API NumericType GetNumericType(const HypData& data);

extern HYP_API bool GetBoolean(const HypData& data, bool* out);

extern HYP_API bool GetString(const HypData& data, const Script_String** out);

extern HYP_API const AnyHandle& ScriptApi_GetObject(const HypData& data);

extern HYP_API int CompareAsPointers(const HypData& lhs, const HypData& rhs);
extern HYP_API int CompareAsFunctions(const HypData& lhs, const HypData& rhs);
extern HYP_API int CompareAsNativeFunctions(const HypData& lhs, const HypData& rhs);

extern HYP_API const char* GetTypeString(const HypData& data);
extern HYP_API String ToString(const HypData& data);

} // namespace hyperion
