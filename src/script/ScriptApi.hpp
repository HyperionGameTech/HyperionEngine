#ifndef SCRIPT_API_HPP
#define SCRIPT_API_HPP

#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/InstructionHandler.hpp>

#include <script/compiler/Configuration.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/lib/Variant.hpp>
#include <core/lib/TypeMap.hpp>

#include <Types.hpp>

#include <util/Defines.hpp>

#include <unordered_map>
#include <list>

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
        params.handler->state->ThrowException(\
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

#define HYP_SCRIPT_RETURN_PTR(value) \
    do { \
        AssertThrow(value != nullptr); \
        value->Mark(); \
        vm::Value _res; \
        _res.m_type = vm::Value::HEAP_POINTER; \
        _res.m_value.ptr = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_NULL() \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::HEAP_POINTER; \
        _res.m_value.ptr = nullptr; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_VOID(value) \
    do { \
        (void)(value); \
        vm::Value _res; \
        _res.m_type = vm::Value::NONE; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_INT32(value) \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::I32; \
        _res.m_value.i32 = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_INT64(value) \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::I64; \
        _res.m_value.i64 = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_UINT32(value) \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::U32; \
        _res.m_value.u32 = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_UINT64(value) \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::U64; \
        _res.m_value.u64 = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_FLOAT32(value) \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::F32; \
        _res.m_value.f = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_FLOAT64(value) \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::F64; \
        _res.m_value.d = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_RETURN_BOOLEAN(value) \
    do { \
        vm::Value _res; \
        _res.m_type = vm::Value::BOOLEAN; \
        _res.m_value.b = value; \
        HYP_SCRIPT_RETURN(_res); \
    } while (false)

#define HYP_SCRIPT_GET_ARG_INT(index, name) \
    Int64 name; \
    do { \
        vm::Number num; \
        if (!params.args[index]->GetSignedOrUnsigned(&num)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type int or uint")); \
            return (decltype(name))0; \
        } \
        name = (num.flags & vm::Number::FLAG_UNSIGNED) ? static_cast<Int64>(num.u) : num.i; \
    } while (false)

#define HYP_SCRIPT_GET_ARG_UINT(index, name) \
    UInt64 name; \
    do { \
        vm::Number num; \
        if (!params.args[index]->GetSignedOrUnsigned(&num)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type int or uint")); \
            return (decltype(name))0; \
        } \
        name = (num.flags & vm::Number::FLAG_SIGNED) ? static_cast<UInt64>(num.i) : num.u; \
    } while (false)

#define HYP_SCRIPT_GET_ARG_BOOLEAN(index, name) \
    bool name; \
    do { \
        if (!params.args[index]->GetBoolean(&name)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type bool")); \
            return (decltype(name))0; \
        } \
    } while (false)

#define HYP_SCRIPT_GET_ARG_FLOAT(index, name) \
    Float64 name; \
    do { \
        if (!params.args[index]->GetFloatingPointCoerce(&name)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type float")); \
            return (decltype(name))0; \
        } \
    } while (false) 

#define HYP_SCRIPT_GET_ARG_PTR(index, type, name) \
    type *name; \
    do { \
        if (!params.args[index]->GetPointer<type>(&name)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type " #type)); \
        } \
    } while (false)

#define HYP_SCRIPT_GET_ARG_PTR_RAW(index, type, name) \
    type *name; \
    do { \
        if (!params.args[index]->GetUserData<type>(&name)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type " #type)); \
            return (decltype(name))nullptr; \
        } \
    } while (false)

#define HYP_SCRIPT_GET_MEMBER_INT(object, name, type, decl_name) \
    type decl_name; \
    do { \
        vm::Number num; \
        vm::Member *_member = nullptr; \
        if (!(_member = object->LookupMemberFromHash(hash_fnv_1(name))) || !_member->value.GetSignedOrUnsigned(&num)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type int or uint")); \
            return; \
        } \
        decl_name = (num.flags & vm::Number::FLAG_UNSIGNED) ? static_cast<Int64>(num.u) : num.i; \
    } while (false)

#define HYP_SCRIPT_GET_MEMBER_UINT(object, name, type, decl_name) \
    type decl_name; \
    do { \
        UInt64 num; \
        vm::Member *_member = nullptr; \
        if (!(_member = object->LookupMemberFromHash(hash_fnv_1(name))) || !_member->value.GetUnsigned(&num)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type uint")); \
            return; \
        } \
        decl_name = static_cast<type>(num); \
    } while (false)

#define HYP_SCRIPT_GET_MEMBER_FLOAT(object, name, type, decl_name) \
    type decl_name; \
    do { \
        Float64 num; \
        vm::Member *_member = nullptr; \
        if (!(_member = object->LookupMemberFromHash(hash_fnv_1(name))) || !_member->value.GetFloatingPointCoerce(&num)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type float")); \
            return; \
        } \
        decl_name = static_cast<type>(num); \
    } while (false)

#define HYP_SCRIPT_GET_MEMBER_PTR(object, name, type, decl_name) \
    type *decl_name; \
    do { \
        vm::Member *_member = nullptr; \
        if (!(_member = object->LookupMemberFromHash(hash_fnv_1(name))) || !_member->value.GetPointer<type>(&decl_name)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type " #type)); \
        } \
    } while (false)

#define HYP_SCRIPT_GET_MEMBER_PTR_RAW(object, name, type, decl_name) \
    type *decl_name; \
    do { \
        vm::Member *_member = nullptr; \
        if (!(_member = object->LookupMemberFromHash(hash_fnv_1(name))) || !_member->value.GetUserData<type>(&decl_name)) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type " #type)); \
            return; \
        } \
    } while (false)

#define HYP_SCRIPT_CREATE_PTR(assignment, out_name) \
    vm::Value out_name; \
    do { \
        vm::HeapValue *ptr_result = params.handler->state->HeapAlloc(params.handler->thread); \
        AssertThrow(ptr_result != nullptr); \
        \
        ptr_result->Assign((assignment)); \
        ptr_result->Mark(); \
        \
        out_name = vm::Value(vm::Value::HEAP_POINTER, {.ptr = ptr_result}); \
    } while (false)

#define HYP_SCRIPT_SET_MEMBER(object, name_str, assignment) \
    do { \
        auto *member = object.LookupMemberFromHash(hash_fnv_1(name_str)); \
        AssertThrowMsg(member != nullptr, "Member " name_str " not set on object, unmatching prototype definition");\
        member->value = assignment; \
    } while (false)

#pragma endregion

namespace hyperion {
namespace vm {
// forward declarations
class VM;
struct VMState;
//struct Value;
struct ExecutionThread;
class InstructionHandler;
class VMString;

} // namespace vm

using namespace compiler;

class APIInstance;

class API
{
public:
    struct NativeVariableDefine
    {
        std::string name;
        SymbolTypePtr_t type;
        enum { INITIALIZER, VALUE } value_type;
        NativeInitializerPtr_t initializer_ptr;
        vm::Value value;
        bool is_const;
        RC<AstExpression> current_value; // defaults to DefaultValue() of type

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

    struct NativeFunctionDefine
    {
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

    struct NativeMemberDefine
    {
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
            value(vm::Value::NONE, vm::Value::ValueData { .ptr = nullptr }),
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

    struct TypeDefine
    {
        std::string name;
        SymbolTypePtr_t base_class;
        std::vector<NativeMemberDefine> members;
        std::vector<NativeMemberDefine> static_members;

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
            const SymbolTypePtr_t &base_class,
            const std::vector<NativeMemberDefine> &members,
            const std::vector<NativeMemberDefine> &static_members
        ) : name(name),
            base_class(base_class),
            members(members),
            static_members(static_members)
        {
        }

        TypeDefine(
            const std::string &name,
            const std::vector<NativeMemberDefine> &members,
            const std::vector<NativeMemberDefine> &static_members
        ) : TypeDefine(
                name,
                nullptr,
                members,
                static_members
            )
        {
        }

        TypeDefine(
            const std::string &name,
            const std::vector<NativeMemberDefine> &members
        ) : name(name),
            base_class(nullptr),
            members(members)
        {
        }
    };

    struct ModuleDefine {
    public:
        ModuleDefine(
            APIInstance &api_instance,
            const std::string &name
        ) : m_api_instance(api_instance),
            m_name(name)
        {
        }

        std::string m_name;
        std::vector<TypeDefine> m_type_defs;
        std::vector<NativeFunctionDefine> m_function_defs;
        std::vector<NativeVariableDefine> m_variable_defs;
        RC<Module> m_mod;

        template <class T>
        ModuleDefine &Class(
            const std::string &class_name,
            const SymbolTypePtr_t &base_class,
            const std::vector<NativeMemberDefine> &members
        );

        template <class T>
        ModuleDefine &Class(
            const std::string &class_name,
            const SymbolTypePtr_t &base_class,
            const std::vector<NativeMemberDefine> &members,
            const std::vector<NativeMemberDefine> &static_members
        );

        template <class T>
        ModuleDefine &Class(
            const std::string &class_name,
            const std::vector<NativeMemberDefine> &members
        );

        template <class T>
        ModuleDefine &Class(
            const std::string &class_name,
            const std::vector<NativeMemberDefine> &members,
            const std::vector<NativeMemberDefine> &static_members
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
            Int32 value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            Int64 alue
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            UInt32 value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            UInt64 value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            Float value
        );

        ModuleDefine &Variable(
            const std::string &variable_name,
            bool value
        );

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

        void BindAll(
            APIInstance &api_instance,
            vm::VM *vm,
            CompilationUnit *compilation_unit
        );

    private:
        void BindNativeVariable(
            APIInstance &api_instance,
            NativeVariableDefine &def,
            Module *mod,
            vm::VM *vm,
            CompilationUnit *compilation_unit
        );

        void BindNativeFunction(
            APIInstance &api_instance,
            NativeFunctionDefine &def,
            Module *mod,
            vm::VM *vm,
            CompilationUnit *compilation_unit
        );

        void BindType(
            APIInstance &api_instance,
            TypeDefine def,
            Module *mod,
            vm::VM *vm,
            CompilationUnit *compilation_unit
        );

        APIInstance &m_api_instance;
    };
};

class APIInstance
{
public:
    struct ClassBindings
    {
        TypeMap<std::string> class_names;
        std::unordered_map<std::string, vm::HeapValue *> class_prototypes;
    } class_bindings;

    APIInstance(const SourceFile &source_file);
    APIInstance(const APIInstance &other) = delete;
    ~APIInstance() = default;

    const SourceFile &GetSourceFile() const
        { return m_source_file; }

    API::ModuleDefine &Module(const std::string &name);

    void BindAll(
        vm::VM *vm,
        CompilationUnit *compilation_unit
    );

private:
    SourceFile m_source_file;
    std::list<API::ModuleDefine> m_module_defs;
};

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const std::string &class_name,
    const SymbolTypePtr_t &base_class,
    const std::vector<NativeMemberDefine> &members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.push_back(TypeDefine(
        class_name,
        base_class,
        members
    ));

    return *this;
}

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const std::string &class_name,
    const SymbolTypePtr_t &base_class,
    const std::vector<NativeMemberDefine> &members,
    const std::vector<NativeMemberDefine> &static_members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.push_back(TypeDefine(
        class_name,
        base_class,
        members,
        static_members
    ));

    return *this;
}

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const std::string &class_name,
    const std::vector<NativeMemberDefine> &members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.push_back(TypeDefine(
        class_name,
        members
    ));

    return *this;
}

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const std::string &class_name,
    const std::vector<NativeMemberDefine> &members,
    const std::vector<NativeMemberDefine> &static_members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.push_back(TypeDefine(
        class_name,
        members,
        static_members
    ));

    return *this;
}

}

#endif
