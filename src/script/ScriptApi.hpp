#ifndef SCRIPT_API_HPP
#define SCRIPT_API_HPP

#include <script/vm/Object.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/ImmutableString.hpp>
#include <script/vm/Value.hpp>

#include <script/compiler/Configuration.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/Typedefs.hpp>

#include <util/defines.h>

#include <stdint.h>
#include <sstream>

#ifndef __cplusplus
#error Script requires a C++ compiler
#endif

#if HYP_WINDOWS
#define HYP_EXPORT extern "C" __declspec(dllexport)
#else
#define HYP_EXPORT extern "C"
#endif

#pragma region Script helper macros

#define HYP_SCRIPT_PARAMS hyperion::sdk::Params

#define HYP_SCRIPT_EXPORT_FUNCTION(name) HYP_EXPORT void name(hyperion::sdk::Params params)

#define HYP_SCRIPT_FUNCTION(name) void name(hyperion::sdk::Params params)

#define HYP_SCRIPT_CHECK_ARGS(cmp, amt) \
    do { \
        if (!(params.nargs cmp amt)) { \
            params.handler->state->ThrowException(params.handler->thread, \
                hyperion::vm::Exception::InvalidArgsException(#cmp " " #amt, params.nargs)); \
            return; \
        } \
    } while (false)

#define HYP_SCRIPT_THROW(exception) \
    do { \
        params.handler->state->ThrowException( \
            params.handler->thread, \
            exception \
        ); \
        return; \
    } while (0)

#define HYP_SCRIPT_RETURN(value) \
    do { \
        params.handler->thread->GetRegisters()[0] = value; \
        return; \
    } while (false)

#define HYP_SCRIPT_RETURN_OBJECT(value) \
    do { \
        AssertThrow(value != nullptr); \
        value->Mark(); \
        vm::Value _res; \
        _res.m_type      = vm::Value::HEAP_POINTER; \
        _res.m_value.ptr = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_INT32(value) \
    do { \
        vm::Value _res; \
        _res.m_type      = vm::Value::I32; \
        _res.m_value.i32 = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_INT64(value) \
    do { \
        vm::Value _res; \
        _res.m_type      = vm::Value::I64; \
        _res.m_value.i64 = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_FLOAT32(value) \
    do { \
        vm::Value _res; \
        _res.m_type      = vm::Value::F32; \
        _res.m_value.f   = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_FLOAT64(value) \
    do { \
        vm::Value _res; \
        _res.m_type      = vm::Value::F64; \
        _res.m_value.d   = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_BOOLEAN(value) \
    do { \
        vm::Value _res; \
        _res.m_type      = vm::Value::BOOLEAN; \
        _res.m_value.b = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#pragma endregion

namespace hyperion {

namespace vm {


// forward declarations
class VM;
struct VMState;
//struct Value;
struct ExecutionThread;
struct InstructionHandler;
class ImmutableString;

} // namespace vm

using namespace compiler;

class API {

public:
    struct NativeVariableDefine {
        std::string name;
        SymbolTypePtr_t type;
        enum { INITIALIZER, VALUE } value_type;
        NativeInitializerPtr_t initializer_ptr;
        vm::Value value;
        bool is_const;
        std::shared_ptr<AstExpression> current_value; // defaults to DefaultValue() of type

        NativeVariableDefine(
            const std::string &name,
            const SymbolTypePtr_t &type,
            NativeInitializerPtr_t initializer_ptr,
            bool is_const = false
        ) : name(name),
            type(type),
            value_type(INITIALIZER),
            initializer_ptr(initializer_ptr),
            is_const(is_const)
        {
        }

        NativeVariableDefine(
            const std::string &name,
            const SymbolTypePtr_t &type,
            const vm::Value &value,
            bool is_const = false
        ) : name(name),
            type(type),
            value_type(VALUE),
            value(value),
            is_const(is_const)
        {
        }

        NativeVariableDefine(const NativeVariableDefine &other)
            : name(other.name),
              type(other.type),
              value_type(other.value_type),
              initializer_ptr(other.initializer_ptr),
              value(other.value),
              is_const(other.is_const),
              current_value(other.current_value)
        {
        }
    };

    struct NativeFunctionDefine {
        std::string function_name;
        SymbolTypePtr_t return_type;
        std::vector<GenericInstanceTypeInfo::Arg> param_types;
        NativeFunctionPtr_t ptr;

        NativeFunctionDefine() = default;

        NativeFunctionDefine(
            const std::string &function_name,
            const SymbolTypePtr_t &return_type,
            const std::vector<GenericInstanceTypeInfo::Arg> &param_types,
            NativeFunctionPtr_t ptr
        ) : function_name(function_name),
            return_type(return_type),
            param_types(param_types),
            ptr(ptr)
        {
        }

        NativeFunctionDefine(const NativeFunctionDefine &other)
            : function_name(other.function_name),
              return_type(other.return_type),
              param_types(other.param_types),
              ptr(other.ptr)
        {
        }
    };

    struct NativeMemberDefine {
        std::string name;
        enum { MEMBER_TYPE_VALUE, MEMBER_TYPE_FUNCTION } member_type;
        vm::Value value;
        SymbolTypePtr_t value_type;
        NativeFunctionDefine fn;

        NativeMemberDefine(
            const std::string &name,
            const SymbolTypePtr_t &value_type,
            const vm::Value &value
        ) : name(name),
            member_type(MEMBER_TYPE_VALUE),
            value(value),
            value_type(value_type)
        {
        }

        NativeMemberDefine(
            const std::string &name,
            const NativeFunctionDefine &fn
        ) : name(name),
            member_type(MEMBER_TYPE_FUNCTION),
            fn(fn)
        {
        }

        NativeMemberDefine(
            const std::string &name,
            const SymbolTypePtr_t &return_type,
            const std::vector<GenericInstanceTypeInfo::Arg> &param_types,
            NativeFunctionPtr_t ptr
        ) : NativeMemberDefine(
                name,
                NativeFunctionDefine(
                    name,
                    return_type,
                    param_types,
                    ptr
                )
            )
        {
        }

        NativeMemberDefine(const NativeMemberDefine &other)
            : name(other.name),
              member_type(other.member_type),
              value(other.value),
              value_type(other.value_type),
              fn(other.fn)
        {
        }
    };

    struct TypeDefine {
        std::string name;
        SymbolTypePtr_t base_class;
        std::vector<NativeMemberDefine> members;

        TypeDefine() = default;
        TypeDefine(const TypeDefine &other) = default;

        TypeDefine(
            const std::string &name,
            const SymbolTypePtr_t &base_class,
            const std::vector<NativeMemberDefine> &members
        ) : name(name),
            base_class(base_class),
            members(members)
        {
        }

        TypeDefine(
            const std::string &name,
            const std::vector<NativeMemberDefine> &members
        ) : TypeDefine(
                name,
                nullptr,
                members
            )
        {
        }
    };

    struct ModuleDefine {
    public:
        std::string m_name;
        std::vector<TypeDefine> m_type_defs;
        std::vector<NativeFunctionDefine> m_function_defs;
        std::vector<NativeVariableDefine> m_variable_defs;
        std::shared_ptr<Module> m_mod;

        ModuleDefine &Class(
            const std::string &class_name,
            const std::vector<NativeMemberDefine> &members
        );

        ModuleDefine &Class(
            const std::string &class_name,
            const SymbolTypePtr_t &base_class,
            const std::vector<NativeMemberDefine> &members
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            const SymbolTypePtr_t &variable_type,
            NativeInitializerPtr_t ptr
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            const SymbolTypePtr_t &variable_type,
            const vm::Value &value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            aint32 int_value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            aint64 int_value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            auint32 int_value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            auint64 int_value
        );

        // ModuleDefine &Variable(
        //     const std::string &variable_name,
        //     const SymbolTypePtr_t &object_type,
        //     const std::vector<NativeVariableDefine> &object_members
        // );

        ModuleDefine &Variable(
            const std::string &variable_name,
            const SymbolTypePtr_t &variable_type,
            UserData_t ptr
        );

        ModuleDefine &Function(
            const std::string &function_name,
            const SymbolTypePtr_t &return_type,
            const std::vector<GenericInstanceTypeInfo::Arg> &param_types,
            NativeFunctionPtr_t ptr
        );

        void BindAll(vm::VM *vm, CompilationUnit *compilation_unit);

    private:
        void BindNativeVariable(
            NativeVariableDefine &def,
            Module *mod,
            vm::VM *vm,
            CompilationUnit *compilation_unit
        );

        void BindNativeFunction(
            NativeFunctionDefine &def,
            Module *mod,
            vm::VM *vm,
            CompilationUnit *compilation_unit
        );

        void BindType(
            TypeDefine def,
            Module *mod,
            vm::VM *vm,
            CompilationUnit *compilation_unit
        );
    };
};

class APIInstance {
public:
    APIInstance() {}
    APIInstance(const APIInstance &other) = delete;
    ~APIInstance() {}

    API::ModuleDefine &Module(const std::string &name);

    void BindAll(vm::VM *vm, CompilationUnit *compilation_unit);

private:
    std::vector<API::ModuleDefine> m_module_defs;
};

}

namespace hyperion {

} // namespace hyperion


#endif
