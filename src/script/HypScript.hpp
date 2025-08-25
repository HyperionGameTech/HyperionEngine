#pragma once

#include <script/ScriptApi.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/vm/BytecodeStream.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/Util.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <util/UTF8.hpp>

namespace hyperion {

using namespace compiler;
using namespace vm;

namespace scriptapi2 {
class Context;
} // namespace scriptapi2

class HypScript;

#define HYP_DEF_SCRIPT_API_HANDLE(handleTypeName, handleTypeNameCaps, underlyingType) \
    enum class handleTypeName##Handle : underlyingType; \
    constexpr handleTypeName##Handle INVALID_##handleTypeNameCaps = handleTypeName##Handle(0);

HYP_DEF_SCRIPT_API_HANDLE(Script, SCRIPT, uint32)
HYP_DEF_SCRIPT_API_HANDLE(Function, FUNCTION, uint64)
HYP_DEF_SCRIPT_API_HANDLE(Object, OBJECT, uint64)

#undef HYP_DEF_SCRIPT_API_HANDLE

class HypScript
{

public:
    using ArgCount = uint16;

    static HypScript& GetInstance();

    HypScript();
    HypScript(const HypScript& other) = delete;
    HypScript& operator=(const HypScript& other) = delete;
    ~HypScript();

    APIInstance& GetAPIInstance()
    {
        return m_apiInstance;
    }

    const APIInstance& GetAPIInstance() const
    {
        return m_apiInstance;
    }

    VM* GetVM() const
    {
        return m_vm;
    }

    void Initialize();

    void DestroyScript(ScriptHandle scriptHandle);

    ScriptHandle Compile(
        SourceFile& sourceFile,
        ErrorList& outErrorList);

    InstructionStream Decompile(
        ScriptHandle scriptHandle,
        std::ostream* os = nullptr) const;

    void Run(ScriptHandle scriptHandle);

    template <class T>
    static inline Value CreateArgument(T&& item)
    {
        return Value(HypData(std::forward<T>(item)));
    }

    template <class... Args>
    static inline auto CreateArguments(Args&&... args) -> FixedArray<Value, sizeof...(Args)>
    {
        return FixedArray<Value, sizeof...(Args)> {
            CreateArgument(args)...
        };
    }

    void CallFunctionArgV(ScriptHandle scriptHandle, FunctionHandle functionHandle, Value* args, ArgCount numArgs);

    bool GetFunctionHandle(const char* name, FunctionHandle& outFunctionHandle);
    bool GetObjectHandle(const char* name, ObjectHandle& outObjectHandle);

    bool GetExportedValue(const char* name, Value*& outValue);

    ExportedSymbolTable& GetExportedSymbols() const;

    bool GetMember(ObjectHandle objectHandle, const char* memberName, Value*& outValue);
    bool SetMember(ObjectHandle objectHandle, const char* memberName, Value&& value);

    template <class... Args>
    void CallFunction(ScriptHandle scriptHandle, FunctionHandle functionHandle, Args&&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        CallFunctionArgV(scriptHandle, functionHandle, arguments.Data(), arguments.Size());
    }
    
    void ReadLastReturnValue(Value*& outValue);

#if 0
    template <class RegisteredType, class T>
    ValueHandle CreateInternedObject(const T& value)
    {
        const auto classNameIt = m_apiInstance.classBindings.classNames.Find<RegisteredType>();
        Assert(classNameIt != m_apiInstance.classBindings.classNames.End(), "Class %s not registered!", TypeName<RegisteredType>().Data());

        const auto prototypeIt = m_apiInstance.classBindings.classPrototypes.Find(classNameIt->second);
        Assert(prototypeIt != m_apiInstance.classBindings.classPrototypes.End(), "Class %s not registered!", TypeName<RegisteredType>().Data());

        vm::Value internValue;
        {
            vm::HeapValue* ptrResult = m_vm.GetState().HeapAlloc(m_vm.GetState().GetMainThread());
            Assert(ptrResult != nullptr);

            ptrResult->Assign(value);
            ptrResult->Mark();
            internValue = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptrResult });
        }

        vm::VMObject boxedValue(prototypeIt->second);
        HYP_SCRIPT_SET_MEMBER(boxedValue, "__intern", internValue);

        vm::Value finalValue;
        {
            vm::HeapValue* ptrResult = m_vm.GetState().HeapAlloc(m_vm.GetState().GetMainThread());
            Assert(ptrResult != nullptr);

            ptrResult->Assign(boxedValue);
            ptrResult->Mark();
            finalValue = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptrResult });
        }

        return { finalValue };
    }
#endif

private:
    scriptapi2::Context m_context;
    APIInstance m_apiInstance;
    VM* m_vm;

    // global cached data used from native code
    mutable Mutex m_mutex;
    HashMap<ScriptHandle, struct ScriptHandleData*> m_scripts;
};

} // namespace hyperion
