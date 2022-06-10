#ifndef VALUE_HPP
#define VALUE_HPP

#include <script/vm/ImmutableString.hpp>

#include <sstream>
#include <cstdint>


namespace hyperion {

namespace vm {

typedef uint32_t bc_address_t;
typedef uint8_t bc_reg_t;

struct Value;
class InstructionHandler;
class ExecutionThread;
class HeapValue;
struct VMState;

} // namespace vm

namespace sdk {

struct Params {
    vm::InstructionHandler *handler;
    vm::Value **args;
    int nargs;
};

} // namespace sdk

} // namespace hyperion

// native typedefs
typedef void(*NativeFunctionPtr_t)(hyperion::sdk::Params);
typedef void(*NativeInitializerPtr_t)(hyperion::vm::VMState*, hyperion::vm::ExecutionThread *thread, hyperion::vm::Value*);
typedef void *UserData_t;

namespace hyperion {
namespace vm {
    
struct Value {
    enum ValueType {
        NONE,

        /* These first four types are listed in order of precedence */
        I32,
        I64,
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

    union ValueData {
        int32_t i32;
        int64_t i64;
        float f;
        double d;

        bool b;

        Value *value_ref;

        HeapValue *ptr;

        struct {
            bc_address_t m_addr;
            uint8_t m_nargs;
            uint8_t m_flags;
        } func;

        NativeFunctionPtr_t native_func;
        UserData_t user_data;
        
        struct {
            bc_address_t return_address;
            int32_t varargs_push;
        } call;

        bc_address_t addr;

        struct {
            bc_address_t catch_address;
        } try_catch_info;
    } m_value;

    Value() = default;
    explicit Value(const Value &other);

    inline Value::ValueType GetType()  const { return m_type; }
    inline Value::ValueData GetValue() const { return m_value; }

    inline bool GetInteger(int64_t *out) const
    {
        switch (m_type) {
            case I32: *out = m_value.i32; return true;
            case I64: *out = m_value.i64; return true;
            default:                      return false;
        }
    }

    inline bool GetFloatingPoint(double *out) const
    {
        switch (m_type) {
            case F32: *out = m_value.f; return true;
            case F64: *out = m_value.d; return true;
            default:                    return false;
        }
    }

    inline bool GetNumber(double *out) const
    {
        switch (m_type) {
            case I32: *out = m_value.i32; return true;
            case I64: *out = m_value.i64; return true;
            case F32: *out = m_value.f;   return true;
            case F64: *out = m_value.d;   return true;
            default:                      return false;
        }
    }

    void Mark();

    const char *GetTypeString() const;
    ImmutableString ToString() const;
    void ToRepresentation(std::stringstream &ss,
        bool add_type_name = true) const;
};

} // namespace vm
} // namespace hyperion

#endif
