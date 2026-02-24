#pragma once

#include <Core/containers/String.hpp>
#include <Core/memory/Any.hpp>
#include <Core/memory/UniquePtr.hpp>

#include <script/vm/String.hpp>
#include <Core/Types.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/Defines.hpp>

#include <Core/HashCode.hpp>

#include <sstream>

namespace Hyperion {

struct BoxedValue;
struct Method;

enum class BytecodeAddress : uint32;

#ifndef INVALID_FUNCTION_ADDRESS
#define INVALID_FUNCTION_ADDRESS BytecodeAddress(~0u)
#endif

struct Script_ExecutionThread;
class VirtualMachine;
class GarbageCollector;
class Exception;

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

struct alignas(8) ScriptObjectData
{
    union
    {
        BoxedValue* valueRef;

        struct
        {
            BytecodeAddress m_addr;
            uint8 m_nargs;
            uint8 m_flags;
        } func;

        Method* nativeFunc;

        struct
        {
            BytecodeAddress returnAddress;
            int32 varargsPush;
        } call;

        BytecodeAddress addr;

        struct
        {
            BytecodeAddress catchAddress;
        } exceptionState;

        struct
        {
            const char* errorMessage; // make sure it is a string literal, as it is not managed
        } invalidStateObject;
    };

    enum class Type
    {
        Reference,
        ScriptFunction,
        NativeFunction,
        BytecodeAddress,
        StackFrame,
        ExceptionState,
        InvalidState // used for error handling in native functions
    } type;
};

enum class GCIndex : uint32;

extern bool IsGarbage(const BoxedValue& value);

extern bool IsFunction(const BoxedValue& value);
extern bool IsNativeFunction(const BoxedValue& value);

extern bool IsRef(const BoxedValue& value);
extern BoxedValue* GetRef(const BoxedValue& value);

extern BoxedValue* Deref(BoxedValue& value);
extern const BoxedValue* Deref(const BoxedValue& value);

extern void AssignValue(BoxedValue& value, BoxedValue&& other, bool assignRef);

extern bool GetUnsigned(const BoxedValue& value, uint64* out);
extern bool GetInteger(const BoxedValue& value, int64* out);
extern bool GetSignedOrUnsigned(const BoxedValue& value, Number* out);

extern bool GetFloatingPoint(const BoxedValue& value, double* out);

extern bool GetNumber(const BoxedValue& value, double* out);
extern bool GetNumber(const BoxedValue& value, Number* out);

extern NumericType GetNumericType(const BoxedValue& value);

extern bool GetBoolean(const BoxedValue& value, bool* out);

extern bool GetString(const BoxedValue& value, const ScriptString** out);

extern const Handle<ObjectBase>& GetObject(const BoxedValue& value);

extern int CompareAsPointers(const BoxedValue& lhs, const BoxedValue& rhs);
extern int CompareAsFunctions(const BoxedValue& lhs, const BoxedValue& rhs);
extern int CompareAsNativeFunctions(const BoxedValue& lhs, const BoxedValue& rhs);

extern const char* GetTypeString(const BoxedValue& value);
extern String ToString(const BoxedValue& value);

} // namespace Hyperion
