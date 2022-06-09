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
        _res.m_value.f32 = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_FLOAT64(value) \
    do { \
        vm::Value _res; \
        _res.m_type      = vm::Value::F64; \
        _res.m_value.f64 = value; \
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
struct Value;
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
        NativeInitializerPtr_t initializer_ptr;

        NativeVariableDefine(
            const std::string &name,
            const SymbolTypePtr_t &type,
            NativeInitializerPtr_t initializer_ptr)
            : name(name),
              type(type),
              initializer_ptr(initializer_ptr)
        {
        }

        NativeVariableDefine(const NativeVariableDefine &other)
            : name(other.name),
              type(other.type),
              initializer_ptr(other.initializer_ptr)
        {
        }
    };

    struct NativeFunctionDefine {
        std::string function_name;
        SymbolTypePtr_t return_type;
        std::vector<GenericInstanceTypeInfo::Arg> param_types;
        NativeFunctionPtr_t ptr;

        NativeFunctionDefine(
            const std::string &function_name,
            const SymbolTypePtr_t &return_type,
            const std::vector<GenericInstanceTypeInfo::Arg> &param_types,
            NativeFunctionPtr_t ptr)
            : function_name(function_name),
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

    struct TypeDefine {
    public:
        /*std::string m_name;
        std::vector<NativeFunctionDefine> m_methods;
        std::vector<NativeVariableDefine> m_members;

        TypeDefine &Member(const std::string &member_name,
            const SymbolTypePtr_t &member_type,
            vm::NativeInitializerPtr_t ptr);

        TypeDefine &Method(const std::string &method_name,
            const SymbolTypePtr_t &return_type,
            const std::vector<std::pair<std::string, SymbolTypePtr_t>> &param_types,
            vm::NativeFunctionPtr_t ptr);*/

        SymbolTypePtr_t m_type;
    };

    struct ModuleDefine {
    public:
        std::string m_name;
        std::vector<TypeDefine> m_type_defs;
        std::vector<NativeFunctionDefine> m_function_defs;
        std::vector<NativeVariableDefine> m_variable_defs;

        ModuleDefine &Type(const SymbolTypePtr_t &type);

        ModuleDefine &Variable(const std::string &variable_name,
            const SymbolTypePtr_t &variable_type,
            NativeInitializerPtr_t ptr);

        ModuleDefine &Function(const std::string &function_name,
            const SymbolTypePtr_t &return_type,
            const std::vector<GenericInstanceTypeInfo::Arg> &param_types,
            NativeFunctionPtr_t ptr);

        void BindAll(vm::VM *vm, CompilationUnit *compilation_unit);

    private:
        void BindNativeVariable(const NativeVariableDefine &def,
            Module *mod, vm::VM *vm, CompilationUnit *compilation_unit);

        void BindNativeFunction(const NativeFunctionDefine &def,
            Module *mod, vm::VM *vm, CompilationUnit *compilation_unit);

        void BindType(TypeDefine def,
            Module *mod, vm::VM *vm, CompilationUnit *compilation_unit);

        std::shared_ptr<compiler::Module> m_created_module;
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
