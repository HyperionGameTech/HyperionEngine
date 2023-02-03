#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include <script/ScriptApi.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VM.hpp>

#include <core/Base.hpp>
#include <core/lib/FixedArray.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <util/UTF8.hpp>

#include <Types.hpp>

#include <memory>

namespace hyperion {

using namespace compiler;
using namespace vm;

namespace v2 {

class Engine;

class Script : public EngineComponentBase<STUB_CLASS(Script)>
{
public:
    using Bytes = Array<UByte>;

    using ArgCount = UInt16;

    struct ValueHandle
    {
        Value _inner = Value(
            Value::NONE,
            Value::ValueData {
                .user_data = nullptr
            }
        );
    };

    struct ObjectHandle : ValueHandle { };
    struct FunctionHandle : ValueHandle { };

    Script(const SourceFile &source_file);
    Script(const Script &other) = delete;
    Script &operator=(const Script &other) = delete;
    ~Script();

    APIInstance &GetAPIInstance()
        { return m_api_instance; }

    const APIInstance &GetAPIInstance() const
        { return m_api_instance; }

    const SourceFile &GetSourceFile() const
        { return m_source_file; }

    void Init();

    const ErrorList &GetErrors() const { return m_errors; }

    ExportedSymbolTable &GetExportedSymbols() { return m_vm.GetState().GetExportedSymbols(); }
    const ExportedSymbolTable &GetExportedSymbols() const { return m_vm.GetState().GetExportedSymbols(); }

    VM &GetVM() { return m_vm; }
    const VM &GetVM() const { return m_vm; }

    bool IsBaked() const { return m_baked_bytes.Any(); }
    bool IsCompiled() const { return m_bytecode_chunk.buildables.Any(); }

    bool Compile();

    InstructionStream Decompile(utf::utf8_ostream *os = nullptr) const;

    void Bake();
    void Bake(BuildParams &build_params);

    void Run();

    template <class T>
    constexpr Value CreateArgument(T &&item)
    {
        using ArgType = std::remove_cv_t<std::decay_t<T>>;

        Value first_value;

        if constexpr (std::is_same_v<Value, ArgType>) {
            first_value = item;
        } else if constexpr (std::is_base_of_v<ValueHandle, ArgType>) {
            first_value = item._inner;
        } else if constexpr (std::is_same_v<int32_t, ArgType>) {
            first_value.m_type = Value::I32;
            first_value.m_value.i32 = item;
        } else if constexpr (std::is_same_v<int64_t, ArgType>) {
            first_value.m_type = Value::I64;
            first_value.m_value.i64 = item;
        } else if constexpr (std::is_same_v<uint32_t, ArgType>) {
            first_value.m_type = Value::U32;
            first_value.m_value.u32 = item;
        } else if constexpr (std::is_same_v<uint64_t, ArgType>) {
            first_value.m_type = Value::U64;
            first_value.m_value.u64 = item;
        } else if constexpr (std::is_same_v<float, ArgType>) {
            first_value.m_type = Value::F32;
            first_value.m_value.f = item;
        } else if constexpr (std::is_same_v<double, ArgType>) {
            first_value.m_type = Value::F64;
            first_value.m_value.d = item;
        } else if constexpr (std::is_same_v<bool, ArgType>) {
            first_value.m_type = Value::BOOLEAN;
            first_value.m_value.b = item;
        } else if constexpr (std::is_pointer_v<ArgType>) {
            // set to userdata
            first_value.m_type = Value::USER_DATA;
            first_value.m_value.user_data = static_cast<void *>(item);
        } else {
            static_assert(resolution_failure<ArgType>, "Cannot pass value type to script function!");
        }

        return first_value;
    }

    template <class ...Args>
    auto CreateArguments(Args &&... args) -> FixedArray<Value, sizeof...(Args)>
    {
        return FixedArray<Value, sizeof...(Args)> {
            CreateArgument(args)...
        };
    }

    void CallFunctionArgV(const FunctionHandle &handle, Value *args, ArgCount num_args);

    bool GetFunctionHandle(const char *name, FunctionHandle &out_handle)
    {
        return GetExportedValue(name, &out_handle._inner);
    }

    bool GetObjectHandle(const char *name, ObjectHandle &out_handle)
    {
        return GetExportedValue(name, &out_handle._inner);
    }

    bool GetExportedValue(const char *name, Value *value)
    {
        return GetExportedSymbols().Find(hash_fnv_1(name), value);
    }

    bool GetMember(const ObjectHandle &object, const char *member_name, ValueHandle &out_value)
    {
        if (object._inner.m_type != Value::HEAP_POINTER) {
            return false;
        }

        if (VMObject *ptr = object._inner.m_value.ptr->GetPointer<VMObject>()) {
            if (Member *member = ptr->LookupMemberFromHash(hash_fnv_1(member_name))) {
                out_value = ValueHandle { member->value };

                return true;
            }
        }

        return false;
    }

    bool SetMember(const ObjectHandle &object, const char *member_name, const Value &value)
    {
        if (object._inner.m_type != Value::HEAP_POINTER) {
            return false;
        }

        if (VMObject *ptr = object._inner.m_value.ptr->GetPointer<VMObject>()) {
            if (Member *member = ptr->LookupMemberFromHash(hash_fnv_1(member_name))) {
                member->value = value;

                return true;
            }
        }

        return false;
    }

    template <class ...Args>
    void CallFunction(const FunctionHandle &handle, Args &&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        CallFunctionArgV(handle, arguments.Data(), arguments.Size());
    }

    template <class RegisteredType, class T>
    ValueHandle CreateInternedObject(const T &value)
    {
        const auto class_name_it = m_api_instance.class_bindings.class_names.Find<RegisteredType>();
        AssertThrowMsg(class_name_it != m_api_instance.class_bindings.class_names.End(), "Class not registered!");

        const auto prototype_it = m_api_instance.class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != m_api_instance.class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Value intern_value;
        {
            vm::HeapValue *ptr_result = m_vm.GetState().HeapAlloc(m_vm.GetState().GetMainThread());
            AssertThrow(ptr_result != nullptr);

            ptr_result->Assign(value);
            ptr_result->Mark();
            intern_value = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptr_result });
        }

        vm::VMObject boxed_value(prototype_it->second);
        HYP_SCRIPT_SET_MEMBER(boxed_value, "__intern", intern_value);

        vm::Value final_value;
        {
            vm::HeapValue *ptr_result = m_vm.GetState().HeapAlloc(m_vm.GetState().GetMainThread());
            AssertThrow(ptr_result != nullptr);

            ptr_result->Assign(boxed_value);
            ptr_result->Mark();
            final_value = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptr_result });
        }

        return { final_value };
    }

private:

    APIInstance m_api_instance;

    SourceFile  m_source_file;
    CompilationUnit m_compilation_unit;
    ErrorList m_errors;

    BytecodeChunk m_bytecode_chunk;

    Bytes m_baked_bytes;

    VM m_vm;
    BytecodeStream m_bs;
};

} // namespace v2

} // namespace hyperion

#endif
