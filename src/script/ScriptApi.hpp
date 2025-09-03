#pragma once

#include <script/vm/VMArray.hpp>
#include <script/vm/VMMap.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/Value.hpp>

#include <script/compiler/Configuration.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/ScriptApi2.hpp>

#include <core/utilities/Variant.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/Optional.hpp>
#include <core/memory/Any.hpp>
#include <core/Util.hpp>

#include <core/Types.hpp>

#include <core/Defines.hpp>

#ifndef __cplusplus
#error Script requires a C++ compiler
#endif

#pragma region Script helper macros

#define HYP_SCRIPT_PARAMS hyperion::sdk::Params

#define HYP_SCRIPT_EXPORT_FUNCTION(name) HYP_EXPORT void name(hyperion::sdk::Params params)

#define HYP_SCRIPT_FUNCTION(name) void name(hyperion::sdk::Params params)

#define HYP_SCRIPT_CHECK_ARGS(cmp, amt)                                                      \
    do                                                                                       \
    {                                                                                        \
        if (!(params.nargs cmp amt))                                                         \
        {                                                                                    \
            params.throwException(params.ctx,                                                \
                hyperion::vm::Exception::InvalidArgsException(#cmp " " #amt, params.nargs)); \
            return;                                                                          \
        }                                                                                    \
    }                                                                                        \
    while (false)

#define HYP_SCRIPT_THROW(exception) \
    do                              \
    {                               \
        params.throwException(      \
            params.ctx,             \
            exception);             \
        return;                     \
    }                               \
    while (0)

#define HYP_SCRIPT_RETURN(value)                             \
    do                                                       \
    {                                                        \
        params.setReturnValue(params.ctx, std::move(value)); \
        return;                                              \
    }                                                        \
    while (false)

#define HYP_SCRIPT_RETURN_PTR(value)    \
    do                                  \
    {                                   \
        Assert(value != nullptr);       \
        value->Mark();                  \
        vm::Value _res(HypData(value)); \
        HYP_SCRIPT_RETURN(_res);        \
    }                                   \
    while (false)

#define HYP_SCRIPT_RETURN_VOID(value) \
    do                                \
    {                                 \
        (void)(value);                \
        vm::Value _res;               \
        HYP_SCRIPT_RETURN(_res);      \
    }                                 \
    while (false)

#define HYP_SCRIPT_RETURN_INT32(value)         \
    do                                         \
    {                                          \
        vm::Value _res(HypData((int32)value)); \
        HYP_SCRIPT_RETURN(_res);               \
    }                                          \
    while (false)

#define HYP_SCRIPT_RETURN_INT64(value)         \
    do                                         \
    {                                          \
        vm::Value _res(HypData((int64)value)); \
        HYP_SCRIPT_RETURN(_res);               \
    }                                          \
    while (false)

#define HYP_SCRIPT_RETURN_UINT32(value)         \
    do                                          \
    {                                           \
        vm::Value _res(HypData((uint32)value)); \
        HYP_SCRIPT_RETURN(_res);                \
    }                                           \
    while (false)

#define HYP_SCRIPT_RETURN_UINT64(value)         \
    do                                          \
    {                                           \
        vm::Value _res(HypData((uint64)value)); \
        HYP_SCRIPT_RETURN(_res);                \
    }                                           \
    while (false)

#define HYP_SCRIPT_RETURN_FLOAT32(value)       \
    do                                         \
    {                                          \
        vm::Value _res(HypData((float)value)); \
        HYP_SCRIPT_RETURN(_res);               \
    }                                          \
    while (false)

#define HYP_SCRIPT_RETURN_FLOAT64(value)        \
    do                                          \
    {                                           \
        vm::Value _res(HypData((double)value)); \
        HYP_SCRIPT_RETURN(_res);                \
    }                                           \
    while (false)

#define HYP_SCRIPT_RETURN_BOOLEAN(value)      \
    do                                        \
    {                                         \
        vm::Value _res(HypData((bool)value)); \
        HYP_SCRIPT_RETURN(_res);              \
    }                                         \
    while (false)

#define HYP_SCRIPT_GET_ARG_INT(index, name)                                                                                                                  \
    int64 name;                                                                                                                                              \
    do                                                                                                                                                       \
    {                                                                                                                                                        \
        vm::Number num;                                                                                                                                      \
        if (!params.args[index]->GetSignedOrUnsigned(&num))                                                                                                  \
        {                                                                                                                                                    \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type int or uint")); \
            return (decltype(name))0;                                                                                                                        \
        }                                                                                                                                                    \
        name = (num.flags & vm::Number::FLAG_UNSIGNED) ? static_cast<int64>(num.u) : num.i;                                                                  \
    }                                                                                                                                                        \
    while (false)

#define HYP_SCRIPT_GET_ARG_UINT(index, name)                                                                                                                 \
    uint64 name;                                                                                                                                             \
    do                                                                                                                                                       \
    {                                                                                                                                                        \
        vm::Number num;                                                                                                                                      \
        if (!params.args[index]->GetSignedOrUnsigned(&num))                                                                                                  \
        {                                                                                                                                                    \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type int or uint")); \
            return (decltype(name))0;                                                                                                                        \
        }                                                                                                                                                    \
        name = (num.flags & vm::Number::FLAG_SIGNED) ? static_cast<uint64>(num.i) : num.u;                                                                   \
    }                                                                                                                                                        \
    while (false)

#define HYP_SCRIPT_GET_ARG_BOOLEAN(index, name)                                                                                                       \
    bool name;                                                                                                                                        \
    do                                                                                                                                                \
    {                                                                                                                                                 \
        if (!params.args[index]->GetBoolean(&name))                                                                                                   \
        {                                                                                                                                             \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type bool")); \
            return (decltype(name))0;                                                                                                                 \
        }                                                                                                                                             \
    }                                                                                                                                                 \
    while (false)

#define HYP_SCRIPT_GET_ARG_FLOAT(index, name)                                                                                                          \
    double name;                                                                                                                                       \
    do                                                                                                                                                 \
    {                                                                                                                                                  \
        if (!params.args[index]->GetFloatingPointCoerce(&name))                                                                                        \
        {                                                                                                                                              \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type float")); \
            return (decltype(name))0;                                                                                                                  \
        }                                                                                                                                              \
    }                                                                                                                                                  \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR(index, type, name)                                                                                                       \
    type* name;                                                                                                                                         \
    do                                                                                                                                                  \
    {                                                                                                                                                   \
        if (!params.args[index]->GetPointer<type>(&name))                                                                                               \
        {                                                                                                                                               \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type " #type)); \
        }                                                                                                                                               \
    }                                                                                                                                                   \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR_RAW(index, type, name)                                                                                                   \
    type* name;                                                                                                                                         \
    do                                                                                                                                                  \
    {                                                                                                                                                   \
        if (!params.args[index]->GetUserData<type>(&name))                                                                                              \
        {                                                                                                                                               \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type " #type)); \
            return (decltype(name))nullptr;                                                                                                             \
        }                                                                                                                                               \
    }                                                                                                                                                   \
    while (false)

#define HYP_SCRIPT_GET_ARG_INT_1(index, name)                                                                                                    \
    int64 name;                                                                                                                                  \
    do                                                                                                                                           \
    {                                                                                                                                            \
        auto _value = ConvertScriptObject<int64>(params.apiInstance, *params.args[index]);                                                       \
        if (!_value.HasValue())                                                                                                                  \
        {                                                                                                                                        \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be numeric")); \
            return (decltype(name))0;                                                                                                            \
        }                                                                                                                                        \
        name = _value.Get();                                                                                                                     \
    }                                                                                                                                            \
    while (false)

#define HYP_SCRIPT_GET_ARG_UINT_1(index, name)                                                                                                  \
    uint64 name;                                                                                                                                \
    do                                                                                                                                          \
    {                                                                                                                                           \
        auto _value = ConvertScriptObject<uint64>(params.apiInstance, *params.args[index]);                                                     \
        if (!_value.HasValue())                                                                                                                 \
        {                                                                                                                                       \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be number")); \
            return (decltype(name))0;                                                                                                           \
        }                                                                                                                                       \
        name = _value.Get();                                                                                                                    \
    }                                                                                                                                           \
    while (false)

#define HYP_SCRIPT_GET_ARG_BOOLEAN_1(index, name)                                                                                                     \
    bool name;                                                                                                                                        \
    do                                                                                                                                                \
    {                                                                                                                                                 \
        auto _value = ConvertScriptObject<bool>(params.apiInstance, *params.args[index]);                                                             \
        if (!_value.HasValue())                                                                                                                       \
        {                                                                                                                                             \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type bool")); \
            return (decltype(name))0;                                                                                                                 \
        }                                                                                                                                             \
        name = _value.Get();                                                                                                                          \
    }                                                                                                                                                 \
    while (false)

#define HYP_SCRIPT_GET_ARG_FLOAT_1(index, name)                                                                                                  \
    double name;                                                                                                                                 \
    do                                                                                                                                           \
    {                                                                                                                                            \
        auto _value = ConvertScriptObject<double>(params.apiInstance, *params.args[index]);                                                      \
        if (!_value.HasValue())                                                                                                                  \
        {                                                                                                                                        \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be numeric")); \
            return (decltype(name))0;                                                                                                            \
        }                                                                                                                                        \
        name = _value.Get();                                                                                                                     \
    }                                                                                                                                            \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR_1(index, type, name)                                                                                                               \
    type* name;                                                                                                                                                   \
    do                                                                                                                                                            \
    {                                                                                                                                                             \
        auto _value = ConvertScriptObject<type*>(params.apiInstance, *params.args[index]);                                                                        \
        if (!_value.HasValue())                                                                                                                                   \
        {                                                                                                                                                         \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be a pointer of type " #type)); \
        }                                                                                                                                                         \
        name = _value.Get();                                                                                                                                      \
    }                                                                                                                                                             \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR_RAW_1(index, type, name)                                                                                                    \
    type* name;                                                                                                                                            \
    do                                                                                                                                                     \
    {                                                                                                                                                      \
        auto _value = ConvertScriptObject<void*>(params.apiInstance, *params.args[index]);                                                                 \
        if (!_value.HasValue())                                                                                                                            \
        {                                                                                                                                                  \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be userdata (void *)")); \
            return (decltype(name))0;                                                                                                                      \
        }                                                                                                                                                  \
        name = static_cast<type*>(_value.Get());                                                                                                           \
    }                                                                                                                                                      \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_INT(object, name, type, declName)                                                                                 \
    type declName;                                                                                                                              \
    do                                                                                                                                          \
    {                                                                                                                                           \
        vm::Number num;                                                                                                                         \
        vm::Member* _member = nullptr;                                                                                                          \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetSignedOrUnsigned(&num))                             \
        {                                                                                                                                       \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type int or uint")); \
            return;                                                                                                                             \
        }                                                                                                                                       \
        declName = (num.flags & vm::Number::FLAG_UNSIGNED) ? static_cast<int64>(num.u) : num.i;                                                 \
    }                                                                                                                                           \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_UINT(object, name, type, declName)                                                                         \
    type declName;                                                                                                                       \
    do                                                                                                                                   \
    {                                                                                                                                    \
        uint64 num;                                                                                                                      \
        vm::Member* _member = nullptr;                                                                                                   \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetUnsigned(&num))                              \
        {                                                                                                                                \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type uint")); \
            return;                                                                                                                      \
        }                                                                                                                                \
        declName = static_cast<type>(num);                                                                                               \
    }                                                                                                                                    \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_FLOAT(object, name, type, declName)                                                                         \
    type declName;                                                                                                                        \
    do                                                                                                                                    \
    {                                                                                                                                     \
        double num;                                                                                                                       \
        vm::Member* _member = nullptr;                                                                                                    \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetFloatingPointCoerce(&num))                    \
        {                                                                                                                                 \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type float")); \
            return;                                                                                                                       \
        }                                                                                                                                 \
        declName = static_cast<type>(num);                                                                                                \
    }                                                                                                                                     \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_PTR(object, name, type, declName)                                                                            \
    type* declName;                                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        vm::Member* _member = nullptr;                                                                                                     \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetPointer<type>(&declName))                      \
        {                                                                                                                                  \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type " #type)); \
        }                                                                                                                                  \
    }                                                                                                                                      \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_PTR_RAW(object, name, type, declName)                                                                        \
    type* declName;                                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        vm::Member* _member = nullptr;                                                                                                     \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetUserData<type>(&declName))                     \
        {                                                                                                                                  \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type " #type)); \
            return;                                                                                                                        \
        }                                                                                                                                  \
    }                                                                                                                                      \
    while (false)

#define HYP_SCRIPT_SET_MEMBER(object, nameStr, assignment)                                                  \
    do                                                                                                      \
    {                                                                                                       \
        auto* member = object.LookupMemberFromHash(hashFnv1(nameStr));                                      \
        Assert(member != nullptr, "Member " nameStr " not set on object, unmatching prototype definition"); \
        member->value = assignment;                                                                         \
    }                                                                                                       \
    while (false)

#pragma endregion

namespace hyperion {

namespace vm {
// forward declarations
class VM;
// struct Value;
class InstructionHandler;
class VMString;

} // namespace vm

using namespace compiler;

class APIInstance;

#pragma region Script API Instance

class APIInstance
{
public:
    struct ClassBindings
    {
        TypeMap<String> classNames;
        HashMap<String, vm::VMObject*> classPrototypes;
    } classBindings;

    APIInstance();
    APIInstance(const APIInstance& other) = delete;
    ~APIInstance() = default;

    vm::VM* GetVM() const
    {
        return m_vm;
    }

    void SetVM(vm::VM* vm)
    {
        m_vm = vm;
    }

private:
    vm::VM* m_vm;
};

#pragma endregion

#pragma region Native Script Binding Helper Structs

struct ScriptBindingsBase;
struct ScriptBindingsHolder;

extern ScriptBindingsHolder g_scriptBindings;

struct ScriptBindingsHolder
{
    static constexpr uint32 maxBindings = 256;

    FixedArray<ScriptBindingsBase*, maxBindings> bindings;
    uint32 bindingIndex = 0;

    void AddBinding(ScriptBindingsBase* scriptBindings);

    void GenerateAll(scriptapi2::Context&);
};

struct ScriptBindingsBase
{
    ScriptBindingsBase(TypeId typeId);
    virtual ~ScriptBindingsBase() = default;

    virtual void Generate(scriptapi2::Context&) = 0;
};

#pragma endregion

} // namespace hyperion
