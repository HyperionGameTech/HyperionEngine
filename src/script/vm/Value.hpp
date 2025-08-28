#pragma once

#include <core/utilities/TypeId.hpp>
#include <core/containers/String.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/UniquePtr.hpp>

#include <script/vm/VMString.hpp>
#include <core/Types.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/Defines.hpp>

#include <core/HashCode.hpp>

#include <sstream>

namespace hyperion {

class APIInstance;
class AnyHandle;
struct HypData;
struct HypMethod;

namespace vm {

typedef uint8 BCRegister;

#ifdef HYP_SCRIPT
enum class Script_FunctionAddress : uint32;
#ifndef INVALID_FUNCTION_ADDRESS
#define INVALID_FUNCTION_ADDRESS Script_FunctionAddress(~0u)
#endif
#endif

struct Value;

class InstructionHandler;
struct Script_ExecutionThread;
class VM;
class GC;
class Exception;

enum NumericType : uint8
{
    NT_NONE = 0,

    NT_I8,
    NT_I16,
    NT_I32,
    NT_I64,
    NT_U8,
    NT_U16,
    NT_U32,
    NT_U64,
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
        FLAG_64_BIT = 0x40
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
        case NT_I8:
            flags = FLAG_SIGNED | FLAG_8_BIT;
            break;
        case NT_I16:
            flags = FLAG_SIGNED | FLAG_16_BIT;
            break;
        case NT_I32:
            flags = FLAG_SIGNED | FLAG_32_BIT;
            break;
        case NT_I64:
            flags = FLAG_SIGNED | FLAG_64_BIT;
            break;
        case NT_U8:
            flags = FLAG_UNSIGNED | FLAG_8_BIT;
            break;
        case NT_U16:
            flags = FLAG_UNSIGNED | FLAG_16_BIT;
            break;
        case NT_U32:
            flags = FLAG_UNSIGNED | FLAG_32_BIT;
            break;
        case NT_U64:
            flags = FLAG_UNSIGNED | FLAG_64_BIT;
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

} // namespace vm

namespace sdk {

struct Params
{
    APIInstance& apiInstance;
    vm::Value** args;
    int32 nargs;

    void* ctx; // needs to be passed to the function pointers below.

    // sets the return value of a native function.
    void (*setReturnValue)(void* ctx, vm::Value&& value);
    void (*throwException)(void* ctx, const vm::Exception& exception);
};

} // namespace sdk
} // namespace hyperion

// native typedefs
typedef void (*Script_NativeFunction)(hyperion::sdk::Params);
typedef void* Script_UserData;

namespace hyperion {
namespace vm {

enum CompareFlags : uint8
{
    NONE = 0x00,
    EQUAL = 0x01,
    GREATER = 0x02
};

class VMObject;
class VMArray;

struct alignas(8) Script_VMData
{
    union
    {
        Value* valueRef;

        struct
        {
            Script_FunctionAddress m_addr;
            uint8 m_nargs;
            uint8 m_flags;
        } func;

        HypMethod* nativeFunc;
        Script_UserData userData;

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
static constexpr GCIndex INVALID_GC_INDEX = GCIndex(0);

class alignas(8) Value
{
    friend class VM;
    friend class GC;

    char m_internal[32];
    GCIndex m_gcIndex;

public:
    Value();

    explicit Value(HypData&& data);
    explicit Value(Number number);
    explicit Value(const Script_VMData& vmData);

    Value(const Value& other);
    Value(Value&& other) noexcept;

    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;

    ~Value();

    HYP_FORCE_INLINE GCIndex GetGCIndex() const
    {
        return m_gcIndex;
    }

    Script_VMData* GetVMData();
    const Script_VMData* GetVMData() const;

    HypData* GetHypData();
    const HypData* GetHypData() const;

    bool IsValid() const;
    bool IsGarbage() const;

    bool IsFunction() const;
    bool IsNativeFunction() const;

    bool IsRef() const;
    Value* GetRef() const;

    Value* Deref()
    {
        Value* deref = GetRef();
        if (deref != nullptr)
        {
            return deref;
        }

        return this;
    }

    const Value* Deref() const
    {
        const Value* deref = GetRef();

        if (deref != nullptr)
        {
            return deref;
        }

        return this;
    }

    void AssignValue(Value&& other, bool assignRef);

    bool GetUnsigned(uint64* out) const;
    bool GetInteger(int64* out) const;
    bool GetSignedOrUnsigned(Number* out) const;

    bool GetFloatingPoint(double* out) const;
    bool GetFloatingPointCoerce(double* out) const;

    bool GetNumber(double* out) const;
    bool GetNumber(Number* out) const;

    NumericType GetNumericType() const;

    bool GetBoolean(bool* out) const;

    const AnyHandle& GetObject() const;
    VMArray* GetArray() const;

    AnyRef ToRef() const;

    Script_UserData GetUserData() const;

    static int CompareAsPointers(Value* lhs, Value* rhs);
    static int CompareAsFunctions(Value* lhs, Value* rhs);
    static int CompareAsNativeFunctions(Value* lhs, Value* rhs);

    HYP_FORCE_INLINE bool operator==(const Value& other) const
    {
        return CompareAsPointers(const_cast<Value*>(this), const_cast<Value*>(&other)) & CompareFlags::EQUAL;
    }

    HYP_FORCE_INLINE bool operator!=(const Value& other) const
    {
        return !(*this == other);
    }

#if 0
    Any ToAny() const;
    AnyPtr ToAnyPtr() const;
#endif

    void Mark();

    const char* GetTypeString() const;
    VMString ToString() const;
    void ToRepresentation(
        std::stringstream& ss,
        bool addTypeName = true,
        int depth = 3) const;
};

} // namespace vm
} // namespace hyperion
