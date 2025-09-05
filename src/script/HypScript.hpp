#pragma once

#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>

#include <script/vm/Value.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {

class HypScript;
class Script_Interpreter;
class Script_SymbolTable;
class InstructionStream;

struct Script_Instance;

#define HYP_DEF_SCRIPT_API_HANDLE(handleTypeName, handleTypeNameCaps, underlyingType) \
    enum class handleTypeName : underlyingType;                                       \
    constexpr handleTypeName INVALID_##handleTypeNameCaps = handleTypeName(0);

HYP_DEF_SCRIPT_API_HANDLE(Script_FunctionHandle, FUNCTION, uintptr_t)
HYP_DEF_SCRIPT_API_HANDLE(Script_ObjectHandle, OBJECT, uintptr_t)

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

    Script_Interpreter* GetVM() const
    {
        return m_vm;
    }

    void Initialize();

    void DestroyScript(Script_Instance* instance);

    Script_Instance* Compile(SourceFile& sourceFile, ErrorList& outErrorList);
    InstructionStream* Decompile(Script_Instance* instance, std::ostream* os = nullptr) const;

    void Run(Script_Instance* instance);

    template <class T>
    static inline Script_Value CreateArgument(T&& item)
    {
        return Script_Value(HypData(std::forward<T>(item)));
    }

    template <class... Args>
    static inline auto CreateArguments(Args&&... args) -> FixedArray<Script_Value, sizeof...(Args)>
    {
        return FixedArray<Script_Value, sizeof...(Args)> { CreateArgument(args)... };
    }

    void CallFunctionArgV(Script_Instance* instance, Script_FunctionHandle functionHandle, Script_Value* args, ArgCount numArgs);

    bool GetFunctionHandle(const char* name, Script_FunctionHandle& outFunctionHandle);
    bool GetObjectHandle(const char* name, Script_ObjectHandle& outObjectHandle);

    bool GetExportedValue(const char* name, Script_Value*& outValue);

    Script_SymbolTable& GetExportedSymbols() const;

    bool GetMember(Script_ObjectHandle objectHandle, const char* memberName, Script_Value*& outValue);
    bool SetMember(Script_ObjectHandle objectHandle, const char* memberName, Script_Value&& value);

    template <class... Args>
    void CallFunction(Script_Instance* instance, Script_FunctionHandle functionHandle, Args&&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        CallFunctionArgV(instance, functionHandle, arguments.Data(), arguments.Size());
    }

    void ReadLastReturnValue(Script_Instance* instance, Script_Value& outValue);

private:
    Script_Interpreter* m_vm;

    // global cached data used from native code
    mutable Mutex m_mutex;
};

} // namespace hyperion
