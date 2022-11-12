#ifndef VALUE_HPP
#define VALUE_HPP

#include <script/vm/HeapValue.hpp>
#include <script/vm/VMString.hpp>
#include <Types.hpp>

#include <util/Defines.hpp>

#include <sstream>
#include <utility>
#include <cstdint>


namespace hyperion {

namespace vm {

typedef UInt32 BCAddress;
typedef UInt8 BCRegister;

struct Value;

class InstructionHandler;
class ExecutionThread;
class HeapValue;
struct VMState;

struct Number
{
    using FlagBits = UInt32;

    enum Flags : FlagBits
    {
        FLAG_NONE = 0x00,
        FLAG_SIGNED = 0x01,
        FLAG_UNSIGNED = 0x02,
        FLAG_FLOATING_POINT = 0x04,

        FLAG_32_BIT = 0x08,
        FLAG_64_BIT = 0x10
    };

    FlagBits flags;

    union
    {
        Int64 i;
        UInt64 u;
        Float64 f;
    };
};

} // namespace vm

namespace sdk {

struct Params
{
    vm::InstructionHandler *handler;
    vm::Value **args;
    Int32 nargs;
};

} // namespace sdk

} // namespace hyperion

// native typedefs
typedef void(*NativeFunctionPtr_t)(hyperion::sdk::Params);
typedef void(*NativeInitializerPtr_t)(hyperion::vm::VMState *, hyperion::vm::ExecutionThread *thread, hyperion::vm::Value *);
typedef void *UserData_t;

namespace hyperion {
namespace vm {

enum CompareFlags : int
{
    NONE = 0x00,
    EQUAL = 0x01,
    GREATER = 0x02
};
    
struct Value
{
    static constexpr SizeType inline_storage_size = 64;

    enum ValueType
    {
        NONE,

        /* These first types are listed in order of precedence */
        I32,
        I64,
        U32,
        U64,
        F32,
        F64,

        BOOLEAN,

        VALUE_REF,

        HEAP_POINTER,
        FUNCTION,
        NATIVE_FUNCTION,
        USER_DATA,
        ADDRESS,
        FUNCTION_CALL,
        TRY_CATCH_INFO
    } m_type;

    union ValueData
    {
        Int32 i32;
        Int64 i64;
        UInt32 u32;
        UInt64 u64;
        Float32 f;
        Float64 d;

        bool b;

        Value *value_ref;

        HeapValue *ptr;

        struct
        {
            BCAddress m_addr;
            UInt8 m_nargs;
            UInt8 m_flags;
        } func;

        NativeFunctionPtr_t native_func;
        UserData_t user_data;
        
        struct
        {
            BCAddress return_address;
            Int32 varargs_push;
        } call;

        BCAddress addr;

        struct
        {
            BCAddress catch_address;
        } try_catch_info;

        UInt8 inline_storage[inline_storage_size];
    } m_value;

    Value() = default;
    Value(const Value &other);
    Value(ValueType value_type, ValueData value_data);

    HYP_DEF_STRUCT_COMPARE_EQL(Value);
    HYP_DEF_STRUCT_COMPARE_LT(Value);

    HYP_FORCE_INLINE Value::ValueType GetType() const { return m_type; }
    HYP_FORCE_INLINE Value::ValueData &GetValue() { return m_value; }
    HYP_FORCE_INLINE const Value::ValueData &GetValue() const { return m_value; }

    HYP_FORCE_INLINE bool GetUnsigned(UInt64 *out) const
    {
        switch (m_type) {
            case U32: *out = static_cast<UInt64>(m_value.u32); return true;
            case U64: *out = m_value.u64; return true;
            default: return false;
        }
    }

    HYP_FORCE_INLINE bool GetInteger(Int64 *out) const
    {
        switch (m_type) {
            case I32: *out = static_cast<Int64>(m_value.i32); return true;
            case I64: *out = m_value.i64; return true;
            default: return false;
        }
    }

    HYP_FORCE_INLINE bool GetSignedOrUnsigned(Number *out) const
    {
        switch (m_type) {
            case I32: out->i = static_cast<Int64>(m_value.i32); out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT; return true;
            case I64: out->i = m_value.i64; out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT; return true;
            case U32: out->u = static_cast<UInt64>(m_value.u32); out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT; return true;
            case U64: out->u = m_value.u64; out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT; return true;
            default: return false;
        }
    }

    HYP_FORCE_INLINE bool GetFloatingPoint(Float64 *out) const
    {
        switch (m_type) {
            case F32: *out = static_cast<Float64>(m_value.f); return true;
            case F64: *out = m_value.d; return true;
            default: return false;
        }
    }

    HYP_FORCE_INLINE bool GetFloatingPointCoerce(Float64 *out) const
    {
        Number num;

        if (!GetNumber(&num)) {
            return false;
        }

        if (num.flags & Number::FLAG_UNSIGNED) {
            *out = static_cast<Float64>(num.u);
        } else if (num.flags & Number::FLAG_SIGNED) {
            *out = static_cast<Float64>(num.i);
        } else {
            *out = num.f;
        }

        return true;
    }

    HYP_FORCE_INLINE bool GetNumber(Double *out) const
    {
        switch (m_type) {
            case I32: *out = static_cast<Double>(m_value.i32); return true;
            case I64: *out = static_cast<Double>(m_value.i64); return true;
            case U32: *out = static_cast<Double>(m_value.u32); return true;
            case U64: *out = static_cast<Double>(m_value.u64); return true;
            case F32: *out = static_cast<Double>(m_value.f); return true;
            case F64: *out = m_value.d; return true;
            default: return false;
        }
    }

    HYP_FORCE_INLINE bool GetNumber(Number *out) const
    {
        switch (m_type) {
            case I32: out->i = static_cast<Int64>(m_value.i32); out->flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT; return true;
            case I64: out->i = m_value.i64; out->flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT; return true;
            case U32: out->u = static_cast<UInt64>(m_value.u32); out->flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT; return true;
            case U64: out->u = m_value.u64; out->flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT; return true;
            case F32: out->f = static_cast<Double>(m_value.f); out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; return true;
            case F64: out->f = m_value.d; out->flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; return true;
            default: return false;
        }
    }

    HYP_FORCE_INLINE bool GetBoolean(bool *out) const
    {
        if (m_type == ValueType::BOOLEAN) {
            *out = m_value.b;

            return true;
        }

        return false;
    }

    template <class T>
    HYP_FORCE_INLINE bool GetPointer(T **out) const
    {
        if (m_type != HEAP_POINTER) {
            return false;
        }

        return (*out = m_value.ptr->GetPointer<T>()) != nullptr;
    }

    template <class T>
    HYP_FORCE_INLINE bool GetUserData(T **out) const
    {
        if (m_type != USER_DATA) {
            return false;
        }

        *out = static_cast<T *>(m_value.user_data);

        return true;
    }

    static int CompareAsPointers(Value *lhs, Value *rhs);

    HYP_FORCE_INLINE static int CompareAsFunctions(
        Value *lhs,
        Value *rhs
    )
    {
        return (lhs->m_value.func.m_addr == rhs->m_value.func.m_addr)
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    }

    HYP_FORCE_INLINE static int CompareAsNativeFunctions(
        Value *lhs,
        Value *rhs)
    {
        return (lhs->m_value.native_func == rhs->m_value.native_func)
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    }

    void Mark();

    const char *GetTypeString() const;
    VMString ToString() const;
    void ToRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;
};

} // namespace vm
} // namespace hyperion

#endif
