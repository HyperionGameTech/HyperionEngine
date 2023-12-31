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
#include <core/lib/DynArray.hpp>
#include <core/lib/LinkedList.hpp>
#include <core/lib/String.hpp>

#include <Types.hpp>

#include <util/Defines.hpp>

#include <unordered_map>

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



#define HYP_SCRIPT_GET_ARG_INT_1(index, name) \
    Int64 name; \
    do { \
        auto _value = ConvertScriptObject<Int64>(params.api_instance, *params.args[index]); \
        if (!_value.HasValue()) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be numeric")); \
            return (decltype(name))0; \
        } \
        name = _value.Get(); \
    } while (false)

#define HYP_SCRIPT_GET_ARG_UINT_1(index, name) \
    UInt64 name; \
    do { \
        auto _value = ConvertScriptObject<UInt64>(params.api_instance, *params.args[index]); \
        if (!_value.HasValue()) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be number")); \
            return (decltype(name))0; \
        } \
        name = _value.Get(); \
    } while (false)

#define HYP_SCRIPT_GET_ARG_BOOLEAN_1(index, name) \
    Bool name; \
    do { \
        auto _value = ConvertScriptObject<Bool>(params.api_instance, *params.args[index]); \
        if (!_value.HasValue()) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type bool")); \
            return (decltype(name))0; \
        } \
        name = _value.Get(); \
    } while (false)

#define HYP_SCRIPT_GET_ARG_FLOAT_1(index, name) \
    Double name; \
    do { \
        auto _value = ConvertScriptObject<Double>(params.api_instance, *params.args[index]); \
        if (!_value.HasValue()) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be numeric")); \
            return (decltype(name))0; \
        } \
        name = _value.Get(); \
    } while (false)

#define HYP_SCRIPT_GET_ARG_PTR_1(index, type, name) \
    type *name; \
    do { \
        auto _value = ConvertScriptObject<type *>(params.api_instance, *params.args[index]); \
        if (!_value.HasValue()) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be a pointer of type " #type)); \
        } \
        name = _value.Get(); \
    } while (false)

#define HYP_SCRIPT_GET_ARG_PTR_RAW_1(index, type, name) \
    type *name; \
    do { \
        auto _value = ConvertScriptObject<void *>(params.api_instance, *params.args[index]); \
        if (!_value.HasValue()) { \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be userdata (void *)")); \
            return (decltype(name))0; \
        } \
        name = static_cast<type *>(_value.Get()); \
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
        AssertThrowMsg(member != nullptr, "Member " name_str " not set on object, unmatching prototype definition"); \
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

template <class T>
constexpr bool is_vm_object_type = std::is_same_v<vm::VMString, T> || std::is_same_v<vm::VMObject, T> || std::is_same_v<vm::VMArray, T> || std::is_same_v<vm::VMStruct, T>;

class API
{
public:
    struct NativeVariableDefine
    {
        String name;
        SymbolTypePtr_t type;
        enum { INITIALIZER, VALUE } value_type;
        NativeInitializerPtr_t initializer_ptr;
        vm::Value value;
        bool is_const;
        RC<AstExpression> current_value; // defaults to DefaultValue() of type

        NativeVariableDefine(
            const String &name,
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
            const String &name,
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
        String function_name;
        SymbolTypePtr_t return_type;
        Array<GenericInstanceTypeInfo::Arg> param_types;
        NativeFunctionPtr_t ptr;

        NativeFunctionDefine() = default;

        NativeFunctionDefine(
            const String &function_name,
            const SymbolTypePtr_t &return_type,
            const Array<GenericInstanceTypeInfo::Arg> &param_types,
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
        String name;
        enum { MEMBER_TYPE_VALUE, MEMBER_TYPE_FUNCTION } member_type;
        vm::Value value;
        SymbolTypePtr_t value_type;
        NativeFunctionDefine fn;

        NativeMemberDefine(
            const String &name,
            const SymbolTypePtr_t &value_type,
            const vm::Value &value
        ) : name(name),
            member_type(MEMBER_TYPE_VALUE),
            value(value),
            value_type(value_type)
        {
        }

        NativeMemberDefine(
            const String &name,
            const NativeFunctionDefine &fn
        ) : name(name),
            member_type(MEMBER_TYPE_FUNCTION),
            value(vm::Value::NONE, vm::Value::ValueData { .ptr = nullptr }),
            fn(fn)
        {
        }

        NativeMemberDefine(
            const String &name,
            const SymbolTypePtr_t &return_type,
            const Array<GenericInstanceTypeInfo::Arg> &param_types,
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
        String name;
        SymbolTypePtr_t base_class;
        Array<NativeMemberDefine> members;
        Array<NativeMemberDefine> static_members;

        TypeDefine() = default;
        TypeDefine(const TypeDefine &other) = default;

        TypeDefine(
            const String &name,
            const SymbolTypePtr_t &base_class,
            const Array<NativeMemberDefine> &members
        ) : name(name),
            base_class(base_class),
            members(members)
        {
        }

        TypeDefine(
            const String &name,
            const SymbolTypePtr_t &base_class,
            const Array<NativeMemberDefine> &members,
            const Array<NativeMemberDefine> &static_members
        ) : name(name),
            base_class(base_class),
            members(members),
            static_members(static_members)
        {
        }

        TypeDefine(
            const String &name,
            const Array<NativeMemberDefine> &members,
            const Array<NativeMemberDefine> &static_members
        ) : TypeDefine(
                name,
                nullptr,
                members,
                static_members
            )
        {
        }

        TypeDefine(
            const String &name,
            const Array<NativeMemberDefine> &members
        ) : name(name),
            base_class(nullptr),
            members(members)
        {
        }
    };

    struct ModuleDefine
    {
    public:
        ModuleDefine(
            APIInstance &api_instance,
            const String &name
        ) : m_api_instance(api_instance),
            m_name(name)
        {
        }

        String m_name;
        Array<TypeDefine> m_type_defs;
        Array<NativeFunctionDefine> m_function_defs;
        Array<NativeVariableDefine> m_variable_defs;
        RC<Module> m_mod;

        template <class T>
        ModuleDefine &Class(
            const String &class_name,
            const SymbolTypePtr_t &base_class,
            const Array<NativeMemberDefine> &members
        );

        template <class T>
        ModuleDefine &Class(
            const String &class_name,
            const SymbolTypePtr_t &base_class,
            const Array<NativeMemberDefine> &members,
            const Array<NativeMemberDefine> &static_members
        );

        template <class T>
        ModuleDefine &Class(
            const String &class_name,
            const Array<NativeMemberDefine> &members
        );

        template <class T>
        ModuleDefine &Class(
            const String &class_name,
            const Array<NativeMemberDefine> &members,
            const Array<NativeMemberDefine> &static_members
        );

        ModuleDefine &Variable(
            const String &variable_name,
            const SymbolTypePtr_t &variable_type,
            NativeInitializerPtr_t ptr
        );

        ModuleDefine &Variable(
            const String &variable_name,
            const SymbolTypePtr_t &variable_type,
            const vm::Value &value
        );

        ModuleDefine &Variable(
            const String &variable_name,
            Int32 value
        );

        ModuleDefine &Variable(
            const String &variable_name,
            Int64 alue
        );

        ModuleDefine &Variable(
            const String &variable_name,
            UInt32 value
        );

        ModuleDefine &Variable(
            const String &variable_name,
            UInt64 value
        );

        ModuleDefine &Variable(
            const String &variable_name,
            Float value
        );

        ModuleDefine &Variable(
            const String &variable_name,
            bool value
        );

        ModuleDefine &Variable(
            const String &variable_name,
            const SymbolTypePtr_t &variable_type,
            UserData_t ptr
        );

        ModuleDefine &Function(
            const String &function_name,
            const SymbolTypePtr_t &return_type,
            const Array<GenericInstanceTypeInfo::Arg> &param_types,
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

#pragma region Script API Instance

// class ClassBindingMap : TypeMap<RC<ClassBase>>
// {
//     template <class T>
//     void SetClass(RC<ClassBase> class_object, vm::HeapValue *hv)
//     {
//         AssertThrow(class_object != nullptr);
//         AssertThrowMsg(class_object->GetScriptHeapValue() == nullptr, "Class already has heap value assigned");

//         class_object->SetScriptHeapValue(hv);

//         TypeMap::template Set<T>(std::move(class_object));
//     }

//     template <class T>
//     const RC<ClassBase> &GetClass() const
//         { return TypeMap::template Get<T>(); }
// };

class APIInstance
{
public:
    struct ClassBindings
    {
        TypeMap<String>                     class_names;
        HashMap<String, vm::HeapValue *>    class_prototypes;
    } class_bindings;

    APIInstance(const SourceFile &source_file);
    APIInstance(const APIInstance &other) = delete;
    ~APIInstance() = default;

    vm::VM *GetVM() const
        { return m_vm; }

    const SourceFile &GetSourceFile() const
        { return m_source_file; }

    API::ModuleDefine &Module(const String &name);

    void BindAll(
        vm::VM *vm,
        CompilationUnit *compilation_unit
    );

private:
    vm::VM *m_vm;
    SourceFile m_source_file;
    LinkedList<API::ModuleDefine> m_module_defs;
};

#pragma endregion

#pragma region ModuleDefine helper methods

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const String &class_name,
    const SymbolTypePtr_t &base_class,
    const Array<NativeMemberDefine> &members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.PushBack(TypeDefine(
        class_name,
        base_class,
        members
    ));

    return *this;
}

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const String &class_name,
    const SymbolTypePtr_t &base_class,
    const Array<NativeMemberDefine> &members,
    const Array<NativeMemberDefine> &static_members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.PushBack(TypeDefine(
        class_name,
        base_class,
        members,
        static_members
    ));

    return *this;
}

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const String &class_name,
    const Array<NativeMemberDefine> &members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.PushBack(TypeDefine(
        class_name,
        members
    ));

    return *this;
}

template <class T>
API::ModuleDefine &API::ModuleDefine::Class(
    const String &class_name,
    const Array<NativeMemberDefine> &members,
    const Array<NativeMemberDefine> &static_members
)
{
    m_api_instance.class_bindings.class_names.Set<T>(class_name);

    m_type_defs.PushBack(TypeDefine(
        class_name,
        members,
        static_members
    ));

    return *this;
}

#pragma endregion


#pragma region Conversion Helpers

template <class T>
struct CxxToScriptValueImpl;

template <class T>
static inline vm::Value CxxToScriptValueInternal(APIInstance &api_instance, T &&value)
{
    CxxToScriptValueImpl<NormalizedType<T>> impl;

    return impl(api_instance, std::forward<T>(value));
}

template <class T>
struct CxxToScriptValueImpl
{
    static_assert(std::is_class_v<T>, "Should not receive a VM object type as it is handled by other specializations");
    static_assert(!is_vm_object_type<T>, "Should not receive a VM object type as it is handled by other specializations");

    // use a template param to allow const references and similar but still having T be decayed
    template <class Ty>
    vm::Value operator()(APIInstance &api_instance, Ty &&value) const
    {
        static_assert(!IsDynArray<T>::value, "other specialization should handle dynarrays");
        static_assert(std::is_convertible_v<Ty, T>, "must be convertible; unrelated types T and Ty");

        const auto class_name_it = api_instance.class_bindings.class_names.Find<T>();
        AssertThrowMsg(class_name_it != api_instance.class_bindings.class_names.End(), "Class not registered!");

        const auto prototype_it = api_instance.class_bindings.class_prototypes.Find(class_name_it->second);
        AssertThrowMsg(prototype_it != api_instance.class_bindings.class_prototypes.End(), "Class not registered!");

        vm::Value intern_value;
        {
            vm::HeapValue *ptr_result = api_instance.GetVM()->GetState().HeapAlloc(api_instance.GetVM()->GetState().GetMainThread());
            AssertThrow(ptr_result != nullptr);

            ptr_result->Assign(std::forward<Ty>(value));
            ptr_result->Mark();
            intern_value = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptr_result });
        }

        vm::VMObject boxed_value(prototype_it->second);
        HYP_SCRIPT_SET_MEMBER(boxed_value, "__intern", intern_value);

        vm::Value final_value;
        {
            vm::HeapValue *ptr_result = api_instance.GetVM()->GetState().HeapAlloc(api_instance.GetVM()->GetState().GetMainThread());
            AssertThrow(ptr_result != nullptr);

            ptr_result->Assign(boxed_value);
            ptr_result->Mark();
            final_value = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptr_result });
        }

        return final_value;
    }
};

template <>
struct CxxToScriptValueImpl<Int8>
{
    vm::Value operator()(APIInstance &, Int8 value) const
    {
        return vm::Value(vm::Value::I8, { .i8 = value });
    }
};

template <>
struct CxxToScriptValueImpl<Int16>
{
    vm::Value operator()(APIInstance &, Int16 value) const
    {
        return vm::Value(vm::Value::I16, { .i16 = value });
    }
};

template <>
struct CxxToScriptValueImpl<Int32>
{
    vm::Value operator()(APIInstance &, Int32 value) const
    {
        return vm::Value(vm::Value::I32, { .i32 = value });
    }
};

template <>
struct CxxToScriptValueImpl<Int64>
{
    vm::Value operator()(APIInstance &, Int64 value) const
    {
        return vm::Value(vm::Value::I64, { .i64 = value });
    }
};

template <>
struct CxxToScriptValueImpl<UInt8>
{
    vm::Value operator()(APIInstance &, UInt8 value) const
    {
        return vm::Value(vm::Value::U8, { .u8 = value });
    }
};

template <>
struct CxxToScriptValueImpl<UInt16>
{
    vm::Value operator()(APIInstance &, UInt16 value) const
    {
        return vm::Value(vm::Value::U16, { .u16 = value });
    }
};

template <>
struct CxxToScriptValueImpl<UInt32>
{
    vm::Value operator()(APIInstance &, UInt32 value) const
    {
        return vm::Value(vm::Value::U32, { .u32 = value });
    }
};

template <>
struct CxxToScriptValueImpl<UInt64>
{
    vm::Value operator()(APIInstance &, UInt64 value) const
    {
        return vm::Value(vm::Value::U64, { .u64 = value });
    }
};

template <>
struct CxxToScriptValueImpl<Float>
{
    vm::Value operator()(APIInstance &, Float value) const
    {
        return vm::Value(vm::Value::F32, { .f = value });
    }
};

template <>
struct CxxToScriptValueImpl<Double>
{
    vm::Value operator()(APIInstance &, Double value) const
    {
        return vm::Value(vm::Value::F64, { .d = value });
    }
};

template <>
struct CxxToScriptValueImpl<Bool>
{
    vm::Value operator()(APIInstance &, Bool value) const
    {
        return vm::Value(vm::Value::BOOLEAN, { .b = value });
    }
};

template <>
struct CxxToScriptValueImpl<String>
{
    vm::Value operator()(APIInstance &api_instance, const String &value) const
    {
        vm::VMString str = value;

        vm::Value final_value;
        {
            vm::HeapValue *ptr_result = api_instance.GetVM()->GetState().HeapAlloc(api_instance.GetVM()->GetState().GetMainThread());
            AssertThrow(ptr_result != nullptr);

            ptr_result->Assign(std::move(str));
            ptr_result->Mark();
            final_value = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptr_result });
        }

        return final_value;
    }
};

// @TODO DynArray -> VMArray


template <class T>
struct ConvertImpl;

template <class T>
static inline auto ConvertScriptObjectInternal(APIInstance &api_instance, const vm::Value &value)
{
    ConvertImpl<T> impl;

    return impl(api_instance, value);
}

template <class T, class Ty = NormalizedType<T>>
static inline auto ConvertScriptObject(APIInstance &api_instance, const vm::Value &value)
    -> std::enable_if_t<!std::is_enum_v<Ty>, decltype(ConvertScriptObjectInternal<Ty>(api_instance, value))>
{
    return ConvertScriptObjectInternal<Ty>(api_instance, value);
}

template <class T, class Ty = NormalizedType<T>>
static inline auto ConvertScriptObject(APIInstance &api_instance, const vm::Value &value)
    -> std::enable_if_t<std::is_enum_v<Ty>, decltype(ConvertScriptObjectInternal<std::underlying_type_t<Ty>>(api_instance, value))>
{
    return ConvertScriptObjectInternal<std::underlying_type_t<Ty>>(api_instance, value);
}

template <class T>
struct ConvertImpl
{
    static_assert(!is_vm_object_type<NormalizedType<T>>, "Should not receive a VM object type as it is handled by other specializations");

    Optional<T> operator()(APIInstance &, const vm::Value &value) const
    {
        static_assert(!IsDynArray<T>::value, "other specialization should handle dynarrays");

        vm::VMObject *object = nullptr;

        if (!value.GetPointer<vm::VMObject>(&object)) {
            return { };
        }
        
        vm::Member *member;

        T *t_ptr = nullptr;

        if ((member = object->LookupMemberFromHash(hash_fnv_1("__intern"))) && member->value.GetPointer<T>(&t_ptr)) {
            return *t_ptr;
        }

        return { };
    }
};

template <>
struct ConvertImpl<Int32>
{
    Optional<Int32> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::Number num { };

        if (value.GetSignedOrUnsigned(&num)) {
            return static_cast<Int32>((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return { };
    }
};

template <>
struct ConvertImpl<Int64>
{
    Optional<Int64> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::Number num { };

        if (value.GetSignedOrUnsigned(&num)) {
            return static_cast<Int64>((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return { };
    }
};

template <>
struct ConvertImpl<UInt32>
{
    Optional<UInt32> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::Number num { };

        if (value.GetSignedOrUnsigned(&num)) {
            return static_cast<UInt32>((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return { };
    }
};

template <>
struct ConvertImpl<UInt64>
{
    Optional<UInt64> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::Number num { };

        if (value.GetSignedOrUnsigned(&num)) {
            return static_cast<UInt64>((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return { };
    }
};

template <>
struct ConvertImpl<Float>
{
    Optional<Float> operator()(APIInstance &, const vm::Value &value) const
    {
        Double dbl;

        if (value.GetFloatingPointCoerce(&dbl)) {
            return static_cast<Float>(dbl);
        }

        return { };
    }
};

template <>
struct ConvertImpl<Double>
{
    Optional<Double> operator()(APIInstance &, const vm::Value &value) const
    {
        Double dbl;

        if (value.GetFloatingPointCoerce(&dbl)) {
            return dbl;
        }

        return { };
    }
};

template <>
struct ConvertImpl<Bool>
{
    Optional<Bool> operator()(APIInstance &, const vm::Value &value) const
    {
        Bool b;

        if (value.GetBoolean(&b)) {
            return b;
        }

        return { };
    }
};

template <>
struct ConvertImpl<String>
{
    Optional<String> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMString *str = nullptr;

        if (value.GetPointer<vm::VMString>(&str)) {
            return String(str->GetData());
        }

        return { };
    }
};


template <>
struct ConvertImpl<void *>
{
    Optional<void *> operator()(APIInstance &, const vm::Value &value) const
    {
        if (value.m_type == vm::Value::ValueType::HEAP_POINTER) {
            return GetRawPointerForHeapValue(value.m_value.ptr);
        }

        if (value.m_type == vm::Value::ValueType::USER_DATA) {
            return value.m_value.user_data;
        }

        return { };
    }
};

template <>
struct ConvertImpl<vm::VMString *>
{
    Optional<vm::VMString *> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMString *ptr;

        if (value.GetPointer<vm::VMString>(&ptr)) {
            return ptr;
        }

        return { };
    }
};

template <>
struct ConvertImpl<vm::VMObject *>
{
    Optional<vm::VMObject *> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMObject *ptr;

        if (value.GetPointer<vm::VMObject>(&ptr)) {
            return ptr;
        }

        return { };
    }
};

template <>
struct ConvertImpl<vm::VMArray *>
{
    Optional<vm::VMArray *> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMArray *ptr;

        if (value.GetPointer<vm::VMArray>(&ptr)) {
            return ptr;
        }

        return { };
    }
};

template <>
struct ConvertImpl<vm::VMStruct *>
{
    Optional<vm::VMStruct *> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMStruct *ptr;

        if (value.GetPointer<vm::VMStruct>(&ptr)) {
            return ptr;
        }

        return { };
    }
};

template <>
struct ConvertImpl<vm::VMString>
{
    vm::VMString *operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMString *ptr;

        if (value.GetPointer<vm::VMString>(&ptr)) {
            return ptr;
        }

        return nullptr;
    }
};

template <>
struct ConvertImpl<vm::VMObject>
{
    vm::VMObject *operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMObject *ptr;

        if (value.GetPointer<vm::VMObject>(&ptr)) {
            return ptr;
        }

        return nullptr;
    }
};

template <>
struct ConvertImpl<vm::VMArray>
{
    vm::VMArray *operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMArray *ptr;

        if (value.GetPointer<vm::VMArray>(&ptr)) {
            return ptr;
        }

        return nullptr;
    }
};

template <>
struct ConvertImpl<vm::VMStruct>
{
    vm::VMStruct *operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMStruct *ptr;

        if (value.GetPointer<vm::VMStruct>(&ptr)) {
            return ptr;
        }

        return nullptr;
    }
};

template <class T>
struct ConvertImpl<T *> // embedded C++ object (as pointer)
{
    static_assert(!is_vm_object_type<NormalizedType<std::remove_pointer_t<T>>>, "Should not receive a VM object type as it is handled by other specializations");

    Optional<T *> operator()(APIInstance &, const vm::Value &value) const
    {
        vm::VMObject *object = nullptr;

        if (!value.GetPointer<vm::VMObject>(&object)) {
            return { };
        }
        
        vm::Member *member;

        T *t_ptr = nullptr;

        if ((member = object->LookupMemberFromHash(hash_fnv_1("__intern"))) && member->value.GetPointer<T>(&t_ptr)) {
            return t_ptr;
        }

        return { };
    }
};

template <class T, SizeType NumInlineBytes>
struct ConvertImpl<Array<T, NumInlineBytes>>
{
    Optional<Array<T, NumInlineBytes>> operator()(APIInstance &api_instance, const vm::Value &value) const
    {
        vm::VMArray *ary = nullptr;

        if (value.GetPointer<vm::VMArray>(&ary)) {
            Array<T, NumInlineBytes> result;

            result.Resize(ary->GetSize());

            for (SizeType index = 0; index < ary->GetSize(); index++) {
                ConvertImpl<T> element_convert_impl;
                
                auto element_result = element_convert_impl(api_instance, ary->AtIndex(index));

                if (!element_result.HasValue()) {
                    return { };
                }

                result[index] = std::move(element_result.Get());
            }

            return Optional<Array<T, NumInlineBytes>>(std::move(result));
        }

        return { };
    }
};

/*template <class T>
static inline auto ConvertScriptObject(APIInstance &api_instance, const vm::Value &value, T &&out) -> std::enable_if_t<std::is_class_v<NormalizedType<std::remove_pointer_t<T>>> && !(is_vm_object_type<NormalizedType<std::remove_pointer_t<T>>>), bool>
{
    ConvertImpl<ScriptClassWrapper<NormalizedType<std::remove_pointer_t<T>>>> impl;

    return impl(api_instance, value, out);
}*/

#pragma endregion

#pragma region Get Argument Helpers

template <class T>
struct GetArgumentImpl
{
    //static_assert(resolution_failure<T>, "Unable to use type as arg");
    
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, T, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<Int32>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_INT_1(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<Int64>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_INT_1(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<UInt32>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_UINT_1(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<UInt64>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_UINT_1(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<Float>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_FLOAT_1(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<Double>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_FLOAT_1(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<bool>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_BOOLEAN_1(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<void *>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR_RAW_1(index, void, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMString>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMString, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMObject>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMObject, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMArray>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMArray, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMStruct>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMStruct, arg_value);

        return arg_value;
    }
};

#if 0

template <int index, class ReturnType, class Ty = std::conditional_t<std::is_class_v<ReturnType>, std::add_pointer_t<ReturnType>, ReturnType>>
typename std::enable_if_t<
    !std::is_class_v<NormalizedType<ReturnType>> || is_vm_object_type<ReturnType>,
    Ty
>
GetArgument(sdk::Params &params)
{
    static_assert(!std::is_same_v<void, ReturnType>);
    
    //return GetArgumentImpl<ReturnType>()(index, params);
    

    vm::Value *value = params.args[index];
    
    auto converted = ConvertScriptObject<Ty>(params.api_instance, *value);

    if (!converted.HasValue()) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Unexpected type for argument, could not convert to expected type"));

        return { };
    }
    
    return converted.Get();
}

template <int index, class ReturnType, class Ty = NormalizedType<ReturnType>>
typename std::enable_if_t<
    std::is_class_v<NormalizedType<ReturnType>> && !(is_vm_object_type<NormalizedType<ReturnType>>),
    Ty &
>
GetArgument(sdk::Params &params)
{
    //HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMObject, arg0);
    //HYP_SCRIPT_GET_MEMBER_PTR(arg0, "__intern", NormalizedType<ReturnType>, member);
    //return *member;
    
    //return GetArgumentImpl<ReturnType>()(index, params);

    vm::Value *value = params.args[index];
    
    auto converted = ConvertScriptObject<Ty *>(params.api_instance, *value);

    if (!converted.HasValue()) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Unexpected type for argument, could not convert to expected type"));
        
        AssertThrowMsg(false, "Re-implement this in a secure way!");
    }
    
    return *converted.Get();
}

#else

template <int Index, class T>
auto GetArgument(sdk::Params &params)
{
    vm::Value *value = params.args[Index];
    
    auto converted = ConvertScriptObject<T>(params.api_instance, *value);

    if (!converted.HasValue()) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Unexpected type for argument, could not convert to expected type"));

        return T { };
    }
    
    return converted.Get();
}

#endif

#pragma endregion



#pragma region Native Script Binding Helper Structs

struct ScriptBindingsBase;

struct ScriptBindingsHolder
{
    static constexpr UInt max_bindings = 256;

    FixedArray<ScriptBindingsBase *, max_bindings> bindings;
    UInt binding_index = 0;

    void AddBinding(ScriptBindingsBase *script_bindings);

    void GenerateAll(APIInstance &api_instance);
};

struct ScriptBindingsBase
{
    ScriptBindingsBase(TypeID type_id);
    virtual ~ScriptBindingsBase() = default;

    virtual void Generate(APIInstance &) = 0;
};

#pragma endregion

}

#endif
