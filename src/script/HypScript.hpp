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
    enum class handleTypeName##Handle : underlyingType;                               \
    constexpr handleTypeName##Handle INVALID_##handleTypeNameCaps = handleTypeName##Handle(0);

HYP_DEF_SCRIPT_API_HANDLE(Script, SCRIPT, uint32)
HYP_DEF_SCRIPT_API_HANDLE(Function, FUNCTION, uintptr_t)
HYP_DEF_SCRIPT_API_HANDLE(Object, OBJECT, uintptr_t)

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

    void ReadLastReturnValue(Value& outValue);

private:
    scriptapi2::Context m_context;
    APIInstance m_apiInstance;
    VM* m_vm;

    // global cached data used from native code
    mutable Mutex m_mutex;
    HashMap<ScriptHandle, struct ScriptHandleData*> m_scripts;
};

} // namespace hyperion
